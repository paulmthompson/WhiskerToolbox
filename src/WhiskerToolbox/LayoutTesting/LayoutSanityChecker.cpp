/**
 * @file LayoutSanityChecker.cpp
 * @brief Implementation of the runtime layout constraint checker
 */

#include "LayoutSanityChecker.hpp"

#include <QApplication>
#include <QEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QStringList>
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
            auto * widget = dynamic_cast<QWidget *>(obj);
            if (widget->isWindow()) {
                scheduleCheck();
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void LayoutSanityChecker::scheduleCheck() {
    // Restart the timer — this coalesces rapid layout cascades
    _debounce_timer.start();
}

void LayoutSanityChecker::runCheck() {
    for (QWidget * window: QApplication::topLevelWidgets()) {
        auto violations = getViolations(window);
        for (auto const & v: violations) {
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
    }
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

    // Skip third-party framework internals (ads:: dock system, combo-box
    // popup containers) — we do not control their sizing.
    if (isThirdPartyInternal(widget)) {
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

    // ---------- Recurse into children ----------
    for (QObject * child: widget->children()) {
        if (child->isWidgetType()) {
            walkTree(dynamic_cast<QWidget *>(child), out);
        }
    }
}
