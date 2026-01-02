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
     * @brief Find interval edge at a given position
     * 
     * Specialized query for detecting interval edges for drag operations.
     * Only considers currently selected intervals or all intervals if none selected.
     * 
     * @param world_x World X coordinate
     * @param scene Scene containing rectangle batches
     * @param selected_intervals Map of series_key -> (start, end) for selected intervals
     * @param series_key_map Mapping from batch index to series key
     * @return HitTestResult with IntervalEdgeLeft/Right if within edge tolerance
     * 
     * @deprecated Use findIntervalEdgeByEntityId() for EntityId-based selection
     */
    [[nodiscard]] HitTestResult findIntervalEdge(
            float world_x,
            RenderableScene const & scene,
            std::map<std::string, std::pair<int64_t, int64_t>> const & selected_intervals,
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

// ============================================================================
// Implementation (inline for header-only utility)
// ============================================================================

inline HitTestResult SceneHitTester::hitTest(
        float world_x,
        float world_y,
        RenderableScene const & scene,
        LayoutResponse const & layout) const {
    HitTestResult best_result = HitTestResult::noHit();

    // Strategy 1: Query QuadTree for discrete elements (events, points)
    if (scene.spatial_index) {
        HitTestResult quad_result = queryQuadTree(world_x, world_y, scene);
        best_result = selectBestHit(best_result, quad_result);
    }

    // Strategy 2: Query rectangle batches for intervals
    // Note: For a complete implementation, the caller should provide the series_key_map
    // For now, we use a simple index-based approach
    // (In practice, the widget maintains this mapping when building the scene)

    // Strategy 3: Query series regions (always do this as fallback)
    HitTestResult region_result = querySeriesRegion(world_x, world_y, layout);

    // If we have a discrete hit, it takes priority over region hits
    if (_config.prioritize_discrete && best_result.isDiscrete()) {
        return best_result;
    }

    return selectBestHit(best_result, region_result);
}

inline HitTestResult SceneHitTester::queryQuadTree(
        float world_x,
        float world_y,
        RenderableScene const & scene) const {
    if (!scene.spatial_index) {
        return HitTestResult::noHit();
    }

    // Query the QuadTree for the nearest point within tolerance
    auto * nearest = scene.spatial_index->findNearest(world_x, world_y, _config.point_tolerance);

    if (nearest) {
        float dx = nearest->x - world_x;
        float dy = nearest->y - world_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        // Create an event hit result (QuadTree stores data as EntityId)
        return HitTestResult::eventHit(
                "",           // Series key would need to be stored in QuadTree or looked up separately
                nearest->data,// QuadTree<EntityId> stores EntityId in .data
                dist,
                nearest->x,
                nearest->y);
    }

    return HitTestResult::noHit();
}

inline HitTestResult SceneHitTester::queryIntervals(
        float world_x,
        float world_y,
        RenderableScene const & scene,
        std::map<size_t, std::string> const & series_key_map) const {
    HitTestResult best = HitTestResult::noHit();

    for (size_t batch_idx = 0; batch_idx < scene.rectangle_batches.size(); ++batch_idx) {
        auto const & batch = scene.rectangle_batches[batch_idx];

        // Get series key for this batch
        std::string series_key;
        auto key_it = series_key_map.find(batch_idx);
        if (key_it != series_key_map.end()) {
            series_key = key_it->second;
        }

        // Check each rectangle in the batch
        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            auto const & rect = batch.bounds[i];
            float rect_x = rect.x;
            float rect_y = rect.y;
            float rect_w = rect.z;
            float rect_h = rect.w;

            // Check if world_x is within interval time range
            if (world_x >= rect_x && world_x <= (rect_x + rect_w)) {
                // Calculate distance (for Y, check if within height)
                float y_dist = 0.0f;
                if (world_y < rect_y) {
                    y_dist = rect_y - world_y;
                } else if (world_y > rect_y + rect_h) {
                    y_dist = world_y - (rect_y + rect_h);
                }

                float dist = y_dist;// For intervals, X is always inside

                EntityId entity_id{0};
                if (i < batch.entity_ids.size()) {
                    entity_id = batch.entity_ids[i];
                }

                auto result = HitTestResult::intervalBodyHit(
                        series_key,
                        entity_id,
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(rect_x + rect_w),
                        dist);

                best = selectBestHit(best, result);
            }
        }
    }

    return best;
}

inline HitTestResult SceneHitTester::findIntervalEdge(
        float world_x,
        RenderableScene const & scene,
        std::map<std::string, std::pair<int64_t, int64_t>> const & selected_intervals,
        std::map<size_t, std::string> const & series_key_map) const {
    HitTestResult best = HitTestResult::noHit();

    for (size_t batch_idx = 0; batch_idx < scene.rectangle_batches.size(); ++batch_idx) {
        auto const & batch = scene.rectangle_batches[batch_idx];

        // Get series key for this batch
        std::string series_key;
        auto key_it = series_key_map.find(batch_idx);
        if (key_it != series_key_map.end()) {
            series_key = key_it->second;
        }

        // Check if this series has a selected interval
        auto sel_it = selected_intervals.find(series_key);
        if (sel_it == selected_intervals.end() && !selected_intervals.empty()) {
            continue;// Only check selected intervals if any are selected
        }

        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            auto const & rect = batch.bounds[i];
            float rect_x = rect.x;
            float rect_w = rect.z;
            float left_edge = rect_x;
            float right_edge = rect_x + rect_w;

            // If we have selected intervals, only check matching ones
            if (sel_it != selected_intervals.end()) {
                if (static_cast<int64_t>(rect_x) != sel_it->second.first ||
                    static_cast<int64_t>(right_edge) != sel_it->second.second) {
                    continue;
                }
            }

            EntityId entity_id{0};
            if (i < batch.entity_ids.size()) {
                entity_id = batch.entity_ids[i];
            }

            // Check left edge
            float left_dist = std::abs(world_x - left_edge);
            if (left_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        true,// is_left_edge
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        left_edge,
                        left_dist);
                best = selectBestHit(best, result);
            }

            // Check right edge
            float right_dist = std::abs(world_x - right_edge);
            if (right_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        false,// is_left_edge (right edge)
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        right_edge,
                        right_dist);
                best = selectBestHit(best, result);
            }
        }
    }

    return best;
}

inline HitTestResult SceneHitTester::findIntervalEdgeByEntityId(
        float world_x,
        RenderableScene const & scene,
        std::unordered_set<EntityId> const & selected_entities,
        std::map<size_t, std::string> const & series_key_map) const {
    HitTestResult best = HitTestResult::noHit();

    for (size_t batch_idx = 0; batch_idx < scene.rectangle_batches.size(); ++batch_idx) {
        auto const & batch = scene.rectangle_batches[batch_idx];

        // Get series key for this batch
        std::string series_key;
        auto key_it = series_key_map.find(batch_idx);
        if (key_it != series_key_map.end()) {
            series_key = key_it->second;
        }

        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            // Get EntityId for this interval
            EntityId entity_id{0};
            if (i < batch.entity_ids.size()) {
                entity_id = batch.entity_ids[i];
            }

            // Only check edges of selected intervals (skip if not selected)
            if (!selected_entities.empty() && !selected_entities.contains(entity_id)) {
                continue;
            }

            auto const & rect = batch.bounds[i];
            float rect_x = rect.x;
            float rect_w = rect.z;
            float left_edge = rect_x;
            float right_edge = rect_x + rect_w;

            // Check left edge
            float left_dist = std::abs(world_x - left_edge);
            if (left_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        true,// is_left_edge
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        left_edge,
                        left_dist);
                best = selectBestHit(best, result);
            }

            // Check right edge
            float right_dist = std::abs(world_x - right_edge);
            if (right_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        false,// is_left_edge (right edge)
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        right_edge,
                        right_dist);
                best = selectBestHit(best, result);
            }
        }
    }

    return best;
}

inline HitTestResult SceneHitTester::querySeriesRegion(
        float world_x,
        float world_y,
        LayoutResponse const & layout) const {
    auto series_result = findSeriesAtWorldY(world_y, layout);

    if (series_result.has_value()) {
        return HitTestResult::analogSeriesHit(
                series_result->series_key,
                world_x,
                world_y,
                series_result->is_within_bounds ? 0.0f : std::abs(series_result->series_local_y));
    }

    return HitTestResult::noHit();
}

inline HitTestResult SceneHitTester::selectBestHit(
        HitTestResult const & a,
        HitTestResult const & b) const {
    // If one is no-hit, return the other
    if (!a.hasHit()) return b;
    if (!b.hasHit()) return a;

    // Prioritize discrete elements if configured
    if (_config.prioritize_discrete) {
        if (a.isDiscrete() && !b.isDiscrete()) return a;
        if (b.isDiscrete() && !a.isDiscrete()) return b;
    }

    // Prioritize interval edges over bodies (for drag detection)
    if (a.isIntervalEdge() && !b.isIntervalEdge()) return a;
    if (b.isIntervalEdge() && !a.isIntervalEdge()) return b;

    // Otherwise, return the closest one
    return a.isCloserThan(b) ? a : b;
}

}// namespace CorePlotting

#endif// COREPLOTTING_INTERACTION_SCENEHITTESTER_HPP
