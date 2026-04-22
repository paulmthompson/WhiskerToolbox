/**
 * @file MultiLaneVerticalAxisWidget.cpp
 * @brief Implementation of MultiLaneVerticalAxisWidget
 */

#include "MultiLaneVerticalAxisWidget.hpp"
#include "Core/MultiLaneVerticalAxisState.hpp"
#include "Core/MultiLaneVerticalAxisStateData.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
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
    setFocusPolicy(Qt::ClickFocus);

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

    for (int i = 0; i < static_cast<int>(lanes.size()); ++i) {
        auto const & lane = lanes[static_cast<size_t>(i)];

        // Cull lanes that are entirely off-screen
        if (!isLaneVisible(lane.y_center, lane.y_extent)) {
            continue;
        }

        bool const is_dragged = _drag_active && (i == _dragged_lane_ndc_index);

        int const center_px = ndcToPixelY(lane.y_center);
        float const half_extent = lane.y_extent / 2.0f;
        int const top_px = ndcToPixelY(lane.y_center + half_extent);
        int const bottom_px = ndcToPixelY(lane.y_center - half_extent);

        // Highlight dragged lane with a translucent rectangle drawn behind text
        if (is_dragged) {
            painter.fillRect(0, top_px, width(), bottom_px - top_px, QColor(100, 180, 255, 55));
        }

        // --- Lane separator lines (at top boundary of each lane) ---
        if (show_separators) {
            painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
            painter.drawLine(0, top_px, width() - 2, top_px);
        }

        // --- Channel label at the center of the lane ---
        if (show_labels) {
            int const available_width = width() - 6;
            QString const label = fm.elidedText(
                    QString::fromStdString(lane.label),
                    Qt::ElideRight,
                    available_width);

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

    // --- Insertion-line marker (shown during drag) ---
    if (_drag_active) {
        int const marker_px = ndcToPixelY(insertSlotNdcY(_current_insert_slot));
        painter.setPen(QPen(QColor(100, 180, 255), 2));
        painter.drawLine(0, marker_px, width() - 1, marker_px);
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

void MultiLaneVerticalAxisWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton || !_state) {
        QWidget::mousePressEvent(event);
        return;
    }

    int const hit_idx = laneIndexAtPixelY(event->pos().y());
    if (hit_idx < 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    auto const & lanes = _state->lanes();
    _dragged_lane_ndc_index = hit_idx;
    _drag_source_lane_id = QString::fromStdString(lanes[static_cast<size_t>(hit_idx)].lane_id);
    _drag_current_pos = event->pos();

    int const n = static_cast<int>(lanes.size());
    // Convert NDC index to visual slot: visual = n-1 - ndc_idx
    int const visual_idx = n - 1 - hit_idx;
    _current_insert_slot = visual_idx;
    _drag_active = true;

    // Notify OpenGL canvas to draw the overlay
    emit laneDragOverlayChanged(true,
                                lanes[static_cast<size_t>(hit_idx)].y_center,
                                lanes[static_cast<size_t>(hit_idx)].y_extent,
                                insertSlotNdcY(_current_insert_slot));

    setFocus();
    update();
    event->accept();
}

void MultiLaneVerticalAxisWidget::mouseMoveEvent(QMouseEvent * event) {
    if (!_drag_active) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    _drag_current_pos = event->pos();
    _current_insert_slot = insertSlotAtPixelY(event->pos().y());

    // Notify OpenGL canvas of updated marker position
    auto const & lanes = _state->lanes();
    emit laneDragOverlayChanged(true,
                                lanes[static_cast<size_t>(_dragged_lane_ndc_index)].y_center,
                                lanes[static_cast<size_t>(_dragged_lane_ndc_index)].y_extent,
                                insertSlotNdcY(_current_insert_slot));

    update();
    event->accept();
}

void MultiLaneVerticalAxisWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton || !_drag_active) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    QString const source_id = _drag_source_lane_id;
    int const target_slot = _current_insert_slot;
    resetDragState();
    update();

    emit laneReorderRequested(source_id, target_slot);
    event->accept();
}

void MultiLaneVerticalAxisWidget::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape && _drag_active) {
        resetDragState();
        update();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

int MultiLaneVerticalAxisWidget::laneIndexAtPixelY(int pixel_y) const {
    if (!_state || _state->lanes().empty()) {
        return -1;
    }

    auto const & lanes = _state->lanes();
    for (int i = 0; i < static_cast<int>(lanes.size()); ++i) {
        auto const & lane = lanes[static_cast<size_t>(i)];
        float const half_extent = lane.y_extent / 2.0f;
        int const top_px = ndcToPixelY(lane.y_center + half_extent);
        int const bottom_px = ndcToPixelY(lane.y_center - half_extent);
        // top_px < bottom_px in pixel space (NDC y increases upward, pixel y increases downward)
        if (pixel_y >= top_px && pixel_y <= bottom_px) {
            return i;
        }
    }
    return -1;
}

int MultiLaneVerticalAxisWidget::insertSlotAtPixelY(int pixel_y) const {
    if (!_state || _state->lanes().empty()) {
        return 0;
    }

    auto const & lanes = _state->lanes();
    int const n = static_cast<int>(lanes.size());

    // Lanes are sorted bottom-to-top (NDC ascending); visual order is top-to-bottom.
    // visual[i] corresponds to NDC index (n-1-i).
    // Divide at the center pixel of each visual lane: if cursor is above the center
    // of visual lane i, the insert slot is i; otherwise it is i+1.
    for (int visual = 0; visual < n; ++visual) {
        int const ndc_idx = n - 1 - visual;
        int const center_px = ndcToPixelY(lanes[static_cast<size_t>(ndc_idx)].y_center);
        if (pixel_y <= center_px) {
            return visual;
        }
    }
    return n;
}

float MultiLaneVerticalAxisWidget::insertSlotNdcY(int slot) const {
    auto const & lanes = _state->lanes();
    int const n = static_cast<int>(lanes.size());
    if (slot <= 0) {
        // Above topmost visual lane = top boundary of NDC index n-1
        return lanes[static_cast<size_t>(n - 1)].y_center + lanes[static_cast<size_t>(n - 1)].y_extent / 2.0f;
    }
    if (slot >= n) {
        // Below bottommost visual lane = bottom boundary of NDC index 0
        return lanes[0].y_center - lanes[0].y_extent / 2.0f;
    }
    // Top boundary of visual lane `slot` = top of NDC index (n-1-slot)
    int const ndc_idx = n - 1 - slot;
    return lanes[static_cast<size_t>(ndc_idx)].y_center + lanes[static_cast<size_t>(ndc_idx)].y_extent / 2.0f;
}

void MultiLaneVerticalAxisWidget::resetDragState() {
    _drag_active = false;
    _dragged_lane_ndc_index = -1;
    _drag_source_lane_id.clear();
    _current_insert_slot = 0;
    _drag_current_pos = QPoint();
    emit laneDragOverlayChanged(false, 0.0f, 0.0f, 0.0f);
}
