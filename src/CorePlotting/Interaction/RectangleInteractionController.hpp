#ifndef COREPLOTTING_INTERACTION_RECTANGLEINTERACTIONCONTROLLER_HPP
#define COREPLOTTING_INTERACTION_RECTANGLEINTERACTIONCONTROLLER_HPP

#include "Entity/EntityTypes.hpp"
#include "GlyphPreview.hpp"
#include "IGlyphInteractionController.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace CorePlotting::Interaction {

/**
 * @brief Configuration for rectangle interaction behavior
 */
struct RectangleInteractionConfig {
    /// Minimum rectangle width in canvas pixels
    float min_width{1.0f};

    /// Minimum rectangle height in canvas pixels (0 = use full viewport height)
    float min_height{0.0f};

    /// If true, height is fixed to span the full canvas (interval mode)
    /// If false, user can drag to define both width and height
    bool constrain_to_x_axis{true};

    /// Viewport height for full-height mode (set by widget)
    float viewport_height{100.0f};

    /// Whether edge dragging for modification is supported
    bool allow_edge_drag{true};

    /// Default fill color for preview
    glm::vec4 fill_color{1.0f, 1.0f, 1.0f, 0.3f};

    /// Default stroke color for preview
    glm::vec4 stroke_color{1.0f, 1.0f, 1.0f, 1.0f};

    /// Stroke width in pixels
    float stroke_width{2.0f};
};

/**
 * @brief Which edge of a rectangle is being dragged (for modification)
 */
enum class RectangleEdge {
    None, ///< No edge (creating new rectangle)
    Left, ///< Left edge
    Right,///< Right edge
    Top,  ///< Top edge
    Bottom///< Bottom edge
};

/**
 * @brief Controller for interactive rectangle creation and modification
 * 
 * Supports two modes:
 * 
 * **Creation Mode** (via `start()`):
 * - User clicks to set first corner
 * - Drags to set opposite corner
 * - Rectangle preview updates during drag
 * - Complete on mouse release
 * 
 * **Modification Mode** (via `startEdgeDrag()`):
 * - Used when user clicks on an edge of an existing rectangle
 * - Only the specified edge moves during drag
 * - Shows ghost of original position
 * 
 * **Interval Mode** (`constrain_to_x_axis = true`):
 * - Rectangle height spans full canvas
 * - Only X coordinates matter (for DigitalIntervalSeries)
 * 
 * **Selection Box Mode** (`constrain_to_x_axis = false`):
 * - User defines both width and height
 * - For rectangular selection regions
 * 
 * @see IGlyphInteractionController for the interface contract
 */
class RectangleInteractionController : public IGlyphInteractionController {
public:
    /**
     * @brief Construct with default configuration
     */
    RectangleInteractionController() = default;

    /**
     * @brief Construct with custom configuration
     */
    explicit RectangleInteractionController(RectangleInteractionConfig config)
        : _config(std::move(config)) {}

    /**
     * @brief Set configuration
     */
    void setConfig(RectangleInteractionConfig config) {
        _config = std::move(config);
    }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] RectangleInteractionConfig const & getConfig() const {
        return _config;
    }

    /**
     * @brief Update viewport height (call when widget resizes)
     */
    void setViewportHeight(float height) {
        _config.viewport_height = height;
    }

    // ========================================================================
    // Edge Drag Mode (for modification)
    // ========================================================================

    /**
     * @brief Start edge drag mode for modifying an existing rectangle
     * 
     * Called when the user clicks on an edge of an existing rectangle.
     * The widget should pre-process the hit test result and provide:
     * - Which edge was hit
     * - The original bounds of the rectangle
     * - The EntityId of the rectangle being modified
     * 
     * @param canvas_x Starting X coordinate in canvas pixels
     * @param canvas_y Starting Y coordinate in canvas pixels
     * @param series_key Identifier of the series containing the rectangle
     * @param entity_id EntityId of the rectangle being modified
     * @param edge Which edge is being dragged
     * @param original_bounds Original rectangle {x, y, width, height} in canvas coords
     */
    void startEdgeDrag(float canvas_x, float canvas_y,
                       std::string series_key,
                       EntityId entity_id,
                       RectangleEdge edge,
                       glm::vec4 original_bounds);

    // ========================================================================
    // IGlyphInteractionController Interface
    // ========================================================================

    void start(float canvas_x, float canvas_y,
               std::string series_key,
               std::optional<EntityId> existing_entity_id = std::nullopt) override;

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

    // ========================================================================
    // Additional Query Methods
    // ========================================================================

    /**
     * @brief Get the edge being dragged (if in edge drag mode)
     */
    [[nodiscard]] RectangleEdge getDraggedEdge() const {
        return _dragged_edge;
    }

    /**
     * @brief Check if this is an edge drag (modification) vs creation
     */
    [[nodiscard]] bool isEdgeDrag() const {
        return _dragged_edge != RectangleEdge::None;
    }

    /**
     * @brief Get current rectangle bounds in canvas coordinates
     * @return {x, y, width, height} where (x,y) is top-left
     */
    [[nodiscard]] glm::vec4 getCurrentBounds() const {
        return _current_bounds;
    }

    /**
     * @brief Get original rectangle bounds (for modification mode)
     */
    [[nodiscard]] std::optional<glm::vec4> getOriginalBounds() const {
        return _original_bounds;
    }

private:
    RectangleInteractionConfig _config;

    // State
    bool _is_active{false};
    std::string _series_key;
    std::optional<EntityId> _entity_id;

    // Geometry (canvas coordinates)
    glm::vec2 _start_point{0.0f};             // Initial click position
    glm::vec4 _current_bounds{0.0f};          // {x, y, width, height}
    std::optional<glm::vec4> _original_bounds;// For modification mode

    // Edge drag state
    RectangleEdge _dragged_edge{RectangleEdge::None};

    // Helpers
    void updateBoundsFromCorners(glm::vec2 corner1, glm::vec2 corner2);
    void updateBoundsFromEdgeDrag(float canvas_x, float canvas_y);
    void applyConstraints();
};

}// namespace CorePlotting::Interaction

#endif// COREPLOTTING_INTERACTION_RECTANGLEINTERACTIONCONTROLLER_HPP
