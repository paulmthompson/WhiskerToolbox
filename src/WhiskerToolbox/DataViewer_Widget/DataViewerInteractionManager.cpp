#include "DataViewerInteractionManager.hpp"

#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/Interaction/RectangleInteractionController.hpp"

#include <iostream>

using CorePlotting::Interaction::RectangleEdge;
using CorePlotting::Interaction::RectangleInteractionConfig;
using CorePlotting::Interaction::RectangleInteractionController;

namespace DataViewer {

DataViewerInteractionManager::DataViewerInteractionManager(QObject * parent)
    : QObject(parent) {
}

void DataViewerInteractionManager::setContext(InteractionContext const & ctx) {
    _ctx = ctx;
}

void DataViewerInteractionManager::setMode(InteractionMode mode) {
    if (_mode == mode) {
        return;
    }

    // Cancel any active interaction before switching modes
    cancel();

    _mode = mode;

    // Set cursor based on mode
    switch (mode) {
        case InteractionMode::CreateInterval:
        case InteractionMode::CreateLine:
            emit cursorChangeRequested(Qt::CrossCursor);
            break;
        case InteractionMode::ModifyInterval:
            emit cursorChangeRequested(Qt::SizeHorCursor);
            break;
        case InteractionMode::Normal:
        default:
            emit cursorChangeRequested(Qt::ArrowCursor);
            break;
    }

    emit modeChanged(mode);
}

bool DataViewerInteractionManager::isActive() const {
    return _controller && _controller->isActive();
}

void DataViewerInteractionManager::cancel() {
    if (_controller && _controller->isActive()) {
        _controller->cancel();
    }

    _mode = InteractionMode::Normal;
    _controller = nullptr;
    _series_key.clear();

    emit cursorChangeRequested(Qt::ArrowCursor);
    emit previewUpdated();
}

void DataViewerInteractionManager::startIntervalCreation(
        std::string const & series_key,
        float canvas_x, float canvas_y,
        glm::vec4 const & fill_color,
        glm::vec4 const & stroke_color) {

    // Don't start if we're already in an interaction
    if (isActive()) {
        return;
    }

    // Create the controller with interval mode configuration
    RectangleInteractionConfig config;
    config.constrain_to_x_axis = true;// Intervals span full height
    config.viewport_height = static_cast<float>(_ctx.widget_height);
    config.fill_color = fill_color;
    config.stroke_color = stroke_color;
    config.stroke_width = 2.0f;

    _controller = std::make_unique<RectangleInteractionController>(config);
    _series_key = series_key;
    _mode = InteractionMode::CreateInterval;

    // Start the controller at the click position
    _controller->start(canvas_x, canvas_y, series_key);

    emit cursorChangeRequested(Qt::SizeHorCursor);
    emit modeChanged(_mode);
    emit previewUpdated();

    std::cout << "Started interval creation for series " << series_key
              << " at canvas (" << canvas_x << ", " << canvas_y << ")" << std::endl;
}

void DataViewerInteractionManager::startEdgeDrag(
        CorePlotting::HitTestResult const & hit_result,
        glm::vec4 const & fill_color,
        glm::vec4 const & stroke_color) {

    // Only handle interval edge hits
    if (!hit_result.isIntervalEdge()) {
        return;
    }

    // Don't start if we're already in an interaction
    if (isActive()) {
        return;
    }

    // Get the entity ID (required for modification)
    if (!hit_result.entity_id.has_value()) {
        return;
    }

    // Get interval bounds (required for modification)
    if (!hit_result.interval_start.has_value() || !hit_result.interval_end.has_value()) {
        return;
    }

    // Convert HitType to RectangleEdge
    RectangleEdge edge = RectangleEdge::None;
    if (hit_result.hit_type == CorePlotting::HitType::IntervalEdgeLeft) {
        edge = RectangleEdge::Left;
    } else if (hit_result.hit_type == CorePlotting::HitType::IntervalEdgeRight) {
        edge = RectangleEdge::Right;
    }

    if (edge == RectangleEdge::None) {
        return;
    }

    if (!_ctx.view_state) {
        return;
    }

    // Create the controller with interval mode configuration
    RectangleInteractionConfig config;
    config.constrain_to_x_axis = true;// Intervals span full height
    config.viewport_height = static_cast<float>(_ctx.widget_height);
    config.fill_color = fill_color;
    config.stroke_color = stroke_color;
    config.stroke_width = 2.0f;

    auto controller = std::make_unique<RectangleInteractionController>(config);

    // Convert interval bounds from data space to canvas coordinates
    CorePlotting::TimeAxisParams time_params(
            _ctx.view_state->time_start,
            _ctx.view_state->time_end,
            _ctx.widget_width);
    float const start_canvas_x = CorePlotting::timeToCanvasX(
            static_cast<float>(*hit_result.interval_start), time_params);
    float const end_canvas_x = CorePlotting::timeToCanvasX(
            static_cast<float>(*hit_result.interval_end), time_params);

    // For intervals, y spans full height
    float const canvas_y = 0.0f;
    float const canvas_height = static_cast<float>(_ctx.widget_height);

    // Original bounds: {x, y, width, height} in canvas coords
    glm::vec4 original_bounds(
            start_canvas_x,               // x (left edge)
            canvas_y,                     // y (bottom)
            end_canvas_x - start_canvas_x,// width
            canvas_height                 // height
    );

    // Current canvas position (where user clicked)
    float const click_canvas_x = CorePlotting::timeToCanvasX(hit_result.world_x, time_params);
    float const click_canvas_y = static_cast<float>(_ctx.widget_height) / 2.0f;

    // Start edge drag mode
    controller->startEdgeDrag(
            click_canvas_x, click_canvas_y,
            hit_result.series_key,
            *hit_result.entity_id,
            edge,
            original_bounds);

    _controller = std::move(controller);
    _series_key = hit_result.series_key;
    _mode = InteractionMode::ModifyInterval;

    emit cursorChangeRequested(Qt::SizeHorCursor);
    emit modeChanged(_mode);
    emit previewUpdated();

    bool const is_left_edge = (edge == RectangleEdge::Left);
    std::cout << "Started interval edge drag (" << (is_left_edge ? "left" : "right")
              << ") for series " << hit_result.series_key
              << " interval [" << *hit_result.interval_start << ", " << *hit_result.interval_end << "]"
              << std::endl;
}

void DataViewerInteractionManager::update(float canvas_x, float canvas_y) {
    if (!_controller || !_controller->isActive()) {
        return;
    }

    _controller->update(canvas_x, canvas_y);
    emit previewUpdated();
}

void DataViewerInteractionManager::complete() {
    if (!_controller || !_controller->isActive()) {
        return;
    }

    // Convert preview to data coordinates
    auto data_coords = convertPreviewToDataCoords();

    // Complete the controller interaction
    _controller->complete();

    // Emit the completed coordinates
    emit interactionCompleted(data_coords);

    // Reset state
    setMode(InteractionMode::Normal);
}

std::optional<CorePlotting::Interaction::GlyphPreview> DataViewerInteractionManager::getPreview() const {
    if (!_controller || !_controller->isActive()) {
        return std::nullopt;
    }

    return _controller->getPreview();
}

CorePlotting::Interaction::DataCoordinates DataViewerInteractionManager::convertPreviewToDataCoords() const {
    if (!_controller || !_controller->isActive() || !_ctx.scene) {
        return CorePlotting::Interaction::DataCoordinates{};
    }

    auto const preview = _controller->getPreview();

    if (preview.type == CorePlotting::Interaction::GlyphPreview::Type::Rectangle) {
        // For intervals: just need X coordinates (time)
        auto interval_coords = _ctx.scene->previewToIntervalCoords(
                preview, _ctx.widget_width, _ctx.widget_height);
        return CorePlotting::Interaction::DataCoordinates::createInterval(
                _series_key, interval_coords.start, interval_coords.end);
    } else if (preview.type == CorePlotting::Interaction::GlyphPreview::Type::Line) {
        // For lines: need full coordinate conversion
        CorePlotting::LayoutTransform identity{0.0f, 1.0f};
        auto line_coords = _ctx.scene->previewToLineCoords(
                preview, _ctx.widget_width, _ctx.widget_height, identity);
        return CorePlotting::Interaction::DataCoordinates::createLine(
                _series_key, line_coords.x1, line_coords.y1, line_coords.x2, line_coords.y2);
    } else if (preview.type == CorePlotting::Interaction::GlyphPreview::Type::Point) {
        // For points: single coordinate
        CorePlotting::LayoutTransform identity{0.0f, 1.0f};
        auto point_coords = _ctx.scene->previewToPointCoords(
                preview, _ctx.widget_width, _ctx.widget_height, identity);
        return CorePlotting::Interaction::DataCoordinates::createPoint(
                _series_key, point_coords.x, point_coords.y);
    }

    return CorePlotting::Interaction::DataCoordinates{};
}

}// namespace DataViewer
