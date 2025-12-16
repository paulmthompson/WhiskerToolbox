#ifndef COREPLOTTING_MAPPERS_MAPPERCONCEPTS_HPP
#define COREPLOTTING_MAPPERS_MAPPERCONCEPTS_HPP

#include "MappedElement.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>

namespace CorePlotting {

// ============================================================================
// Core Mapper Element Concepts
// ============================================================================

/**
 * @brief Concept for types convertible to a mapped discrete element
 * 
 * Requires x, y coordinates and entity_id.
 */
template<typename T>
concept MappedElementLike = requires(T const & elem) {
    { elem.x } -> std::convertible_to<float>;
    { elem.y } -> std::convertible_to<float>;
    { elem.entity_id } -> std::convertible_to<EntityId>;
};

/**
 * @brief Concept for types convertible to a mapped rectangle
 * 
 * Requires x, y, width, height and entity_id.
 */
template<typename T>
concept MappedRectLike = requires(T const & elem) {
    { elem.x } -> std::convertible_to<float>;
    { elem.y } -> std::convertible_to<float>;
    { elem.width } -> std::convertible_to<float>;
    { elem.height } -> std::convertible_to<float>;
    { elem.entity_id } -> std::convertible_to<EntityId>;
};

/**
 * @brief Concept for types convertible to a mapped vertex
 * 
 * Requires x, y coordinates (EntityId tracked per-line, not per-vertex).
 */
template<typename T>
concept MappedVertexLike = requires(T const & elem) {
    { elem.x } -> std::convertible_to<float>;
    { elem.y } -> std::convertible_to<float>;
};

// ============================================================================
// Range Concepts for Mappers
// ============================================================================

/**
 * @brief Concept for ranges that yield discrete elements
 * 
 * Used by SceneBuilder::addGlyphs to accept any range of mapped positions.
 * The range must yield types with (x, y, entity_id).
 */
template<typename R>
concept MappedElementRange = 
    std::ranges::input_range<R> &&
    MappedElementLike<std::ranges::range_value_t<R>>;

/**
 * @brief Concept for ranges that yield rectangles
 * 
 * Used by SceneBuilder::addRectangles to accept any range of mapped bounds.
 */
template<typename R>
concept MappedRectRange = 
    std::ranges::input_range<R> &&
    MappedRectLike<std::ranges::range_value_t<R>>;

/**
 * @brief Concept for ranges that yield vertices
 * 
 * Used for polyline geometry where vertices form a connected line.
 */
template<typename R>
concept MappedVertexRange = 
    std::ranges::input_range<R> &&
    MappedVertexLike<std::ranges::range_value_t<R>>;

// ============================================================================
// Line View Concepts (for multi-line data)
// ============================================================================

/**
 * @brief Concept for a single mapped line view
 * 
 * A line view provides:
 * - entity_id: identification for the whole line
 * - vertices(): a range of MappedVertexLike elements
 * 
 * This allows lazy iteration over line vertices without materializing
 * intermediate vectors.
 */
template<typename T>
concept MappedLineViewLike = requires(T const & view) {
    { view.entity_id } -> std::convertible_to<EntityId>;
    { view.vertices() } -> MappedVertexRange;
};

/**
 * @brief Concept for ranges that yield line views
 * 
 * Used by SceneBuilder::addPolyLines to accept any range of mapped lines.
 * Each element in the range represents one complete polyline.
 */
template<typename R>
concept MappedLineRange = 
    std::ranges::input_range<R> &&
    MappedLineViewLike<std::ranges::range_value_t<R>>;

// ============================================================================
// Helper Concepts for Data Source Detection
// ============================================================================

/**
 * @brief Concept for types that provide a TimeFrame
 * 
 * Used to detect time-series data sources.
 */
template<typename T>
concept HasTimeFrame = requires(T const & obj) {
    { obj.getTimeFrame() } -> std::convertible_to<std::shared_ptr<TimeFrame>>;
};

/**
 * @brief Concept for types that have spatial coordinates (points, lines)
 * 
 * Used to detect spatial data sources that don't need time-based mapping.
 */
template<typename T>
concept IsSpatialData = requires(T const & obj) {
    { obj.getImageSize() };
};

/**
 * @brief Concept for ranged views over time-indexed data
 * 
 * Matches types that can be iterated and yield time-indexed elements.
 */
template<typename T>
concept TimeIndexedRange = std::ranges::input_range<T> &&
    requires(std::ranges::range_value_t<T> elem) {
        // Must have some form of time indexing
        requires requires { elem.event_time; } ||
                 requires { elem.interval; } ||
                 requires { std::get<0>(elem); };
    };

// ============================================================================
// Utility Type Traits
// ============================================================================

/**
 * @brief Check if a type is a valid mapper output type
 */
template<typename T>
inline constexpr bool is_mapped_element_v = MappedElementLike<T>;

template<typename T>
inline constexpr bool is_mapped_rect_v = MappedRectLike<T>;

template<typename T>
inline constexpr bool is_mapped_vertex_v = MappedVertexLike<T>;

/**
 * @brief Check if a range yields mappable elements
 */
template<typename R>
inline constexpr bool is_element_range_v = MappedElementRange<R>;

template<typename R>
inline constexpr bool is_rect_range_v = MappedRectRange<R>;

template<typename R>
inline constexpr bool is_vertex_range_v = MappedVertexRange<R>;

template<typename R>
inline constexpr bool is_line_range_v = MappedLineRange<R>;

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_MAPPERCONCEPTS_HPP
