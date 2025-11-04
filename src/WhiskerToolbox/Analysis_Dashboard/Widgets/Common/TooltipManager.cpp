#include "TooltipManager.hpp"

#include <QApplication>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

TooltipManager::TooltipManager(QObject * parent)
    : QObject(parent),
      _show_timer(new QTimer(this)),
      _refresh_timer(new QTimer(this)) {
    _show_timer->setSingleShot(true);
    _refresh_timer->setSingleShot(false);

    connect(_show_timer, &QTimer::timeout, this, &TooltipManager::handleShowTooltip);
    connect(_refresh_timer, &QTimer::timeout, this, &TooltipManager::handleRefreshTooltip);
}

void TooltipManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_enabled) {
        hideTooltip();
    }
}

void TooltipManager::setContentProvider(TooltipContentProvider provider) {
    _content_provider = std::move(provider);
}

void TooltipManager::setTiming(int show_delay_ms, int refresh_interval_ms) {
    _show_delay_ms = show_delay_ms;
    _refresh_interval_ms = refresh_interval_ms;
}

void TooltipManager::handleMouseMove(QPoint const & screen_pos) {
    if (!_enabled || _suppressed || !_content_provider) {
        return;
    }

    _current_mouse_pos = screen_pos;

    // Restart the show timer
    _show_timer->start(_show_delay_ms);

    // If tooltip is already visible, keep refreshing it
    if (_refresh_timer->isActive()) {
        // Don't restart refresh timer - let it continue
    }
}

void TooltipManager::handleMouseLeave() {
    hideTooltip();
}

void TooltipManager::hideTooltip() {
    stopAllTimers();
    QToolTip::hideText();
}

void TooltipManager::setSuppressed(bool suppressed) {
    _suppressed = suppressed;
    if (_suppressed) {
        hideTooltip();
    }
}

void TooltipManager::handleShowTooltip() {
    if (!_enabled || _suppressed || !_content_provider) {
        return;
    }

    auto tooltip_text = _content_provider(_current_mouse_pos);
    if (tooltip_text.has_value() && !tooltip_text->isEmpty()) {
        QToolTip::showText(_current_mouse_pos, *tooltip_text);

        // Start refresh timer to keep tooltip updated
        _refresh_timer->start(_refresh_interval_ms);
    }
}

void TooltipManager::handleRefreshTooltip() {
    if (!_enabled || _suppressed || !_content_provider) {
        hideTooltip();
        return;
    }

    auto tooltip_text = _content_provider(_current_mouse_pos);
    if (tooltip_text.has_value() && !tooltip_text->isEmpty()) {
        // Update tooltip text at current position
        QToolTip::showText(_current_mouse_pos, *tooltip_text);
    } else {
        hideTooltip();
    }
}

void TooltipManager::stopAllTimers() {
    _show_timer->stop();
    _refresh_timer->stop();
}
