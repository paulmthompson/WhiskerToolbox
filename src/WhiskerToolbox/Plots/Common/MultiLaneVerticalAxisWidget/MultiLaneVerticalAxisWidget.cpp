/**
 * @file MultiLaneVerticalAxisWidget.cpp
 * @brief Implementation of MultiLaneVerticalAxisWidget
 */

#include "MultiLaneVerticalAxisWidget.hpp"
#include "Core/MultiLaneVerticalAxisState.hpp"
#include "Core/MultiLaneVerticalAxisStateData.hpp"

#include <QPaintEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

MultiLaneVerticalAxisWidget::MultiLaneVerticalAxisWidget(
        MultiLaneVerticalAxisState * state,
        QWidget * parent)
    : QWidget(parent),
      _state(state) {
    setMinimumWidth(kAxisWidth);
    setMaximumWidth(kAxisWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // Connect to state signals
    if (_state) {
        connect(_state, &MultiLaneVerticalAxisState::lanesChanged,
                this, QOverload<>::of(&QWidget::update));
        connect(_state, &MultiLaneVerticalAxisState::visibilityChanged,
                this, QOverload<>::of(&QWidget::update));
    }
}

void MultiLaneVerticalAxisWidget::setState(MultiLaneVerticalAxisState * state) {
    // Disconnect from old state
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    // Reconnect to new state
    if (_state) {
        connect(_state, &MultiLaneVerticalAxisState::lanesChanged,
                this, QOverload<>::of(&QWidget::update));
        connect(_state, &MultiLaneVerticalAxisState::visibilityChanged,
                this, QOverload<>::of(&QWidget::update));
    }

    update();
}

void MultiLaneVerticalAxisWidget::setViewportGetter(ViewportGetter getter) {
    _viewport_getter = std::move(getter);
    update();
}

void MultiLaneVerticalAxisWidget::setPanGetter(std::function<float()> getter) {
    // Legacy compatibility: wrap pan getter as viewport getter using stored y_min/y_max
    auto y_min = _y_min;
    auto y_max = _y_max;
    _viewport_getter = [pan_fn = std::move(getter), y_min, y_max]() -> std::pair<float, float> {
        float const pan = pan_fn ? pan_fn() : 0.0f;
        return {y_min + pan, y_max + pan};
    };
    update();
}

void MultiLaneVerticalAxisWidget::setYRange(float y_min, float y_max) {
    _y_min = y_min;
    _y_max = y_max;
    update();
}

QSize MultiLaneVerticalAxisWidget::sizeHint() const {
    return {kAxisWidth, 200};
}

void MultiLaneVerticalAxisWidget::paintEvent(QPaintEvent * /* event */) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background — matches DataViewer dark theme
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!_state) {
        return;
    }

    auto const & lanes = _state->lanes();
    if (lanes.empty()) {
        return;
    }

    bool const show_labels = _state->showLabels();
    bool const show_separators = _state->showSeparators();

    if (!show_labels && !show_separators) {
        return;
    }

    // Draw the axis line at the right edge
    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawLine(width() - 1, 0, width() - 1, height());

    QFont label_font = painter.font();
    label_font.setPointSize(8);
    painter.setFont(label_font);

    QFontMetrics const fm(label_font);
    int const label_height = fm.height();

    for (auto const & lane: lanes) {
        // Cull lanes that are entirely off-screen
        if (!isLaneVisible(lane.y_center, lane.y_extent)) {
            continue;
        }

        int const center_px = ndcToPixelY(lane.y_center);
        float const half_extent = lane.y_extent / 2.0f;
        int const top_px = ndcToPixelY(lane.y_center + half_extent);
        int const bottom_px = ndcToPixelY(lane.y_center - half_extent);

        // --- Lane separator lines (at top boundary of each lane) ---
        if (show_separators) {
            painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
            painter.drawLine(0, top_px, width() - 2, top_px);
        }

        // --- Channel label at the center of the lane ---
        if (show_labels) {
            // Elide label text if it doesn't fit in the available width
            int const available_width = width() - 6;// 3px margin on each side
            QString const label = fm.elidedText(
                    QString::fromStdString(lane.label),
                    Qt::ElideRight,
                    available_width);

            // Only draw if the lane is tall enough to fit the label
            int const lane_height_px = std::abs(bottom_px - top_px);
            if (lane_height_px >= label_height) {
                painter.setPen(QColor(200, 200, 200));
                QRect const label_rect(3, center_px - label_height / 2,
                                       available_width, label_height);
                painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
            }
        }
    }

    // Draw bottom separator for the last lane
    if (show_separators && !lanes.empty()) {
        auto const & last = lanes.back();
        float const half_extent = last.y_extent / 2.0f;
        int const bottom_px = ndcToPixelY(last.y_center - half_extent);
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
        painter.drawLine(0, bottom_px, width() - 2, bottom_px);
    }
}

int MultiLaneVerticalAxisWidget::ndcToPixelY(float ndc_y) const {
    auto const [eff_y_min, eff_y_max] = _viewport_getter
                                                ? _viewport_getter()
                                                : std::make_pair(_y_min, _y_max);

    float const range = eff_y_max - eff_y_min;
    if (std::abs(range) < 1e-10f) {
        return 0;
    }

    float const normalized = (ndc_y - eff_y_min) / range;
    return static_cast<int>(static_cast<float>(height()) * (1.0f - normalized));
}

bool MultiLaneVerticalAxisWidget::isLaneVisible(float center, float extent) const {
    auto const [eff_y_min, eff_y_max] = _viewport_getter
                                                ? _viewport_getter()
                                                : std::make_pair(_y_min, _y_max);
    float const half = extent / 2.0f;
    float const lane_top = center + half;
    float const lane_bottom = center - half;

    // Lane is visible if it overlaps the effective viewport
    return lane_top >= eff_y_min && lane_bottom <= eff_y_max;
}
