
#include "LineInteractionController.hpp"

namespace CorePlotting::Interaction {

void LineInteractionController::start(
        float canvas_x, float canvas_y,
        std::string series_key,
        std::optional<EntityId> existing_entity_id) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = existing_entity_id;
    _dragged_endpoint = LineEndpoint::None;
    _original_line.reset();

    _start_point = glm::vec2{canvas_x, canvas_y};
    _end_point = _start_point;// Start with zero-length line
}

void LineInteractionController::startEndpointDrag(
        float canvas_x, float canvas_y,
        std::string series_key,
        EntityId entity_id,
        LineEndpoint endpoint,
        glm::vec2 original_start,
        glm::vec2 original_end) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = entity_id;
    _dragged_endpoint = endpoint;
    _original_line = std::make_pair(original_start, original_end);

    _start_point = original_start;
    _end_point = original_end;

    // Set the fixed point (the one NOT being dragged)
    if (endpoint == LineEndpoint::Start) {
        _fixed_point = original_end;
    } else {
        _fixed_point = original_start;
    }

    static_cast<void>(canvas_x);
    static_cast<void>(canvas_y);
}

void LineInteractionController::update(float canvas_x, float canvas_y) {
    if (!_is_active) return;

    glm::vec2 const new_point{canvas_x, canvas_y};

    if (_dragged_endpoint == LineEndpoint::None) {
        // Creation mode: start is fixed, end follows cursor
        _end_point = applyConstraints(new_point, _start_point);
    } else if (_dragged_endpoint == LineEndpoint::Start) {
        // Dragging start point
        _start_point = applyConstraints(new_point, _fixed_point);
        _end_point = _fixed_point;
    } else {
        // Dragging end point
        _end_point = applyConstraints(new_point, _fixed_point);
        _start_point = _fixed_point;
    }
}

void LineInteractionController::complete() {
    _is_active = false;
    // Preview remains valid for coordinate conversion
}

void LineInteractionController::cancel() {
    _is_active = false;

    // Reset to original line if we were modifying
    if (_original_line.has_value()) {
        _start_point = _original_line->first;
        _end_point = _original_line->second;
    }

    // Clear state
    _dragged_endpoint = LineEndpoint::None;
}

GlyphPreview LineInteractionController::getPreview() const {
    GlyphPreview preview;
    preview.type = GlyphPreview::Type::Line;
    preview.line_start = _start_point;
    preview.line_end = _end_point;

    // Styling
    preview.stroke_color = _config.stroke_color;
    preview.stroke_width = _config.stroke_width;
    preview.show_fill = false;// Lines don't have fill

    // Original line for ghost rendering (modification mode)
    preview.original_line = _original_line;
    preview.show_ghost = _original_line.has_value();

    return preview;
}

glm::vec2 LineInteractionController::applyConstraints(
        glm::vec2 point, glm::vec2 anchor) const {

    // Horizontal only constraint
    if (_config.horizontal_only) {
        return glm::vec2{point.x, anchor.y};
    }

    // Vertical only constraint
    if (_config.vertical_only) {
        return glm::vec2{anchor.x, point.y};
    }

    // Axis snapping
    if (_config.snap_to_axis) {
        glm::vec2 const delta = point - anchor;
        float const length = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        if (length > 0.001f) {
            // Calculate angle from horizontal
            float const angle_rad = std::atan2(std::abs(delta.y), std::abs(delta.x));
            float const angle_deg = angle_rad * 180.0f / 3.14159265f;

            // Snap to horizontal if close
            if (angle_deg < _config.snap_angle_threshold) {
                return glm::vec2{point.x, anchor.y};
            }

            // Snap to vertical if close
            if (angle_deg > (90.0f - _config.snap_angle_threshold)) {
                return glm::vec2{anchor.x, point.y};
            }
        }
    }

    return point;
}

}// namespace CorePlotting::Interaction