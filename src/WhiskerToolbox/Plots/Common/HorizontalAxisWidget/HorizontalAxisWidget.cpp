#include "HorizontalAxisWidget.hpp"

#include "PlotZoomProfile.hpp"

#include <QPainter>
#include <QString>

#include <cmath>
#include <optional>

HorizontalAxisWidget::HorizontalAxisWidget(QWidget * parent)
    : QWidget(parent) {
    _applyDisplayModeDefaults();
}

void HorizontalAxisWidget::setRangeGetter(RangeGetter getter) {
    _range_getter = std::move(getter);
    _use_getter = true;
    update();
}

void HorizontalAxisWidget::setRange(double min, double max) {
    _min_value = min;
    _max_value = max;
    _use_getter = false;
    update();
}

void HorizontalAxisWidget::setAxisMapping(CorePlotting::AxisMapping mapping) {
    _axis_mapping = std::move(mapping);
    update();
}

void HorizontalAxisWidget::clearAxisMapping() {
    _axis_mapping.reset();
    update();
}

CorePlotting::AxisMapping const * HorizontalAxisWidget::axisMapping() const {
    if (_axis_mapping.has_value()) {
        return &_axis_mapping.value();
    }
    return nullptr;
}

void HorizontalAxisWidget::setDisplayMode(Neuralyzer::Plots::AxisDisplayMode mode) {
    if (_display_mode != mode) {
        _display_mode = mode;
        _applyDisplayModeDefaults();
        update();
    }
}

void HorizontalAxisWidget::setThickness(int px) {
    if (_thickness != px) {
        _thickness = px;
        setMinimumHeight(_thickness);
        setMaximumHeight(_thickness);
        updateGeometry();
        update();
    }
}

void HorizontalAxisWidget::setShowExtentLabels(bool show) {
    if (_show_extent_labels != show) {
        _show_extent_labels = show;
        update();
    }
}

void HorizontalAxisWidget::setNegativeLabelColor(QColor color) {
    _negative_label_color = color;
    update();
}

void HorizontalAxisWidget::setTickConfig(Neuralyzer::Plots::AxisTickConfig config) {
    _tick_config = config;
    update();
}

QSize HorizontalAxisWidget::sizeHint() const {
    return QSize(200, _thickness);
}

void HorizontalAxisWidget::_applyDisplayModeDefaults() {
    if (_display_mode == Neuralyzer::Plots::AxisDisplayMode::Ruler) {
        _thickness = kRulerThickness;
        _show_extent_labels = false;
    } else {
        _thickness = kPlotThickness;
        _show_extent_labels = true;
    }
    setMinimumHeight(_thickness);
    setMaximumHeight(_thickness);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HorizontalAxisWidget::paintEvent(QPaintEvent * /* event */) {
    std::optional<Neuralyzer::Plots::PlotZoomProfileAxisPaintScope> axis_profile;
    if (_display_mode == Neuralyzer::Plots::AxisDisplayMode::Plot) {
        axis_profile.emplace("horizontal");
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor(30, 30, 30));

    double min_val = 0.0;
    double max_val = 0.0;

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

    double const range = max_val - min_val;
    double const tick_interval = computeTickInterval(range, _tick_config);

    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawLine(0, 0, width(), 0);

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double const v: computeTickPositions(min_val, max_val, _tick_config)) {
        bool const is_zero = std::abs(v) < tick_interval * 0.01;
        bool const is_major = Neuralyzer::Plots::isMajorTick(v, tick_interval);

        if (!Neuralyzer::Plots::shouldDrawTick(v, tick_interval, _tick_config.show_minor_ticks)) {
            continue;
        }

        int const px = _valueToPixelX(v, min_val, max_val);
        int const tick_h = is_major ? kMajorTickHeight : kTickHeight;

        QColor const tick_color = _tickColor(v, tick_interval, is_zero, is_major);
        int const pen_width = is_zero ? 2 : 1;
        painter.setPen(QPen(tick_color, pen_width));
        painter.drawLine(px, 0, px, tick_h);

        if (is_major || is_zero) {
            QString label;
            if (_axis_mapping.has_value() && _axis_mapping->isValid()) {
                label = QString::fromStdString(_axis_mapping->formatLabel(v));
            } else {
                label = QString::number(v, 'f', 1);
                if (label.contains('.')) {
                    while (label.endsWith('0')) {
                        label.chop(1);
                    }
                    if (label.endsWith('.')) {
                        label.chop(1);
                    }
                }
            }

            painter.setPen(_tickColor(v, tick_interval, is_zero, is_major));

            QRect const label_rect(px - 30, kMajorTickHeight + kLabelOffset, 60, 14);
            painter.drawText(label_rect, Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }

    if (!_show_extent_labels) {
        return;
    }

    painter.setPen(QColor(100, 150, 200));
    font.setPointSize(7);
    painter.setFont(font);

    QString const min_label = QString("min: %1").arg(min_val, 0, 'f', 1);
    QString const max_label = QString("max: %1").arg(max_val, 0, 'f', 1);

    QRect const min_rect(2, height() - 20, width() / 2 - 4, 12);
    QRect const max_rect(width() / 2 + 2, height() - 20, width() / 2 - 4, 12);

    painter.drawText(min_rect, Qt::AlignLeft | Qt::AlignVCenter, min_label);
    painter.drawText(max_rect, Qt::AlignRight | Qt::AlignVCenter, max_label);
}

int HorizontalAxisWidget::_valueToPixelX(double value, double min, double max) const {
    if (max <= min) {
        return 0;
    }

    double const normalized = (value - min) / (max - min);
    return static_cast<int>(normalized * width());
}

QColor HorizontalAxisWidget::_tickColor(double value, double tick_interval, bool is_zero, bool is_major) const {
    if (is_zero) {
        return QColor(255, 100, 100);
    }
    if (value < 0.0) {
        return _negative_label_color;
    }
    if (is_major) {
        return QColor(180, 180, 180);
    }
    return QColor(100, 100, 100);
}
