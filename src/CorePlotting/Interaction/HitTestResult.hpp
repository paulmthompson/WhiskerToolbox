#ifndef COREPLOTTING_INTERACTION_HITTESTRESULT_HPP
#define COREPLOTTING_INTERACTION_HITTESTRESULT_HPP

#include "Entity/EntityTypes.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace CorePlotting {

/**
 * @brief Types of hit targets that can be detected
 * 
 * Categorizes the different kinds of elements that can be clicked/hovered
 * in a time-series plot. The hit type determines what actions are available
 * (e.g., interval edges can be dragged, events can be selected).
 */
enum class HitType {
    None,              ///< No hit detected
    DigitalEvent,      ///< Hit a discrete event marker (has EntityId)
    IntervalBody,      ///< Hit inside an interval (has EntityId)
    IntervalEdgeLeft,  ///< Hit the left edge of an interval (for dragging)
    IntervalEdgeRight, ///< Hit the right edge of an interval (for dragging)
    AnalogSeries,      ///< Hit within an analog series region (no EntityId)
    Point,             ///< Hit a point marker (has EntityId)
    PolyLine,          ///< Hit a polyline segment (may have EntityId)
    SeriesRegion       ///< Hit within a series' allocated region but not on data
};

/**
 * @brief Result of a hit test query
 * 
 * Contains all information about what (if anything) was hit at a given
 * screen/world position. This struct is returned by SceneHitTester and
 * used by widgets to determine appropriate responses to mouse events.
 * 
 * Design philosophy:
 * - `entity_id` is present for discrete data (events, intervals, points)
 * - `entity_id` is absent for continuous data (analog series regions)
 * - `series_key` identifies which series was hit (always present if hit_type != None)
 * - `distance` allows selecting the closest hit when multiple candidates exist
 * 
 * @see SceneHitTester for the query interface
 */
struct HitTestResult {
    /// Type of element that was hit
    HitType hit_type{HitType::None};
    
    /// Series key (empty if no hit)
    std::string series_key;
    
    /// EntityId if the hit target has one (events, intervals, points)
    std::optional<EntityId> entity_id;
    
    /// World-space distance from query point to hit target
    /// Lower values indicate more precise hits
    float distance{std::numeric_limits<float>::max()};
    
    /// World X coordinate of the hit target (for events: time, for intervals: edge position)
    float world_x{0.0f};
    
    /// World Y coordinate of the hit target
    float world_y{0.0f};
    
    /// For intervals: the start time of the interval
    std::optional<int64_t> interval_start;
    
    /// For intervals: the end time of the interval  
    std::optional<int64_t> interval_end;
    
    /**
     * @brief Default constructor creates a "no hit" result
     */
    HitTestResult() = default;
    
    /**
     * @brief Check if anything was hit
     * @return true if hit_type is not None
     */
    [[nodiscard]] bool hasHit() const {
        return hit_type != HitType::None;
    }
    
    /**
     * @brief Check if the hit target has an EntityId
     * @return true if entity_id contains a value
     */
    [[nodiscard]] bool hasEntityId() const {
        return entity_id.has_value();
    }
    
    /**
     * @brief Check if this is an interval hit (body or edge)
     * @return true if hit_type is IntervalBody, IntervalEdgeLeft, or IntervalEdgeRight
     */
    [[nodiscard]] bool isIntervalHit() const {
        return hit_type == HitType::IntervalBody ||
               hit_type == HitType::IntervalEdgeLeft ||
               hit_type == HitType::IntervalEdgeRight;
    }
    
    /**
     * @brief Check if this is an interval edge hit
     * @return true if hit_type is IntervalEdgeLeft or IntervalEdgeRight
     */
    [[nodiscard]] bool isIntervalEdge() const {
        return hit_type == HitType::IntervalEdgeLeft ||
               hit_type == HitType::IntervalEdgeRight;
    }
    
    /**
     * @brief Check if this is a discrete/clickable element (event, interval, point)
     * @return true if the hit type represents a discrete element
     */
    [[nodiscard]] bool isDiscrete() const {
        return hit_type == HitType::DigitalEvent ||
               hit_type == HitType::IntervalBody ||
               hit_type == HitType::IntervalEdgeLeft ||
               hit_type == HitType::IntervalEdgeRight ||
               hit_type == HitType::Point;
    }
    
    /**
     * @brief Compare hit results by distance (for finding closest hit)
     */
    [[nodiscard]] bool isCloserThan(HitTestResult const& other) const {
        return distance < other.distance;
    }
    
    // ========== Factory methods for common hit types ==========
    
    /**
     * @brief Create a "no hit" result
     */
    static HitTestResult noHit() {
        return HitTestResult{};
    }
    
    /**
     * @brief Create a digital event hit result
     * @param key Series key
     * @param id Entity ID of the event
     * @param dist Distance from query point
     * @param x World X coordinate (time) of the event
     * @param y World Y coordinate of the event
     */
    static HitTestResult eventHit(
        std::string key, 
        EntityId id, 
        float dist, 
        float x, 
        float y)
    {
        HitTestResult result;
        result.hit_type = HitType::DigitalEvent;
        result.series_key = std::move(key);
        result.entity_id = id;
        result.distance = dist;
        result.world_x = x;
        result.world_y = y;
        return result;
    }
    
    /**
     * @brief Create an interval body hit result
     * @param key Series key
     * @param id Entity ID of the interval
     * @param start Interval start time
     * @param end Interval end time
     * @param dist Distance from query point
     */
    static HitTestResult intervalBodyHit(
        std::string key,
        EntityId id,
        int64_t start,
        int64_t end,
        float dist)
    {
        HitTestResult result;
        result.hit_type = HitType::IntervalBody;
        result.series_key = std::move(key);
        result.entity_id = id;
        result.distance = dist;
        result.interval_start = start;
        result.interval_end = end;
        return result;
    }
    
    /**
     * @brief Create an interval edge hit result
     * @param key Series key
     * @param id Entity ID of the interval
     * @param is_left_edge true for left edge, false for right edge
     * @param start Interval start time
     * @param end Interval end time
     * @param edge_time Time coordinate of the edge
     * @param dist Distance from query point
     */
    static HitTestResult intervalEdgeHit(
        std::string key,
        EntityId id,
        bool is_left_edge,
        int64_t start,
        int64_t end,
        float edge_time,
        float dist)
    {
        HitTestResult result;
        result.hit_type = is_left_edge ? HitType::IntervalEdgeLeft : HitType::IntervalEdgeRight;
        result.series_key = std::move(key);
        result.entity_id = id;
        result.distance = dist;
        result.world_x = edge_time;
        result.interval_start = start;
        result.interval_end = end;
        return result;
    }
    
    /**
     * @brief Create an analog series region hit result
     * @param key Series key
     * @param x World X coordinate
     * @param y World Y coordinate
     * @param dist Distance from series center (or 0 if within bounds)
     */
    static HitTestResult analogSeriesHit(
        std::string key,
        float x,
        float y,
        float dist = 0.0f)
    {
        HitTestResult result;
        result.hit_type = HitType::AnalogSeries;
        result.series_key = std::move(key);
        result.distance = dist;
        result.world_x = x;
        result.world_y = y;
        // No entity_id for analog series - it's continuous data
        return result;
    }
    
    /**
     * @brief Create a point hit result
     * @param key Series key
     * @param id Entity ID of the point
     * @param x World X coordinate
     * @param y World Y coordinate
     * @param dist Distance from query point
     */
    static HitTestResult pointHit(
        std::string key,
        EntityId id,
        float x,
        float y,
        float dist)
    {
        HitTestResult result;
        result.hit_type = HitType::Point;
        result.series_key = std::move(key);
        result.entity_id = id;
        result.distance = dist;
        result.world_x = x;
        result.world_y = y;
        return result;
    }
};

} // namespace CorePlotting

#endif // COREPLOTTING_INTERACTION_HITTESTRESULT_HPP
