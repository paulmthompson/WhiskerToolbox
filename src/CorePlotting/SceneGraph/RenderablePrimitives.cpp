#include "RenderablePrimitives.hpp"

#include "../CoordinateTransform/InverseTransform.hpp"

#include <algorithm>
#include <cmath>

namespace CorePlotting {

glm::vec2 RenderableScene::canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height) const {
    
    return CorePlotting::canvasToWorld(
        canvas_x, canvas_y,
        viewport_width, viewport_height,
        view_matrix, projection_matrix);
}

Interaction::DataCoordinates RenderableScene::previewToDataCoords(
        Interaction::GlyphPreview const& preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform,
        std::string const& series_key,
        std::optional<EntityId> entity_id) const {
    
    Interaction::DataCoordinates result;
    result.series_key = series_key;
    result.entity_id = entity_id;
    result.is_modification = entity_id.has_value();

    switch (preview.type) {
        case Interaction::GlyphPreview::Type::Rectangle: {
            auto interval = previewToIntervalCoords(preview, viewport_width, viewport_height);
            result.coords = interval;
            break;
        }
        case Interaction::GlyphPreview::Type::Line: {
            auto line = previewToLineCoords(preview, viewport_width, viewport_height, y_transform);
            result.coords = line;
            break;
        }
        case Interaction::GlyphPreview::Type::Point: {
            auto point = previewToPointCoords(preview, viewport_width, viewport_height, y_transform);
            result.coords = point;
            break;
        }
        case Interaction::GlyphPreview::Type::Polygon:
            // TODO: Polygon conversion not yet implemented
            // Would need to convert each vertex
            break;
        case Interaction::GlyphPreview::Type::None:
            // No coordinates to convert
            break;
    }

    return result;
}

Interaction::DataCoordinates::IntervalCoords RenderableScene::previewToIntervalCoords(
        Interaction::GlyphPreview const& rect_preview,
        int viewport_width, int viewport_height) const {
    
    Interaction::DataCoordinates::IntervalCoords result;

    if (rect_preview.type != Interaction::GlyphPreview::Type::Rectangle) {
        return result;
    }

    // Rectangle: {x, y, width, height} in canvas coords
    // For intervals, we only care about X (time) coordinates
    float const left_canvas = rect_preview.rectangle.x;
    float const right_canvas = rect_preview.rectangle.x + rect_preview.rectangle.z;

    // Convert both edges to world X (time)
    glm::vec2 const left_world = canvasToWorld(
        left_canvas, 0.0f, viewport_width, viewport_height);
    glm::vec2 const right_world = canvasToWorld(
        right_canvas, 0.0f, viewport_width, viewport_height);

    // Convert world X to time indices
    int64_t const start_time = worldXToTimeIndex(left_world.x);
    int64_t const end_time = worldXToTimeIndex(right_world.x);

    // Ensure start <= end
    result.start = std::min(start_time, end_time);
    result.end = std::max(start_time, end_time);

    return result;
}

Interaction::DataCoordinates::LineCoords RenderableScene::previewToLineCoords(
        Interaction::GlyphPreview const& line_preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform) const {
    
    Interaction::DataCoordinates::LineCoords result;

    if (line_preview.type != Interaction::GlyphPreview::Type::Line) {
        return result;
    }

    // Convert start point
    glm::vec2 const start_world = canvasToWorld(
        line_preview.line_start.x, line_preview.line_start.y,
        viewport_width, viewport_height);
    
    // Convert end point
    glm::vec2 const end_world = canvasToWorld(
        line_preview.line_end.x, line_preview.line_end.y,
        viewport_width, viewport_height);

    // Apply inverse Y transform to get data-space Y values
    result.x1 = start_world.x;
    result.y1 = y_transform.inverse(start_world.y);
    result.x2 = end_world.x;
    result.y2 = y_transform.inverse(end_world.y);

    return result;
}

Interaction::DataCoordinates::PointCoords RenderableScene::previewToPointCoords(
        Interaction::GlyphPreview const& point_preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform) const {
    
    Interaction::DataCoordinates::PointCoords result;

    if (point_preview.type != Interaction::GlyphPreview::Type::Point) {
        return result;
    }

    // Convert point position
    glm::vec2 const world = canvasToWorld(
        point_preview.point.x, point_preview.point.y,
        viewport_width, viewport_height);

    // Apply inverse Y transform
    result.x = world.x;
    result.y = y_transform.inverse(world.y);

    return result;
}

} // namespace CorePlotting
