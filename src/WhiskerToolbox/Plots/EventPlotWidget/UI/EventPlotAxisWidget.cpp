#include "EventPlotAxisWidget.hpp"

#include "Core/EventPlotState.hpp"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

EventPlotAxisWidget::EventPlotAxisWidget(QWidget * parent)
    : QWidget(parent)
{
    setMinimumHeight(kAxisHeight);
    setMaximumHeight(kAxisHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void EventPlotAxisWidget::setState(std::shared_ptr<EventPlotState> state)
{
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        connect(_state.get(), &EventPlotState::viewStateChanged,
                this, QOverload<>::of(&QWidget::update));
        connect(_state.get(), &EventPlotState::stateChanged,
                this, QOverload<>::of(&QWidget::update));
    }

    update();
}

QSize EventPlotAxisWidget::sizeHint() const
{
    return QSize(200, kAxisHeight);
}

void EventPlotAxisWidget::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!_state) {
        return;
    }

    auto const & view = _state->viewState();

    // Calculate visible range with zoom and pan
    double x_range = view.x_max - view.x_min;
    double zoomed_range = x_range / view.x_zoom;
    double x_center = (view.x_min + view.x_max) / 2.0;
    double visible_min = x_center - zoomed_range / 2.0 + view.x_pan;
    double visible_max = x_center + zoomed_range / 2.0 + view.x_pan;

    // Draw axis line at top
    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawLine(0, 0, width(), 0);

    // Compute nice tick interval
    double tick_interval = computeTickInterval(zoomed_range);
    
    // Find first tick position
    double first_tick = std::ceil(visible_min / tick_interval) * tick_interval;

    // Draw ticks and labels
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double t = first_tick; t <= visible_max; t += tick_interval) {
        int px = timeToPixelX(t);
        
        // Check if this is a major tick (at wider intervals) or at zero
        bool is_zero = std::abs(t) < tick_interval * 0.01;
        bool is_major = std::fmod(std::abs(t), tick_interval * 5) < tick_interval * 0.01 || is_zero;

        // Draw tick
        int tick_h = is_major ? kMajorTickHeight : kTickHeight;
        
        if (is_zero) {
            // Zero line - highlighted
            painter.setPen(QPen(QColor(255, 100, 100), 2));
        } else if (is_major) {
            painter.setPen(QPen(QColor(180, 180, 180), 1));
        } else {
            painter.setPen(QPen(QColor(100, 100, 100), 1));
        }
        
        painter.drawLine(px, 0, px, tick_h);

        // Draw label for major ticks
        if (is_major || is_zero) {
            QString label;
            if (is_zero) {
                label = "0";
            } else if (t > 0) {
                label = QString("+%1").arg(static_cast<int>(t));
            } else {
                label = QString::number(static_cast<int>(t));
            }

            painter.setPen(is_zero ? QColor(255, 100, 100) : QColor(180, 180, 180));
            
            QRect label_rect(px - 30, kLabelOffset, 60, 15);
            painter.drawText(label_rect, Qt::AlignCenter, label);
        }
    }

    // Draw extent labels at edges (showing actual bounds)
    painter.setPen(QColor(100, 150, 200));
    font.setPointSize(7);
    painter.setFont(font);

    QString min_label = QString("min: %1").arg(static_cast<int>(view.x_min));
    QString max_label = QString("max: %1").arg(static_cast<int>(view.x_max));
    
    QRect min_rect(2, kAxisHeight - 12, 60, 12);
    QRect max_rect(width() - 62, kAxisHeight - 12, 60, 12);
    
    painter.drawText(min_rect, Qt::AlignLeft | Qt::AlignVCenter, min_label);
    painter.drawText(max_rect, Qt::AlignRight | Qt::AlignVCenter, max_label);
}

double EventPlotAxisWidget::computeTickInterval(double range) const
{
    // Aim for roughly 5-10 ticks
    double target_ticks = 7.0;
    double raw_interval = range / target_ticks;

    // Round to nice number (1, 2, 5, 10, 20, 50, 100, ...)
    double magnitude = std::pow(10.0, std::floor(std::log10(raw_interval)));
    double normalized = raw_interval / magnitude;

    double nice;
    if (normalized < 1.5) {
        nice = 1.0;
    } else if (normalized < 3.5) {
        nice = 2.0;
    } else if (normalized < 7.5) {
        nice = 5.0;
    } else {
        nice = 10.0;
    }

    return nice * magnitude;
}

int EventPlotAxisWidget::timeToPixelX(double time) const
{
    if (!_state) {
        return 0;
    }

    auto const & view = _state->viewState();

    // Calculate visible range with zoom and pan
    double x_range = view.x_max - view.x_min;
    double zoomed_range = x_range / view.x_zoom;
    double x_center = (view.x_min + view.x_max) / 2.0;
    double visible_min = x_center - zoomed_range / 2.0 + view.x_pan;

    // Convert time to pixel
    double normalized = (time - visible_min) / zoomed_range;
    return static_cast<int>(normalized * width());
}
