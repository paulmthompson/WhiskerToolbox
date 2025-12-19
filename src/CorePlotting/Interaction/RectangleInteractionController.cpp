
#include "RectangleInteractionController.hpp"

namespace CorePlotting::Interaction {


void RectangleInteractionController::start(
        float canvas_x, float canvas_y,
        std::string series_key,
        std::optional<EntityId> existing_entity_id) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = existing_entity_id;
    _dragged_edge = RectangleEdge::None;
    _original_bounds.reset();

    _start_point = glm::vec2{canvas_x, canvas_y};

    // Initialize with zero-size rectangle at click point
    if (_config.constrain_to_x_axis) {
        // Interval mode: full height
        _current_bounds = glm::vec4{canvas_x, 0.0f, 0.0f, _config.viewport_height};
    } else {
        // Selection box mode: start with zero size
        _current_bounds = glm::vec4{canvas_x, canvas_y, 0.0f, 0.0f};
    }
}

void RectangleInteractionController::startEdgeDrag(
        float canvas_x, float canvas_y,
        std::string series_key,
        EntityId entity_id,
        RectangleEdge edge,
        glm::vec4 original_bounds) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = entity_id;
    _dragged_edge = edge;
    _original_bounds = original_bounds;
    _current_bounds = original_bounds;

    _start_point = glm::vec2{canvas_x, canvas_y};
}

void RectangleInteractionController::update(float canvas_x, float canvas_y) {
    if (!_is_active) return;

    if (_dragged_edge != RectangleEdge::None) {
        // Edge drag mode
        updateBoundsFromEdgeDrag(canvas_x, canvas_y);
    } else {
        // Creation mode
        updateBoundsFromCorners(_start_point, glm::vec2{canvas_x, canvas_y});
    }

    applyConstraints();
}

void RectangleInteractionController::complete() {
    _is_active = false;
    // Preview remains valid for coordinate conversion
}

void RectangleInteractionController::cancel() {
    _is_active = false;

    // Reset to original bounds if we were modifying
    if (_original_bounds.has_value()) {
        _current_bounds = *_original_bounds;
    }

    // Clear state
    _dragged_edge = RectangleEdge::None;
}

GlyphPreview RectangleInteractionController::getPreview() const {
    GlyphPreview preview;
    preview.type = GlyphPreview::Type::Rectangle;
    preview.rectangle = _current_bounds;

    // Styling
    preview.fill_color = _config.fill_color;
    preview.stroke_color = _config.stroke_color;
    preview.stroke_width = _config.stroke_width;

    // Original bounds for ghost rendering (modification mode)
    preview.original_rectangle = _original_bounds;
    preview.show_ghost = _original_bounds.has_value();

    return preview;
}

void RectangleInteractionController::updateBoundsFromCorners(
        glm::vec2 corner1, glm::vec2 corner2) {

    if (_config.constrain_to_x_axis) {
        // Interval mode: only X matters, full height
        float const x = std::min(corner1.x, corner2.x);
        float const width = std::abs(corner2.x - corner1.x);
        _current_bounds = glm::vec4{x, 0.0f, width, _config.viewport_height};
    } else {
        // Selection box mode: both dimensions
        float const x = std::min(corner1.x, corner2.x);
        float const y = std::min(corner1.y, corner2.y);
        float const width = std::abs(corner2.x - corner1.x);
        float const height = std::abs(corner2.y - corner1.y);
        _current_bounds = glm::vec4{x, y, width, height};
    }
}

void RectangleInteractionController::updateBoundsFromEdgeDrag(
        float canvas_x, float canvas_y) {

    if (!_original_bounds.has_value()) return;

    glm::vec4 const & orig = *_original_bounds;
    float const orig_left = orig.x;
    float const orig_top = orig.y;
    float const orig_right = orig.x + orig.z;
    float const orig_bottom = orig.y + orig.w;

    float new_left = orig_left;
    float new_top = orig_top;
    float new_right = orig_right;
    float new_bottom = orig_bottom;

    switch (_dragged_edge) {
        case RectangleEdge::Left:
            new_left = canvas_x;
            // Prevent crossing over
            if (new_left > orig_right - _config.min_width) {
                new_left = orig_right - _config.min_width;
            }
            break;

        case RectangleEdge::Right:
            new_right = canvas_x;
            if (new_right < orig_left + _config.min_width) {
                new_right = orig_left + _config.min_width;
            }
            break;

        case RectangleEdge::Top:
            new_top = canvas_y;
            if (new_top > orig_bottom - _config.min_height) {
                new_top = orig_bottom - _config.min_height;
            }
            break;

        case RectangleEdge::Bottom:
            new_bottom = canvas_y;
            if (new_bottom < orig_top + _config.min_height) {
                new_bottom = orig_top + _config.min_height;
            }
            break;

        case RectangleEdge::None:
            break;
    }

    _current_bounds = glm::vec4{
            new_left,
            new_top,
            new_right - new_left,
            new_bottom - new_top};
}

void RectangleInteractionController::applyConstraints() {
    // Enforce minimum width
    if (_current_bounds.z < _config.min_width) {
        _current_bounds.z = _config.min_width;
    }

    // Enforce minimum height (if not in interval mode)
    if (!_config.constrain_to_x_axis && _current_bounds.w < _config.min_height) {
        _current_bounds.w = _config.min_height;
    }
}


}// namespace CorePlotting::Interaction