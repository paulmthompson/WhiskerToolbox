#ifndef COREPLOTTING_INTERACTION_SCENEHITTESTER_HPP
#define COREPLOTTING_INTERACTION_SCENEHITTESTER_HPP

#include "../CoordinateTransform/SeriesCoordinateQuery.hpp"
#include "../Layout/LayoutEngine.hpp"
#include "../SceneGraph/RenderablePrimitives.hpp"
#include "HitTestResult.hpp"

#include <glm/glm.hpp>

#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace CorePlotting {

/**
 * @brief Configuration for hit testing behavior
 */
struct HitTestConfig {
    /// Tolerance in world units for point/event proximity
    float point_tolerance{5.0f};

    /// Tolerance in world units for interval edge detection
    float edge_tolerance{5.0f};

    /// Whether to prioritize discrete elements (events) over regions (analog)
    bool prioritize_discrete{true};

    /// Default configuration with reasonable values
    static HitTestConfig defaultConfig() {
        return HitTestConfig{};
    }
};

/**
 * @brief Multi-strategy hit tester for RenderableScene
 * 
 * This class orchestrates multiple hit testing strategies to determine
 * what element (if any) exists at a given world coordinate. It supports:
 * 
 * 1. **QuadTree queries** for discrete elements (events, points)
 * 2. **Interval containment** for rectangle batches
 * 3. **Series region** queries for analog series
 * 
 * The tester prioritizes hits by distance and type, typically preferring
 * discrete elements over continuous regions.
 * 
 * Usage:
 * @code
 * SceneHitTester tester;
 * 
 * // Convert screen coordinates to world coordinates
 * glm::vec2 world_pos = screenToWorld(mouse_pos, viewport_size, 
 *                                      scene.view_matrix, scene.projection_matrix);
 * 
 * // Perform hit test
 * HitTestResult result = tester.hitTest(world_pos.x, world_pos.y, scene, layout);
 * 
 * if (result.hasHit()) {
 *     if (result.hit_type == HitType::DigitalEvent) {
 *         selectEvent(result.entity_id.value());
 *     }
 * }
 * @endcode
 */
class SceneHitTester {
public:
    /**
     * @brief Construct with default configuration
     */
    SceneHitTester() = default;

    /**
     * @brief Construct with custom configuration
     * @param config Hit testing configuration
     */
    explicit SceneHitTester(HitTestConfig config)
        : _config(std::move(config)) {}

    /**
     * @brief Set hit testing configuration
     */
    void setConfig(HitTestConfig config) {
        _config = std::move(config);
    }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] HitTestConfig const & getConfig() const {
        return _config;
    }

    /**
     * @brief Perform a hit test at the given world coordinates
     * 
     * Queries all applicable strategies and returns the best hit.
     * Priority: discrete elements (events, points) > interval edges > 
     * interval bodies > analog series regions.
     * 
     * @param world_x World X coordinate (typically time)
     * @param world_y World Y coordinate
     * @param scene The renderable scene containing spatial index and batches
     * @param layout Layout information for series positioning
     * @return HitTestResult describing what was hit (or NoHit if nothing)
     */
    [[nodiscard]] HitTestResult hitTest(
            float world_x,
            float world_y,
            RenderableScene const & scene,
            LayoutResponse const & layout) const;

    /**
     * @brief Query only the QuadTree spatial index for discrete elements
     * 
     * Use this when you only care about events/points, not regions.
     * 
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param scene Scene containing the spatial index
     * @return HitTestResult if an element is within tolerance, NoHit otherwise
     */
    [[nodiscard]] HitTestResult queryQuadTree(
            float world_x,
            float world_y,
            RenderableScene const & scene) const;

    /**
     * @brief Query for intervals at a given time coordinate
     * 
     * Checks all rectangle batches for intervals containing the given time.
     * Returns the interval hit with smallest distance to the query point.
     * 
     * @param world_x World X coordinate (time)
     * @param world_y World Y coordinate (used for distance calculation)
     * @param scene Scene containing rectangle batches
     * @param series_key_map Mapping from batch index to series key
     * @return HitTestResult for the closest matching interval
     */
    [[nodiscard]] HitTestResult queryIntervals(
            float world_x,
            float world_y,
            RenderableScene const & scene,
            std::map<size_t, std::string> const & series_key_map) const;

    /**
     * @brief Find interval edge at a given position using EntityId-based selection
     * 
     * Modern version of findIntervalEdge that uses EntityId set for selection state.
     * This integrates with the new selection system where selection is tracked by EntityId
     * rather than time bounds.
     * 
     * @param world_x World X coordinate
     * @param scene Scene containing rectangle batches (with entity_ids populated)
     * @param selected_entities Set of currently selected EntityIds
     * @param series_key_map Mapping from batch index to series key
     * @return HitTestResult with IntervalEdgeLeft/Right if within edge tolerance
     */
    [[nodiscard]] HitTestResult findIntervalEdgeByEntityId(
            float world_x,
            RenderableScene const & scene,
            std::unordered_set<EntityId> const & selected_entities,
            std::map<size_t, std::string> const & series_key_map) const;

    /**
     * @brief Query which series region contains the given point
     * 
     * Use this to determine which analog series the mouse is hovering over.
     * Does not check for discrete elements.
     * 
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @param layout Layout information for series positioning
     * @return HitTestResult with AnalogSeries type if within a series region
     */
    [[nodiscard]] HitTestResult querySeriesRegion(
            float world_x,
            float world_y,
            LayoutResponse const & layout) const;

private:
    HitTestConfig _config;

    // Helper to merge multiple hit results, keeping the best one
    [[nodiscard]] HitTestResult selectBestHit(
            HitTestResult const & a,
            HitTestResult const & b) const;
};

}// namespace CorePlotting

#endif// COREPLOTTING_INTERACTION_SCENEHITTESTER_HPP
