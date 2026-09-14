#include "VerticalAxisWidget.hpp"

#include "PlotZoomProfile.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QString>

#include <cmath>
#include <optional>

VerticalAxisWidget::VerticalAxisWidget(QWidget * parent)
    : QWidget(parent) {
    _applyDisplayModeDefaults();
}

void VerticalAxisWidget::setRangeGetter(RangeGetter getter) {
    _range_getter = std::move(getter);
    _use_getter = true;
    update();
}

void VerticalAxisWidget::setRange(double min, double max) {
    _min_value = min;
    _max_value = max;
    _use_getter = false;
    update();
}

void VerticalAxisWidget::setAxisMapping(CorePlotting::AxisMapping mapping) {
    _axis_mapping = std::move(mapping);
    update();
}

void VerticalAxisWidget::clearAxisMapping() {
    _axis_mapping.reset();
    update();
}

CorePlotting::AxisMapping const * VerticalAxisWidget::axisMapping() const {
    if (_axis_mapping.has_value()) {
        return &_axis_mapping.value();
    }
    return nullptr;
}

void VerticalAxisWidget::setInverted(bool inverted) {
    if (_inverted != inverted) {
        _inverted = inverted;
        update();
    }
}

bool VerticalAxisWidget::isInverted() const {
    return _inverted;
}

void VerticalAxisWidget::setDisplayMode(Neuralyzer::Plots::AxisDisplayMode mode) {
    if (_display_mode != mode) {
        _display_mode = mode;
        _applyDisplayModeDefaults();
        update();
    }
}

void VerticalAxisWidget::setThickness(int px) {
    if (_thickness != px) {
        _thickness = px;
        setMinimumWidth(_thickness);
        setMaximumWidth(_thickness);
        updateGeometry();
        update();
    }
}

void VerticalAxisWidget::setShowExtentLabels(bool show) {
    if (_show_extent_labels != show) {
        _show_extent_labels = show;
        update();
    }
}

void VerticalAxisWidget::setNegativeLabelColor(QColor color) {
    _negative_label_color = color;
    update();
}

void VerticalAxisWidget::setTickConfig(Neuralyzer::Plots::AxisTickConfig config) {
    _tick_config = config;
    update();
}

QSize VerticalAxisWidget::sizeHint() const {
    return {_thickness, 200};
}

void VerticalAxisWidget::_applyDisplayModeDefaults() {
    if (_display_mode == Neuralyzer::Plots::AxisDisplayMode::Ruler) {
        _thickness = kRulerThickness;
        _show_extent_labels = false;
    } else {
        _thickness = kPlotThickness;
        _show_extent_labels = true;
    }
    setMinimumWidth(_thickness);
    setMaximumWidth(_thickness);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void VerticalAxisWidget::paintEvent(QPaintEvent * /* event */) {
    std::optional<Neuralyzer::Plots::PlotZoomProfileAxisPaintScope> axis_profile;
    if (_display_mode == Neuralyzer::Plots::AxisDisplayMode::Plot) {
        axis_profile.emplace("vertical");
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

    bool const ruler_mode = _display_mode == Neuralyzer::Plots::AxisDisplayMode::Ruler;
    int const axis_x = ruler_mode ? 0 : width() - 1;

    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawLine(axis_x, 0, axis_x, height());

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double const v: computeTickPositions(min_val, max_val, _tick_config)) {
        bool const is_zero = std::abs(v) < tick_interval * 0.01;
        bool const is_major = Neuralyzer::Plots::isMajorTick(v, tick_interval);

        if (!Neuralyzer::Plots::shouldDrawTick(v, tick_interval, _tick_config.show_minor_ticks)) {
            continue;
        }

        int const py = _valueToPixelY(v, min_val, max_val);
        int const tick_w = is_major ? kMajorTickWidth : kTickWidth;

        QColor const tick_color = _tickColor(v, tick_interval, is_zero, is_major);
        int const pen_width = is_zero ? 2 : 1;
        painter.setPen(QPen(tick_color, pen_width));

        if (ruler_mode) {
            painter.drawLine(axis_x, py, axis_x + tick_w, py);
        } else {
            painter.drawLine(axis_x, py, axis_x - tick_w, py);
        }

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

            QFontMetrics const fm(painter.font());
            int const text_width = fm.horizontalAdvance(label);
            int const text_height = fm.height();

            bool const axis_on_left = axis_x == 0;
            int const label_anchor_x = axis_on_left ? (axis_x + tick_w + kLabelOffset)
                                                    : (axis_x - tick_w - kLabelOffset);

            // Rotated -90°, local +x extends upward on screen. Place the anchor so the
            // bottom edge of the label sits kVerticalLabelGap pixels below the tick.
            painter.save();
            painter.translate(label_anchor_x, py + kVerticalLabelGap + text_width);
            painter.rotate(-90.0);
            QRect const label_rect(0, -text_height / 2, text_width + 2, text_height);
            painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
            painter.restore();
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

    QRect const top_rect(2, 2, width() - 4, 12);
    QRect const bottom_rect(2, height() - 20, width() - 4, 12);

    if (_inverted) {
        painter.drawText(top_rect, Qt::AlignLeft | Qt::AlignVCenter, min_label);
        painter.drawText(bottom_rect, Qt::AlignLeft | Qt::AlignVCenter, max_label);
    } else {
        painter.drawText(bottom_rect, Qt::AlignLeft | Qt::AlignVCenter, min_label);
        painter.drawText(top_rect, Qt::AlignLeft | Qt::AlignVCenter, max_label);
    }
}

int VerticalAxisWidget::_valueToPixelY(double value, double min, double max) const {
    if (max <= min) {
        return 0;
    }

    double const normalized = (value - min) / (max - min);
    if (_inverted) {
        return static_cast<int>(normalized * height());
    }
    return static_cast<int>(height() - normalized * height());
}

QColor VerticalAxisWidget::_tickColor(double value, double tick_interval, bool is_zero, bool is_major) const {
    if (is_zero) {
        return {255, 100, 100};
    }
    if (value < 0.0) {
        return _negative_label_color;
    }
    if (is_major) {
        return {180, 180, 180};
    }
    return {100, 100, 100};
}
