/**
 * @file LayoutSanityChecker.cpp
 * @brief Implementation of the runtime layout constraint checker
 */

#include "LayoutSanityChecker.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QMenuBar>
#include <QScrollArea>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <cmath>

// Aspect ratio beyond this threshold triggers a warning
static constexpr double kExtremeAspectRatio = 20.0;

// ---------------------------------------------------------------------------
// False-positive exclusion helpers
// ---------------------------------------------------------------------------

/// Returns true for widgets that belong to third-party framework internals
/// (e.g. the ads:: Advanced Docking System) which we do not control.
static auto isThirdPartyInternal(QWidget const * widget) -> bool {
    auto const className = QString(widget->metaObject()->className());

    // ads:: Advanced Docking System — spacers, title bars, areas, etc.
    if (className.startsWith(QStringLiteral("ads::"))) {
        return true;
    }

    // Qt combo-box popup internals (ephemeral containers)
    if (className == QStringLiteral("QComboBoxPrivateContainer") ||
        className == QStringLiteral("QComboBoxListView")) {
        return true;
    }

    return false;
}

/// Returns true for widget classes that are inherently extreme-aspect-ratio
/// by design, so the aspect-ratio check is not meaningful for them.
static auto isNaturallyExtreme(QWidget const * widget) -> bool {
    return qobject_cast<QMenuBar const *>(widget) != nullptr ||
           qobject_cast<QStatusBar const *>(widget) != nullptr;
}

/// Returns true for decorative QFrame separators (HLine / VLine) which are
/// expected to have near-zero dimension in one axis.
static auto isDecorativeFrame(QWidget const * widget) -> bool {
    auto const * frame = qobject_cast<QFrame const *>(widget);
    if (!frame) {
        return false;
    }
    auto const shape = frame->frameShape();
    return shape == QFrame::HLine || shape == QFrame::VLine;
}

/// Returns true if the widget is inside a QScrollArea whose viewport has
/// zero height (e.g. a collapsed Section). Such widgets are technically
/// "visible" in Qt's sense but are not physically viewable.
static auto isClippedByAncestor(QWidget const * widget) -> bool {
    // Walk up the parent chain. If any ancestor has height 0, the widget
    // is not physically visible on screen.
    for (auto const * ancestor = widget->parentWidget(); ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
        if (ancestor->height() == 0) {
            return true;
        }
    }
    return false;
}

// Debounce interval (ms) for the global monitor
static constexpr int kDebounceMs = 50;

// =============================================================================
// Construction
// =============================================================================

LayoutSanityChecker::LayoutSanityChecker(QObject * parent)
    : QObject(parent) {
    _debounce_timer.setSingleShot(true);
    _debounce_timer.setInterval(kDebounceMs);
    connect(&_debounce_timer, &QTimer::timeout, this, &LayoutSanityChecker::runCheck);

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

// =============================================================================
// Event Filter (global monitor mode)
// =============================================================================

bool LayoutSanityChecker::eventFilter(QObject * obj, QEvent * event) {
    if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) {
        if (obj->isWidgetType()) {
            // Any visible widget's layout change should trigger a check.
            // The debounce timer coalesces rapid cascades into a single scan.
            scheduleCheck();
        }
    }
    return QObject::eventFilter(obj, event);
}

void LayoutSanityChecker::scheduleCheck() {
    // Restart the timer — this coalesces rapid layout cascades
    _debounce_timer.start();
}

void LayoutSanityChecker::runCheck() {
    std::vector<LayoutViolation> current;
    for (QWidget * window: QApplication::topLevelWidgets()) {
        auto violations = getViolations(window);
        for (auto & v: violations) {
            current.push_back(std::move(v));
        }
    }

    // Emit only violations not present in the previous scan
    for (auto const & v: current) {
        bool const wasPrevious = std::any_of(
                _previous_violations.begin(), _previous_violations.end(),
                [&v](auto const & prev) { return prev == v; });
        if (wasPrevious) {
            continue;
        }

        switch (v.severity) {
            case LayoutViolation::Severity::Critical:
                qWarning("[Layout] CRITICAL: %s — %s (actual: %dx%d, expected: %dx%d)",
                         qPrintable(v.widgetPath),
                         qPrintable(v.description),
                         v.actual.width(), v.actual.height(),
                         v.expected.width(), v.expected.height());
                break;
            case LayoutViolation::Severity::Warning:
                qWarning("[Layout] WARNING: %s — %s (actual: %dx%d, expected: %dx%d)",
                         qPrintable(v.widgetPath),
                         qPrintable(v.description),
                         v.actual.width(), v.actual.height(),
                         v.expected.width(), v.expected.height());
                break;
            case LayoutViolation::Severity::Info:
                qDebug("[Layout] INFO: %s — %s (actual: %dx%d)",
                       qPrintable(v.widgetPath),
                       qPrintable(v.description),
                       v.actual.width(), v.actual.height());
                break;
        }
    }

    _previous_violations = std::move(current);
}

// =============================================================================
// Static API
// =============================================================================

auto LayoutSanityChecker::getViolations(QWidget * root) -> std::vector<LayoutViolation> {
    std::vector<LayoutViolation> violations;
    if (root) {
        walkTree(root, violations);
    }
    return violations;
}

auto LayoutSanityChecker::widgetPath(QWidget const * widget) -> QString {
    QStringList parts;
    for (auto const * cur = widget; cur != nullptr; cur = cur->parentWidget()) {
        QString const name = cur->objectName().isEmpty()
                                     ? QString(cur->metaObject()->className())
                                     : cur->objectName();
        parts.prepend(name);
    }
    return parts.join('/');
}

// =============================================================================
// Tree Walking & Checks
// =============================================================================

void LayoutSanityChecker::walkTree(QWidget * widget, std::vector<LayoutViolation> & out) {
    if (!widget || !widget->isVisible()) {
        return;
    }

    // Skip Qt-internal private child widgets (objectName starts with "qt_")
    // These are implementation details of composite widgets (e.g. the QLineEdit
    // inside a QSpinBox) and are not under user control.
    if (widget->objectName().startsWith(QStringLiteral("qt_"))) {
        return;
    }

    // Skip checks for third-party framework internals (ads:: dock system,
    // combo-box popup containers) — we do not control their sizing.
    // BUT still recurse into their children so we check app widgets nested
    // inside dock containers.
    if (isThirdPartyInternal(widget)) {
        for (QObject * child: widget->children()) {
            if (child->isWidgetType()) {
                walkTree(dynamic_cast<QWidget *>(child), out);
            }
        }
        return;
    }

    // Skip widgets inside a collapsed container (e.g. collapsed Section)
    // — they are technically "visible" but are clipped by a 0-height ancestor.
    if (isClippedByAncestor(widget)) {
        return;
    }

    // Skip widgets that are intentionally collapsed (maximumHeight == 0).
    // Section's QScrollArea uses this mechanism to hide content.
    if (widget->maximumHeight() == 0) {
        return;
    }

    // Skip decorative QFrame separators — HLine/VLine are expected to have
    // near-zero dimension in one axis.
    if (isDecorativeFrame(widget)) {
        // Still recurse into children (unlikely for separator frames)
        for (QObject * child: widget->children()) {
            if (child->isWidgetType()) {
                walkTree(dynamic_cast<QWidget *>(child), out);
            }
        }
        return;
    }

    auto const path = widgetPath(widget);
    auto const * className = widget->metaObject()->className();

    // ---------- Check 1: Zero dimension ----------
    if (widget->width() == 0 || widget->height() == 0) {
        out.push_back({
                LayoutViolation::Severity::Critical,
                path,
                QString(className),
                QStringLiteral("Visible widget has zero dimension"),
                widget->size(),
                widget->minimumSizeHint(),
        });
    }

    // ---------- Check 2: Below minimumSizeHint ----------
    QSize const minHint = widget->minimumSizeHint();
    if (minHint.isValid()) {
        bool const widthViolation = minHint.width() > 0 && widget->width() < minHint.width();
        bool const heightViolation = minHint.height() > 0 && widget->height() < minHint.height();
        if (widthViolation || heightViolation) {
            out.push_back({
                    LayoutViolation::Severity::Warning,
                    path,
                    QString(className),
                    QStringLiteral("Widget smaller than minimumSizeHint"),
                    widget->size(),
                    minHint,
            });
        }
    }

    // ---------- Check 3: Extreme aspect ratio ----------
    if (widget->width() > 0 && widget->height() > 0 && !isNaturallyExtreme(widget)) {
        double const ratio = static_cast<double>(widget->width()) /
                             static_cast<double>(widget->height());
        if (ratio > kExtremeAspectRatio || ratio < 1.0 / kExtremeAspectRatio) {
            out.push_back({
                    LayoutViolation::Severity::Info,
                    path,
                    QString(className),
                    QStringLiteral("Extreme aspect ratio (%1:1)").arg(ratio, 0, 'f', 1),
                    widget->size(),
                    QSize{},
            });
        }
    }

    // ---------- Check 4: Label text truncation ----------
    if (auto const * label = qobject_cast<QLabel const *>(widget)) {
        QString const text = label->text();
        if (!text.isEmpty() && label->textFormat() != Qt::RichText) {
            int const textWidth = label->fontMetrics().horizontalAdvance(text);
            // Account for margins/indent
            int const available = label->contentsRect().width();
            if (available > 0 && textWidth > available) {
                out.push_back({
                        LayoutViolation::Severity::Warning,
                        path,
                        QString(className),
                        QStringLiteral("Label text truncated (needs %1 px, has %2 px)")
                                .arg(textWidth)
                                .arg(available),
                        widget->size(),
                        QSize{textWidth + (label->width() - available), label->height()},
                });
            }
        }
    }

    // ---------- Check 5: Spinbox content overflow ----------
    if (auto const * spinBox = qobject_cast<QAbstractSpinBox const *>(widget)) {
        // Estimate the widest text the spinbox needs to display
        QString const currentText = spinBox->text();
        if (!currentText.isEmpty()) {
            int const textWidth = spinBox->fontMetrics().horizontalAdvance(currentText + QStringLiteral(" "));
            // Estimate available text area: total width minus the up/down
            // button area. QStyle::PM_SpinBoxFrameWidth gives the frame
            // border, and the button area is approximately the widget height
            // (buttons are stacked vertically, each roughly square).
            auto const * style = spinBox->style();
            int const frame = style->pixelMetric(QStyle::PM_SpinBoxFrameWidth, nullptr, spinBox);
            int const buttonWidth = spinBox->height();// buttons are ~square, stacked
            int const available = spinBox->width() - buttonWidth - 2 * frame;
            if (available <= 0 || textWidth > available) {
                int const reportedAvailable = std::max(available, 0);
                out.push_back({
                        LayoutViolation::Severity::Warning,
                        path,
                        QString(className),
                        QStringLiteral("Spinbox too narrow for content (needs %1 px, has %2 px)")
                                .arg(textWidth)
                                .arg(reportedAvailable),
                        widget->size(),
                        QSize{widget->width() + (textWidth - available), widget->height()},
                });
            }
        }
    }

    // ---------- Check 6: ComboBox content overflow ----------
    if (auto const * combo = qobject_cast<QComboBox const *>(widget)) {
        // Find the widest item text
        int maxTextWidth = 0;
        for (int i = 0; i < combo->count(); ++i) {
            int const w = combo->fontMetrics().horizontalAdvance(combo->itemText(i));
            if (w > maxTextWidth) {
                maxTextWidth = w;
            }
        }
        if (maxTextWidth > 0) {
            // Estimate the text area: total width minus the dropdown arrow
            // button and frame. The arrow button width is given by
            // PM_ComboBoxFrameWidth (frame) plus a platform-dependent arrow
            // size — we approximate the arrow button as roughly the height
            // of the combo (it's approximately square).
            auto const * style = combo->style();
            int const frame = style->pixelMetric(QStyle::PM_ComboBoxFrameWidth, nullptr, combo);
            int const arrowWidth = combo->height();// arrow button is ~square
            int const available = combo->width() - arrowWidth - 2 * frame;
            if (available <= 0 || maxTextWidth > available) {
                int const reportedAvailable = std::max(available, 0);
                out.push_back({
                        LayoutViolation::Severity::Warning,
                        path,
                        QString(className),
                        QStringLiteral("ComboBox too narrow for content (widest item needs %1 px, has %2 px)")
                                .arg(maxTextWidth)
                                .arg(reportedAvailable),
                        widget->size(),
                        QSize{widget->width() + (maxTextWidth - available), widget->height()},
                });
            }
        }
    }

    // ---------- Recurse into children ----------
    for (QObject * child: widget->children()) {
        if (child->isWidgetType()) {
            walkTree(dynamic_cast<QWidget *>(child), out);
        }
    }
}
