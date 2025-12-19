#ifndef COREPLOTTING_INTERACTION_GLYPHPREVIEW_HPP
#define COREPLOTTING_INTERACTION_GLYPHPREVIEW_HPP

#include <glm/glm.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace CorePlotting::Interaction {

/**
 * @brief Preview geometry for rendering during interactive glyph creation/modification
 * 
 * This struct holds the geometric state of an interaction in **canvas coordinates**
 * (pixels). It is produced by IGlyphInteractionController::getPreview() and consumed
 * by PlottingOpenGL::PreviewRenderer.
 * 
 * **Coordinate System**: All positions are in canvas pixels with:
 * - Origin at top-left corner of the viewport
 * - X increasing rightward
 * - Y increasing downward
 * 
 * **Supported Primitive Types**:
 * - Point: Single position (for placing markers)
 * - Line: Two endpoints (for line selection/annotation)
 * - Rectangle: Position + size (for intervals, selection boxes)
 * - Polygon: Arbitrary vertex list (for freeform regions)
 * 
 * **Modification Mode**:
 * When modifying an existing element (vs creating new), the "original" fields
 * contain the element's position before modification, allowing the renderer to
 * show a "ghost" of the original alongside the new position.
 * 
 * @see IGlyphInteractionController for the source of preview data
 * @see PlottingOpenGL::PreviewRenderer for rendering
 */
struct GlyphPreview {
    /**
     * @brief Type of primitive being previewed
     */
    enum class Type {
        None,      ///< No active preview
        Point,     ///< Single point (use `point` field)
        Line,      ///< Line segment (use `line_start`, `line_end` fields)
        Rectangle, ///< Axis-aligned rectangle (use `rectangle` field)
        Polygon    ///< Arbitrary polygon (use `polygon_vertices` field)
    };

    /// Current preview type
    Type type{Type::None};

    // ========================================================================
    // Geometry (Canvas Coordinates)
    // ========================================================================

    /// Point position (for Type::Point)
    glm::vec2 point{0.0f};

    /// Line start point (for Type::Line)
    glm::vec2 line_start{0.0f};
    
    /// Line end point (for Type::Line)
    glm::vec2 line_end{0.0f};

    /// Rectangle as {x, y, width, height} (for Type::Rectangle)
    /// - x, y: Top-left corner in canvas coords
    /// - width, height: Size in pixels (always positive)
    glm::vec4 rectangle{0.0f};

    /// Polygon vertices in order (for Type::Polygon)
    std::vector<glm::vec2> polygon_vertices;

    // ========================================================================
    // Original Geometry (for Modification Mode)
    // ========================================================================

    /// Original rectangle bounds before modification (if modifying existing)
    std::optional<glm::vec4> original_rectangle;

    /// Original line endpoints before modification (if modifying existing)
    std::optional<std::pair<glm::vec2, glm::vec2>> original_line;

    /// Original point position before modification (if modifying existing)
    std::optional<glm::vec2> original_point;

    // ========================================================================
    // Styling
    // ========================================================================

    /// Fill color (RGBA, for rectangles and polygons)
    glm::vec4 fill_color{1.0f, 1.0f, 1.0f, 0.3f};

    /// Stroke/outline color (RGBA)
    glm::vec4 stroke_color{1.0f, 1.0f, 1.0f, 1.0f};

    /// Stroke width in pixels
    float stroke_width{2.0f};

    /// Color for the "ghost" of original geometry (when modifying)
    glm::vec4 ghost_color{0.5f, 0.5f, 0.5f, 0.3f};

    /// Whether to render filled interior (for rectangles/polygons)
    bool show_fill{true};

    /// Whether to render stroke/outline
    bool show_stroke{true};

    /// Whether to show the original position ghost (when modifying)
    bool show_ghost{true};

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /**
     * @brief Check if this preview has any geometry
     */
    [[nodiscard]] bool isValid() const {
        return type != Type::None;
    }

    /**
     * @brief Check if this is a modification (has original geometry)
     */
    [[nodiscard]] bool isModification() const {
        return original_rectangle.has_value() || 
               original_line.has_value() ||
               original_point.has_value();
    }

    /**
     * @brief Clear all geometry and reset to None
     */
    void clear() {
        type = Type::None;
        point = glm::vec2{0.0f};
        line_start = glm::vec2{0.0f};
        line_end = glm::vec2{0.0f};
        rectangle = glm::vec4{0.0f};
        polygon_vertices.clear();
        original_rectangle.reset();
        original_line.reset();
        original_point.reset();
    }

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a point preview
     */
    [[nodiscard]] static GlyphPreview makePoint(glm::vec2 pos) {
        GlyphPreview preview;
        preview.type = Type::Point;
        preview.point = pos;
        return preview;
    }

    /**
     * @brief Create a line preview
     */
    [[nodiscard]] static GlyphPreview makeLine(glm::vec2 start, glm::vec2 end) {
        GlyphPreview preview;
        preview.type = Type::Line;
        preview.line_start = start;
        preview.line_end = end;
        return preview;
    }

    /**
     * @brief Create a rectangle preview
     * @param x Left edge (canvas X)
     * @param y Top edge (canvas Y)
     * @param width Width in pixels
     * @param height Height in pixels
     */
    [[nodiscard]] static GlyphPreview makeRectangle(float x, float y, float width, float height) {
        GlyphPreview preview;
        preview.type = Type::Rectangle;
        preview.rectangle = glm::vec4{x, y, width, height};
        return preview;
    }

    /**
     * @brief Create a rectangle preview from two corner points
     */
    [[nodiscard]] static GlyphPreview makeRectangleFromCorners(glm::vec2 corner1, glm::vec2 corner2) {
        float const x = std::min(corner1.x, corner2.x);
        float const y = std::min(corner1.y, corner2.y);
        float const width = std::abs(corner2.x - corner1.x);
        float const height = std::abs(corner2.y - corner1.y);
        return makeRectangle(x, y, width, height);
    }

    /**
     * @brief Create a polygon preview
     */
    [[nodiscard]] static GlyphPreview makePolygon(std::vector<glm::vec2> vertices) {
        GlyphPreview preview;
        preview.type = Type::Polygon;
        preview.polygon_vertices = std::move(vertices);
        return preview;
    }
};

} // namespace CorePlotting::Interaction

#endif // COREPLOTTING_INTERACTION_GLYPHPREVIEW_HPP