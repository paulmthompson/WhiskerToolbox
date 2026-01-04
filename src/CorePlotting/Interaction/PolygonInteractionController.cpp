#include "PolygonInteractionController.hpp"

namespace CorePlotting::Interaction {

void PolygonInteractionController::start(
        float canvas_x, float canvas_y,
        std::string series_key,
        std::optional<EntityId> existing_entity_id) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = existing_entity_id;
    _dragged_vertex = PolygonVertexHandle{};
    _original_vertices.reset();

    // Clear any existing vertices and add the first one
    _vertices.clear();
    _vertices.emplace_back(canvas_x, canvas_y);
    _cursor_position = glm::vec2{canvas_x, canvas_y};
}

void PolygonInteractionController::startVertexDrag(
        float canvas_x, float canvas_y,
        std::string series_key,
        EntityId entity_id,
        std::size_t vertex_index,
        std::vector<glm::vec2> original_vertices) {

    _is_active = true;
    _series_key = std::move(series_key);
    _entity_id = entity_id;
    _original_vertices = original_vertices;
    _vertices = original_vertices;

    // Set up vertex drag handle
    _dragged_vertex.vertex_index = vertex_index;

    // Initialize cursor to the vertex position
    if (vertex_index < _vertices.size()) {
        _cursor_position = _vertices[vertex_index];
    } else {
        _cursor_position = glm::vec2{canvas_x, canvas_y};
    }

    static_cast<void>(canvas_x);
    static_cast<void>(canvas_y);
}

AddVertexResult PolygonInteractionController::addVertex(float canvas_x, float canvas_y) {
    if (!_is_active) {
        return AddVertexResult::NotActive;
    }

    // In vertex drag mode, addVertex doesn't make sense
    if (_dragged_vertex.isValid()) {
        return AddVertexResult::NotActive;
    }

    // Check if click is near the first vertex (to close the polygon)
    if (_config.allow_click_to_close && _vertices.size() >= _config.min_vertices) {
        if (isNearFirstVertex(canvas_x, canvas_y)) {
            complete();
            return AddVertexResult::ClosedPolygon;
        }
    }

    // Add the new vertex
    _vertices.emplace_back(canvas_x, canvas_y);
    _cursor_position = glm::vec2{canvas_x, canvas_y};

    return AddVertexResult::Added;
}

void PolygonInteractionController::updateCursorPosition(float canvas_x, float canvas_y) {
    if (!_is_active) return;

    _cursor_position = glm::vec2{canvas_x, canvas_y};
}

void PolygonInteractionController::update(float canvas_x, float canvas_y) {
    if (!_is_active) return;

    _cursor_position = glm::vec2{canvas_x, canvas_y};

    // In vertex drag mode, move the dragged vertex
    if (_dragged_vertex.isValid()) {
        std::size_t const idx = _dragged_vertex.index();
        if (idx < _vertices.size()) {
            _vertices[idx] = glm::vec2{canvas_x, canvas_y};
        }
    }
}

bool PolygonInteractionController::canComplete() const {
    return _vertices.size() >= _config.min_vertices;
}

bool PolygonInteractionController::removeLastVertex() {
    if (!_is_active || _vertices.size() <= 1) {
        return false;
    }

    _vertices.pop_back();

    // Update cursor to the new last vertex
    if (!_vertices.empty()) {
        _cursor_position = _vertices.back();
    }

    return true;
}

void PolygonInteractionController::complete() {
    _is_active = false;
    // Preview remains valid for coordinate conversion
    // Clear the dragged vertex state
    _dragged_vertex = PolygonVertexHandle{};
}

void PolygonInteractionController::cancel() {
    _is_active = false;

    // Reset to original vertices if we were modifying
    if (_original_vertices.has_value()) {
        _vertices = _original_vertices.value();
    } else {
        _vertices.clear();
    }

    // Clear state
    _dragged_vertex = PolygonVertexHandle{};
}

GlyphPreview PolygonInteractionController::getPreview() const {
    GlyphPreview preview;
    preview.type = GlyphPreview::Type::Polygon;

    // Copy vertices to preview
    preview.polygon_vertices = _vertices;

    // If we're actively creating (not in vertex drag mode), include cursor position
    // as a "preview" vertex to show the potential next edge
    if (_is_active && !_dragged_vertex.isValid() && !_vertices.empty()) {
        // The cursor position is used by the renderer to draw a preview line
        // from the last vertex to the cursor. We don't add it to polygon_vertices
        // since it's not a committed vertex yet.
        // Instead, renderers should check isActive() and draw the preview line separately.
    }

    // Styling
    preview.fill_color = _config.fill_color;
    preview.stroke_color = _config.stroke_color;
    preview.stroke_width = _config.stroke_width;
    preview.show_fill = (_vertices.size() >= 3);  // Only fill if we have a polygon
    preview.show_stroke = true;

    // Original vertices for ghost rendering (modification mode)
    if (_original_vertices.has_value()) {
        preview.show_ghost = true;
    }

    return preview;
}

float PolygonInteractionController::distanceToFirstVertex(float x, float y) const {
    if (_vertices.empty()) {
        return std::numeric_limits<float>::max();
    }

    float const dx = x - _vertices.front().x;
    float const dy = y - _vertices.front().y;
    return std::sqrt(dx * dx + dy * dy);
}

bool PolygonInteractionController::isNearFirstVertex(float x, float y) const {
    return distanceToFirstVertex(x, y) <= _config.close_threshold;
}

} // namespace CorePlotting::Interaction
