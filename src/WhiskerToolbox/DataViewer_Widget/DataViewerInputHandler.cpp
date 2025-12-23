#include "DataViewerInputHandler.hpp"

#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"

#include <QMouseEvent>
#include <QWidget>

#include <iostream>

namespace DataViewer {

DataViewerInputHandler::DataViewerInputHandler(QWidget * parent)
    : QObject(parent),
      _parent_widget(parent) {
}

void DataViewerInputHandler::setContext(InputContext const & ctx) {
    _ctx = ctx;
}

bool DataViewerInputHandler::handleMousePress(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton) {
        return false;
    }

    float const canvas_x = static_cast<float>(event->pos().x());
    float const canvas_y = static_cast<float>(event->pos().y());

    // Check if we're clicking near an interval edge for dragging (only for selected intervals)
    auto edge_result = findIntervalEdgeAtPosition(canvas_x, canvas_y);
    if (edge_result.isIntervalEdge()) {
        emit intervalEdgeDragRequested(edge_result);
        return true;// Don't start panning when dragging intervals
    }

    // Perform hit testing for interval body selection
    auto hit_result = hitTestAtPosition(canvas_x, canvas_y);

    if (hit_result.hasHit() && hit_result.hit_type == CorePlotting::HitType::IntervalBody) {
        if (hit_result.hasEntityId()) {
            EntityId const hit_entity = hit_result.entity_id.value();
            bool const ctrl_pressed = (event->modifiers() & Qt::ControlModifier) != 0;
            emit entityClicked(hit_entity, ctrl_pressed);
            return true;// Don't start panning when selecting intervals
        }
    }

    // Start panning
    _is_panning = true;
    _last_mouse_pos = event->pos();
    emit panStarted();

    // Emit click coordinates
    float const time_coord = canvasXToTime(canvas_x);
    QString const series_info = buildSeriesInfoString(canvas_x, canvas_y);
    emit clicked(time_coord, canvas_y, series_info);

    return true;
}

bool DataViewerInputHandler::handleMouseMove(QMouseEvent * event) {
    // If interaction is active, let the interaction manager handle it
    if (_interaction_active) {
        return false;
    }

    float const canvas_x = static_cast<float>(event->pos().x());
    float const canvas_y = static_cast<float>(event->pos().y());

    if (_is_panning) {
        // Calculate vertical movement in pixels
        int const deltaY = event->pos().y() - _last_mouse_pos.y();

        // Convert to normalized device coordinates
        // A positive deltaY (moving down) should move the view up
        float const normalized_dy = -1.0f * static_cast<float>(deltaY) /
                                    static_cast<float>(_ctx.widget_height) * 2.0f;

        emit panDelta(normalized_dy);

        _last_mouse_pos = event->pos();
        emit tooltipCancelled();
        emit repaintRequested();
    } else {
        // Check for cursor changes when hovering near interval edges
        auto edge_result = findIntervalEdgeAtPosition(canvas_x, canvas_y);
        if (edge_result.isIntervalEdge()) {
            emit cursorChangeRequested(Qt::SizeHorCursor);
            emit tooltipCancelled();
            emit intervalEdgeHovered(true);
        } else {
            emit cursorChangeRequested(Qt::ArrowCursor);
            emit intervalEdgeHovered(false);
            // Start tooltip timer for series info
            emit tooltipRequested(event->pos());
        }
    }

    // Emit hover coordinates for coordinate display
    float const time_coord = canvasXToTime(canvas_x);
    QString const series_info = buildSeriesInfoString(canvas_x, canvas_y);
    emit hoverCoordinates(time_coord, canvas_y, series_info);

    return true;
}

bool DataViewerInputHandler::handleMouseRelease(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton) {
        return false;
    }

    if (_is_panning) {
        _is_panning = false;
        emit panEnded();
        return true;
    }

    return false;
}

bool DataViewerInputHandler::handleDoubleClick(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton) {
        return false;
    }

    // Find which digital interval series (if any) is at this position
    // For now, emit signal and let widget determine the series
    emit intervalCreationRequested(QString(), event->pos());
    return true;
}

void DataViewerInputHandler::handleLeave() {
    emit tooltipCancelled();
}

float DataViewerInputHandler::canvasXToTime(float canvas_x) const {
    if (!_ctx.view_state) {
        return 0.0f;
    }

    CorePlotting::TimeAxisParams params(
            _ctx.view_state->time_start,
            _ctx.view_state->time_end,
            _ctx.widget_width);
    return CorePlotting::canvasXToTime(canvas_x, params);
}

CorePlotting::HitTestResult DataViewerInputHandler::findIntervalEdgeAtPosition(
        float canvas_x, float canvas_y) const {

    if (!_ctx.view_state || !_ctx.scene || !_ctx.selected_entities || !_ctx.rectangle_batch_key_map) {
        return CorePlotting::HitTestResult::noHit();
    }

    // If we have no cached scene yet or no selection, nothing to check
    if (_ctx.scene->rectangle_batches.empty() && _ctx.selected_entities->empty()) {
        return CorePlotting::HitTestResult::noHit();
    }

    // Use CorePlotting time axis utilities for coordinate conversion
    CorePlotting::TimeAxisParams const time_params(
            _ctx.view_state->time_start,
            _ctx.view_state->time_end,
            _ctx.widget_width);

    // Convert canvas position to time (world X coordinate)
    float const world_x = CorePlotting::canvasXToTime(canvas_x, time_params);

    // Configure hit tester with edge tolerance in world units
    constexpr float EDGE_TOLERANCE_PX = 10.0f;
    float const time_per_pixel = static_cast<float>(time_params.getTimeSpan()) /
                                 static_cast<float>(time_params.viewport_width_px);
    float const edge_tolerance = EDGE_TOLERANCE_PX * time_per_pixel;

    CorePlotting::HitTestConfig config;
    config.edge_tolerance = edge_tolerance;
    config.point_tolerance = edge_tolerance;

    CorePlotting::SceneHitTester tester(config);

    // Use EntityId-based hit testing for interval edges
    static_cast<void>(canvas_y);// Y not used for edge detection
    return tester.findIntervalEdgeByEntityId(
            world_x,
            *_ctx.scene,
            *_ctx.selected_entities,
            *_ctx.rectangle_batch_key_map);
}

CorePlotting::HitTestResult DataViewerInputHandler::hitTestAtPosition(
        float canvas_x, float canvas_y) const {

    if (!_ctx.view_state || !_ctx.scene || !_ctx.rectangle_batch_key_map) {
        return CorePlotting::HitTestResult::noHit();
    }

    // If we have no cached scene yet, return no hit
    if (_ctx.scene->rectangle_batches.empty() && _ctx.scene->glyph_batches.empty()) {
        return CorePlotting::HitTestResult::noHit();
    }

    // Use CorePlotting time axis utilities for coordinate conversion
    CorePlotting::TimeAxisParams const time_params(
            _ctx.view_state->time_start,
            _ctx.view_state->time_end,
            _ctx.widget_width);

    // Convert canvas position to world coordinates
    float const world_x = CorePlotting::canvasXToTime(canvas_x, time_params);

    // Convert canvas Y to world Y
    CorePlotting::YAxisParams const y_params(
            _ctx.view_state->y_min,
            _ctx.view_state->y_max,
            _ctx.widget_height,
            _ctx.view_state->vertical_pan_offset);
    float const world_y = CorePlotting::canvasYToWorldY(canvas_y, y_params);

    // Configure hit tester with appropriate tolerances
    constexpr float TOLERANCE_PX = 10.0f;
    float const time_per_pixel = static_cast<float>(time_params.getTimeSpan()) /
                                 static_cast<float>(time_params.viewport_width_px);
    float const tolerance = TOLERANCE_PX * time_per_pixel;

    CorePlotting::HitTestConfig config;
    config.edge_tolerance = tolerance;
    config.point_tolerance = tolerance;
    config.prioritize_discrete = true;

    CorePlotting::SceneHitTester tester(config);

    // Check for intervals (body hits)
    CorePlotting::HitTestResult result = tester.queryIntervals(
            world_x,
            world_y,
            *_ctx.scene,
            *_ctx.rectangle_batch_key_map);

    // If we got an interval body hit, return it
    if (result.hasHit() && result.hit_type == CorePlotting::HitType::IntervalBody) {
        return result;
    }

    return CorePlotting::HitTestResult::noHit();
}

QString DataViewerInputHandler::buildSeriesInfoString(float canvas_x, float canvas_y) const {
    if (!_series_info_callback || !_analog_value_callback) {
        return QString();
    }

    auto series_info = _series_info_callback(canvas_x, canvas_y);
    if (series_info.has_value()) {
        auto const & [series_type, series_key] = series_info.value();
        if (series_type == "Analog") {
            float const analog_value = _analog_value_callback(canvas_y, series_key);
            return QString("Series: %1, Value: %2")
                    .arg(QString::fromStdString(series_key))
                    .arg(analog_value, 0, 'f', 3);
        }
    }

    return QString();
}

}// namespace DataViewer
