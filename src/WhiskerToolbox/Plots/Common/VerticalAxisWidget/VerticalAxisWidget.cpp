#include "VerticalAxisWidget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QString>

#include <cmath>

VerticalAxisWidget::VerticalAxisWidget(QWidget * parent)
    : QWidget(parent)
{
    setMinimumWidth(kAxisWidth);
    setMaximumWidth(kAxisWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void VerticalAxisWidget::setRangeGetter(RangeGetter getter)
{
    _range_getter = std::move(getter);
    _use_getter = true;
    update();
}

void VerticalAxisWidget::setRange(double min, double max)
{
    _min_value = min;
    _max_value = max;
    _use_getter = false;
    update();
}

void VerticalAxisWidget::setAxisMapping(CorePlotting::AxisMapping mapping)
{
    _axis_mapping = std::move(mapping);
    update();
}

void VerticalAxisWidget::clearAxisMapping()
{
    _axis_mapping.reset();
    update();
}

CorePlotting::AxisMapping const * VerticalAxisWidget::axisMapping() const
{
    if (_axis_mapping.has_value()) {
        return &_axis_mapping.value();
    }
    return nullptr;
}

QSize VerticalAxisWidget::sizeHint() const
{
    return QSize(kAxisWidth, 200);
}

void VerticalAxisWidget::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(30, 30, 30));

    double min_val;
    double max_val;

    if (_use_getter && _range_getter) {
        auto const range = _range_getter();
        min_val = range.first;
        max_val = range.second;
    } else {
        min_val = _min_value;
        max_val = _max_value;
    }

    if (max_val <= min_val) {
        return;
    }

    double range = max_val - min_val;

    // Draw axis line at right edge
    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawLine(width() - 1, 0, width() - 1, height());

    // Compute nice tick interval
    double tick_interval = computeTickInterval(range);

    // Find first tick position
    double first_tick = std::ceil(min_val / tick_interval) * tick_interval;

    // Draw ticks and labels
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double v = first_tick; v <= max_val; v += tick_interval) {
        int py = valueToPixelY(v, min_val, max_val);

        // Check if this is a major tick (at wider intervals) or at zero
        bool is_zero = std::abs(v) < tick_interval * 0.01;
        bool is_major = std::fmod(std::abs(v), tick_interval * 5) < tick_interval * 0.01 || is_zero;

        // Draw tick
        int tick_w = is_major ? kMajorTickWidth : kTickWidth;

        if (is_zero) {
            // Zero line - highlighted
            painter.setPen(QPen(QColor(255, 100, 100), 2));
        } else if (is_major) {
            painter.setPen(QPen(QColor(180, 180, 180), 1));
        } else {
            painter.setPen(QPen(QColor(100, 100, 100), 1));
        }

        painter.drawLine(width() - 1, py, width() - 1 - tick_w, py);

        // Draw label for major ticks
        if (is_major || is_zero) {
            QString label;
            if (_axis_mapping.has_value() && _axis_mapping->isValid()) {
                // Use AxisMapping: the values are already in domain space,
                // so formatLabel directly
                label = QString::fromStdString(_axis_mapping->formatLabel(v));
            } else {
                // Default: decimal formatting
                label = QString::number(v, 'f', 1);
                // Remove trailing zeros
                if (label.contains('.')) {
                    while (label.endsWith('0')) {
                        label.chop(1);
                    }
                    if (label.endsWith('.')) {
                        label.chop(1);
                    }
                }
            }

            painter.setPen(is_zero ? QColor(255, 100, 100) : QColor(180, 180, 180));

            QRect label_rect(kLabelOffset, py - 7, width() - kLabelOffset - kMajorTickWidth - 2, 14);
            painter.drawText(label_rect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }

    // Draw extent labels at edges (showing actual bounds)
    painter.setPen(QColor(100, 150, 200));
    font.setPointSize(7);
    painter.setFont(font);

    QString min_label = QString("min: %1").arg(min_val, 0, 'f', 1);
    QString max_label = QString("max: %1").arg(max_val, 0, 'f', 1);

    QRect min_rect(2, height() - 20, width() - 4, 12);
    QRect max_rect(2, 2, width() - 4, 12);

    painter.drawText(min_rect, Qt::AlignLeft | Qt::AlignVCenter, min_label);
    painter.drawText(max_rect, Qt::AlignLeft | Qt::AlignVCenter, max_label);
}

double VerticalAxisWidget::computeTickInterval(double range) const
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

int VerticalAxisWidget::valueToPixelY(double value, double min, double max) const
{
    if (max <= min) {
        return 0;
    }

    // Map value from [min, max] to [height, 0] (top to bottom)
    // Note: In screen coordinates, Y=0 is at top, so we invert
    double normalized = (value - min) / (max - min);
    return static_cast<int>(height() - normalized * height());
}
