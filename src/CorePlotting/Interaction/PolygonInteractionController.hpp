#ifndef COREPLOTTING_INTERACTION_POLYGONINTERACTIONCONTROLLER_HPP
#define COREPLOTTING_INTERACTION_POLYGONINTERACTIONCONTROLLER_HPP

#include "Entity/EntityTypes.hpp"
#include "GlyphPreview.hpp"
#include "IGlyphInteractionController.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace CorePlotting::Interaction {

/**
 * @brief Configuration for polygon interaction behavior
 */
struct PolygonInteractionConfig {
    /// Minimum number of vertices required to complete the polygon
    std::size_t min_vertices{3};

    /// Distance threshold (in canvas pixels) for closing the polygon by clicking near the first vertex
    float close_threshold{15.0f};

    /// If true, allow automatic polygon closure when clicking near the first vertex
    bool allow_click_to_close{true};

    /// Default fill color for preview
    glm::vec4 fill_color{0.2f, 0.6f, 1.0f, 0.3f};

    /// Default stroke color for preview
    glm::vec4 stroke_color{0.2f, 0.6f, 1.0f, 1.0f};

    /// Color for vertices
    glm::vec4 vertex_color{1.0f, 0.0f, 0.0f, 1.0f};

    /// Stroke width in pixels
    float stroke_width{2.0f};

    /// Vertex point size in pixels
    float vertex_size{8.0f};

    /// Color for the closure line (line from last to first vertex during construction)
    glm::vec4 closure_line_color{1.0f, 0.6f, 0.2f, 1.0f};
};

/**
 * @brief Which vertex of a polygon is being dragged (for modification)
 */
struct PolygonVertexHandle {
    /// Index of the vertex being dragged (std::nullopt if none)
    std::optional<std::size_t> vertex_index{std::nullopt};

    /// Check if a vertex is being dragged
    [[nodiscard]] bool isValid() const { return vertex_index.has_value(); }

    /// Get the vertex index (assumes isValid() is true)
    [[nodiscard]] std::size_t index() const { return vertex_index.value(); }
};

/**
 * @brief Result of adding a vertex during polygon creation
 */
enum class AddVertexResult {
    Added,          ///< Vertex was added normally
    ClosedPolygon,  ///< Click was near first vertex, polygon was closed
    TooClose,       ///< Click was too close to an existing vertex, ignored
    NotActive       ///< Controller is not in active creation mode
};

/**
 * @brief Controller for interactive polygon creation and modification
 * 
 * Unlike LineInteractionController and RectangleInteractionController which use
 * drag-based interactions, PolygonInteractionController uses a click-based approach
 * where each click adds a new vertex to the polygon.
 * 
 * **Creation Mode** (via `start()` + repeated `addVertex()` calls):
 * 1. Call `start()` to begin polygon creation (adds first vertex)
 * 2. Call `addVertex()` for each additional vertex
 * 3. Call `complete()` to finalize the polygon (requires min_vertices)
 * 4. Alternatively, click near the first vertex to auto-close (if enabled)
 * 
 * **Modification Mode** (via `startVertexDrag()`):
 * - Used when user clicks on a vertex of an existing polygon
 * - Only the specified vertex moves during drag
 * - Shows ghost of original polygon
 * 
 * **Preview Updates**:
 * - Call `updateCursorPosition()` during mouse move to show a preview line
 *   from the last vertex to the current cursor position
 * 
 * @see IGlyphInteractionController for the interface contract
 * @see PolygonSelectionHandler for the original Qt-based implementation
 */
class PolygonInteractionController : public IGlyphInteractionController {
public:
    /**
     * @brief Construct with default configuration
     */
    PolygonInteractionController() = default;

    /**
     * @brief Construct with custom configuration
     */
    explicit PolygonInteractionController(PolygonInteractionConfig config)
        : _config(std::move(config)) {}

    /**
     * @brief Set configuration
     */
    void setConfig(PolygonInteractionConfig config) {
        _config = std::move(config);
    }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] PolygonInteractionConfig const & getConfig() const {
        return _config;
    }

    // ========================================================================
    // Polygon-Specific Methods (Click-Based Creation)
    // ========================================================================

    /**
     * @brief Add a vertex to the polygon being constructed
     * 
     * This is the primary method for building the polygon during creation mode.
     * Call this on each mouse click after start() has been called.
     * 
     * @param canvas_x X coordinate in canvas pixels
     * @param canvas_y Y coordinate in canvas pixels
     * @return Result indicating what action was taken
     */
    AddVertexResult addVertex(float canvas_x, float canvas_y);

    /**
     * @brief Update the cursor position for preview rendering
     * 
     * Call this during mouse move to show a preview line from the last vertex
     * to the current cursor position. This does NOT add a vertex.
     * 
     * @param canvas_x Current X coordinate in canvas pixels
     * @param canvas_y Current Y coordinate in canvas pixels
     */
    void updateCursorPosition(float canvas_x, float canvas_y);

    /**
     * @brief Check if the polygon can be completed (has enough vertices)
     * @return True if vertex count >= min_vertices
     */
    [[nodiscard]] bool canComplete() const;

    /**
     * @brief Get the number of vertices in the current polygon
     */
    [[nodiscard]] std::size_t getVertexCount() const {
        return _vertices.size();
    }

    /**
     * @brief Get all vertices in the current polygon
     */
    [[nodiscard]] std::vector<glm::vec2> const & getVertices() const {
        return _vertices;
    }

    /**
     * @brief Get the current cursor position (for preview line)
     */
    [[nodiscard]] glm::vec2 getCursorPosition() const {
        return _cursor_position;
    }

    /**
     * @brief Remove the last added vertex (undo)
     * @return True if a vertex was removed, false if polygon has only one vertex
     */
    bool removeLastVertex();

    // ========================================================================
    // Vertex Drag Mode (for modification)
    // ========================================================================

    /**
     * @brief Start vertex drag mode for modifying an existing polygon
     * 
     * @param canvas_x Starting X coordinate in canvas pixels
     * @param canvas_y Starting Y coordinate in canvas pixels
     * @param series_key Identifier of the series containing the polygon
     * @param entity_id EntityId of the polygon being modified
     * @param vertex_index Index of the vertex being dragged
     * @param original_vertices Original polygon vertices in canvas coords
     */
    void startVertexDrag(float canvas_x, float canvas_y,
                         std::string series_key,
                         EntityId entity_id,
                         std::size_t vertex_index,
                         std::vector<glm::vec2> original_vertices);

    /**
     * @brief Get the vertex handle being dragged (if in vertex drag mode)
     */
    [[nodiscard]] PolygonVertexHandle getDraggedVertex() const {
        return _dragged_vertex;
    }

    /**
     * @brief Check if this is a vertex drag (modification) vs creation
     */
    [[nodiscard]] bool isVertexDrag() const {
        return _dragged_vertex.isValid();
    }

    /**
     * @brief Get original polygon vertices (for modification mode)
     */
    [[nodiscard]] std::optional<std::vector<glm::vec2>> const & getOriginalVertices() const {
        return _original_vertices;
    }

    // ========================================================================
    // IGlyphInteractionController Interface
    // ========================================================================

    /**
     * @brief Begin polygon creation at the given position
     * 
     * This adds the first vertex of the polygon. Continue adding vertices
     * with addVertex() calls.
     */
    void start(float canvas_x, float canvas_y,
               std::string series_key,
               std::optional<EntityId> existing_entity_id = std::nullopt) override;

    /**
     * @brief Update during drag (for vertex modification mode)
     * 
     * In creation mode, use updateCursorPosition() instead for preview updates,
     * and addVertex() to add points.
     */
    void update(float canvas_x, float canvas_y) override;

    void complete() override;

    void cancel() override;

    [[nodiscard]] bool isActive() const override {
        return _is_active;
    }

    [[nodiscard]] GlyphPreview getPreview() const override;

    [[nodiscard]] std::string const & getSeriesKey() const override {
        return _series_key;
    }

    [[nodiscard]] std::optional<EntityId> getEntityId() const override {
        return _entity_id;
    }

private:
    PolygonInteractionConfig _config;

    // State
    bool _is_active{false};
    std::string _series_key;
    std::optional<EntityId> _entity_id;

    // Geometry (canvas coordinates)
    std::vector<glm::vec2> _vertices;
    glm::vec2 _cursor_position{0.0f};  // Current cursor for preview line
    std::optional<std::vector<glm::vec2>> _original_vertices;

    // Vertex drag state
    PolygonVertexHandle _dragged_vertex;

    // Helpers
    [[nodiscard]] float distanceToFirstVertex(float x, float y) const;
    [[nodiscard]] bool isNearFirstVertex(float x, float y) const;
};

} // namespace CorePlotting::Interaction

#endif // COREPLOTTING_INTERACTION_POLYGONINTERACTIONCONTROLLER_HPP
