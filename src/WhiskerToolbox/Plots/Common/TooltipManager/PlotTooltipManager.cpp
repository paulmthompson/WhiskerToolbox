/**
 * @file PlotTooltipManager.cpp
 * @brief Implementation of the shared tooltip manager for plot widgets
 */

#include "PlotTooltipManager.hpp"

#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

namespace WhiskerToolbox::Plots {

// =============================================================================
// PixmapPopup — lightweight floating widget for rich tooltips
// =============================================================================

class PlotTooltipManager::PixmapPopup : public QWidget {
public:
    explicit PixmapPopup(QWidget * parent)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAutoFillBackground(true);

        _label = new QLabel(this);
        _label->setAlignment(Qt::AlignCenter);

        auto * layout = new QVBoxLayout(this);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->addWidget(_label);
    }

    void setPixmap(QPixmap const & pixmap) {
        _label->setPixmap(pixmap);
        adjustSize();
    }

private:
    QLabel * _label{nullptr};
};

// =============================================================================
// PlotTooltipManager
// =============================================================================

PlotTooltipManager::PlotTooltipManager(QWidget * parent_widget, int dwell_ms)
    : QObject(parent_widget),
      _parent_widget(parent_widget) {
    _dwell_timer = new QTimer(this);
    _dwell_timer->setSingleShot(true);
    _dwell_timer->setInterval(dwell_ms);
    connect(_dwell_timer, &QTimer::timeout,
            this, &PlotTooltipManager::onDwellTimerFired);
}

PlotTooltipManager::~PlotTooltipManager() {
    // Stop any pending timer
    _dwell_timer->stop();
    hidePixmapPopup();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void PlotTooltipManager::setHitTestProvider(TooltipHitTestFn provider) {
    _hit_test_fn = std::move(provider);
}

void PlotTooltipManager::setTextProvider(TooltipTextFn provider) {
    // Wrap the text provider into a full content provider
    if (provider) {
        _content_fn = [fn = std::move(provider)](PlotTooltipHit const & hit) {
            PlotTooltipContent content;
            content.text = fn(hit);
            return content;
        };
    } else {
        _content_fn = nullptr;
    }
}

void PlotTooltipManager::setContentProvider(TooltipContentFn provider) {
    _content_fn = std::move(provider);
}

void PlotTooltipManager::setDwellTime(int ms) {
    _dwell_timer->setInterval(ms);
}

void PlotTooltipManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_enabled) {
        hide();
    }
}

bool PlotTooltipManager::isEnabled() const {
    return _enabled;
}

// ---------------------------------------------------------------------------
// Event forwarding
// ---------------------------------------------------------------------------

void PlotTooltipManager::onMouseMove(QPoint screen_pos, bool is_interacting) {
    // Always hide existing tooltip on move — it will reappear after dwell
    QToolTip::hideText();
    hidePixmapPopup();

    if (!_enabled || is_interacting) {
        _dwell_timer->stop();
        _pending_pos.reset();
        return;
    }

    _pending_pos = screen_pos;
    _dwell_timer->start();// restart timer
}

void PlotTooltipManager::onLeave() {
    _dwell_timer->stop();
    _pending_pos.reset();
    QToolTip::hideText();
    hidePixmapPopup();
}

void PlotTooltipManager::hide() {
    _dwell_timer->stop();
    _pending_pos.reset();
    QToolTip::hideText();
    hidePixmapPopup();
}

// ---------------------------------------------------------------------------
// Timer slot
// ---------------------------------------------------------------------------

void PlotTooltipManager::onDwellTimerFired() {
    if (!_enabled || !_pending_pos || !_hit_test_fn || !_content_fn) {
        return;
    }

    QPoint const pos = *_pending_pos;
    _pending_pos.reset();

    // Step 1: Hit test (may be expensive — QuadTree lookup, etc.)
    auto hit = _hit_test_fn(pos);
    if (!hit) {
        return;
    }

    // Step 2: Generate content (may be expensive — glyph rendering, etc.)
    auto content = _content_fn(*hit);
    if (content.isEmpty()) {
        return;
    }

    // Step 3: Display
    if (content.hasPixmap()) {
        showPixmapTooltip(content.pixmap, pos);
    } else {
        showTextTooltip(content.text, pos);
    }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void PlotTooltipManager::showTextTooltip(QString const & text, QPoint screen_pos) {
    hidePixmapPopup();
    QToolTip::showText(_parent_widget->mapToGlobal(screen_pos), text, _parent_widget);
}

void PlotTooltipManager::showPixmapTooltip(QPixmap const & pixmap, QPoint screen_pos) {
    QToolTip::hideText();

    if (!_pixmap_popup) {
        _pixmap_popup = std::make_unique<PixmapPopup>(nullptr);
    }

    _pixmap_popup->setPixmap(pixmap);

    // Position near the cursor, offset slightly to avoid obscuring it
    QPoint global_pos = _parent_widget->mapToGlobal(screen_pos);
    constexpr int CURSOR_OFFSET = 16;
    global_pos += QPoint(CURSOR_OFFSET, CURSOR_OFFSET);

    // Ensure the popup stays on-screen
    if (auto * screen = QApplication::screenAt(global_pos)) {
        QRect const screen_rect = screen->availableGeometry();
        QSize const popup_size = _pixmap_popup->sizeHint();

        if (global_pos.x() + popup_size.width() > screen_rect.right()) {
            global_pos.setX(global_pos.x() - popup_size.width() - 2 * CURSOR_OFFSET);
        }
        if (global_pos.y() + popup_size.height() > screen_rect.bottom()) {
            global_pos.setY(global_pos.y() - popup_size.height() - 2 * CURSOR_OFFSET);
        }
    }

    _pixmap_popup->move(global_pos);
    _pixmap_popup->show();
}

void PlotTooltipManager::hidePixmapPopup() {
    if (_pixmap_popup) {
        _pixmap_popup->hide();
    }
}

}// namespace WhiskerToolbox::Plots
