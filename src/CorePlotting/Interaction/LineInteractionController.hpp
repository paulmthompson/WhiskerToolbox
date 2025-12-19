#ifndef COREPLOTTING_INTERACTION_LINEINTERACTIONCONTROLLER_HPP
#define COREPLOTTING_INTERACTION_LINEINTERACTIONCONTROLLER_HPP

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
 * @brief Configuration for line interaction behavior
 */
struct LineInteractionConfig {
    /// Minimum line length in canvas pixels
    float min_length{1.0f};

    /// If true, constrain to horizontal or vertical when near axis
    bool snap_to_axis{false};

    /// Angle threshold (degrees) for axis snapping
    float snap_angle_threshold{15.0f};

    /// If true, constrain line to be horizontal (for time-axis selections)
    bool horizontal_only{false};

    /// If true, constrain line to be vertical
    bool vertical_only{false};

    /// Default stroke color for preview
    glm::vec4 stroke_color{1.0f, 0.0f, 0.0f, 1.0f};// Red by default

    /// Stroke width in pixels
    float stroke_width{2.0f};
};

/**
 * @brief Which endpoint of a line is being dragged (for modification)
 */
enum class LineEndpoint {
    None, ///< Neither (creating new line)
    Start,///< Start point
    End   ///< End point
};

/**
 * @brief Controller for interactive line creation and modification
 * 
 * **Creation Mode** (via `start()`):
 * - User clicks to set start point
 * - Drags to set end point
 * - Line preview updates during drag
 * - Complete on mouse release
 * 
 * **Modification Mode** (via `startEndpointDrag()`):
 * - Used when user clicks near an endpoint of an existing line
 * - Only the specified endpoint moves during drag
 * - Shows ghost of original line
 * 
 * **Axis Snapping**:
 * - If `snap_to_axis` is enabled, the line will snap to horizontal/vertical
 *   when within `snap_angle_threshold` degrees of the axis
 * 
 * @see IGlyphInteractionController for the interface contract
 */
class LineInteractionController : public IGlyphInteractionController {
public:
    /**
     * @brief Construct with default configuration
     */
    LineInteractionController() = default;

    /**
     * @brief Construct with custom configuration
     */
    explicit LineInteractionController(LineInteractionConfig config)
        : _config(std::move(config)) {}

    /**
     * @brief Set configuration
     */
    void setConfig(LineInteractionConfig config) {
        _config = std::move(config);
    }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] LineInteractionConfig const & getConfig() const {
        return _config;
    }

    // ========================================================================
    // Endpoint Drag Mode (for modification)
    // ========================================================================

    /**
     * @brief Start endpoint drag mode for modifying an existing line
     * 
     * @param canvas_x Starting X coordinate in canvas pixels
     * @param canvas_y Starting Y coordinate in canvas pixels
     * @param series_key Identifier of the series containing the line
     * @param entity_id EntityId of the line being modified
     * @param endpoint Which endpoint is being dragged
     * @param original_start Original start point in canvas coords
     * @param original_end Original end point in canvas coords
     */
    void startEndpointDrag(float canvas_x, float canvas_y,
                           std::string series_key,
                           EntityId entity_id,
                           LineEndpoint endpoint,
                           glm::vec2 original_start,
                           glm::vec2 original_end);

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
     * @brief Get the endpoint being dragged (if in endpoint drag mode)
     */
    [[nodiscard]] LineEndpoint getDraggedEndpoint() const {
        return _dragged_endpoint;
    }

    /**
     * @brief Check if this is an endpoint drag (modification) vs creation
     */
    [[nodiscard]] bool isEndpointDrag() const {
        return _dragged_endpoint != LineEndpoint::None;
    }

    /**
     * @brief Get current line start point in canvas coordinates
     */
    [[nodiscard]] glm::vec2 getStartPoint() const {
        return _start_point;
    }

    /**
     * @brief Get current line end point in canvas coordinates
     */
    [[nodiscard]] glm::vec2 getEndPoint() const {
        return _end_point;
    }

    /**
     * @brief Get current line length in canvas pixels
     */
    [[nodiscard]] float getLength() const {
        glm::vec2 const delta = _end_point - _start_point;
        return std::sqrt(delta.x * delta.x + delta.y * delta.y);
    }

    /**
     * @brief Get original line points (for modification mode)
     */
    [[nodiscard]] std::optional<std::pair<glm::vec2, glm::vec2>> getOriginalLine() const {
        return _original_line;
    }

private:
    LineInteractionConfig _config;

    // State
    bool _is_active{false};
    std::string _series_key;
    std::optional<EntityId> _entity_id;

    // Geometry (canvas coordinates)
    glm::vec2 _start_point{0.0f};
    glm::vec2 _end_point{0.0f};
    std::optional<std::pair<glm::vec2, glm::vec2>> _original_line;

    // Endpoint drag state
    LineEndpoint _dragged_endpoint{LineEndpoint::None};
    glm::vec2 _fixed_point{0.0f};// The endpoint NOT being dragged

    // Helpers
    [[nodiscard]] glm::vec2 applyConstraints(glm::vec2 point, glm::vec2 anchor) const;
};

}// namespace CorePlotting::Interaction

#endif// COREPLOTTING_INTERACTION_LINEINTERACTIONCONTROLLER_HPP
