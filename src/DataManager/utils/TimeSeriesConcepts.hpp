#ifndef TIME_SERIES_CONCEPTS_HPP
#define TIME_SERIES_CONCEPTS_HPP

/**
 * @file TimeSeriesConcepts.hpp
 * @brief C++20 concepts for unified time series element types
 * 
 * This header defines concepts that enable generic programming across all
 * time series data types in WhiskerToolbox. The concepts establish a common
 * interface for element access while respecting the different characteristics
 * of each data type.
 * 
 * ## Element Type Hierarchy
 * 
 * All time series element types satisfy the `TimeSeriesElement` concept,
 * which requires a `.time()` accessor returning `TimeFrameIndex`.
 * 
 * Types with EntityId support additionally satisfy `EntityElement`:
 * - `EventWithId` (DigitalEventSeries)
 * - `IntervalWithId` (DigitalIntervalSeries)
 * - `RaggedElement<TData>` (RaggedTimeSeries<TData>::RaggedElement)
 * 
 * Types without EntityId support:
 * - `TimeValuePoint` (AnalogTimeSeries)
 * - `FlatElement` (RaggedAnalogTimeSeries)
 * 
 * ## Backward Compatibility
 * 
 * The main `elements()` method on containers returns pair-based types for
 * backward compatibility with TransformPipeline. Use `elementsView()` for
 * concept-compliant iteration with proper element types.
 * 
 * ## Usage Example
 * 
 * ```cpp
 * // Generic function that works with any time series element
 * template<TimeSeriesElement T>
 * TimeFrameIndex extractTime(T const& elem) {
 *     return elem.time();
 * }
 * 
 * // Generic function only for entity-bearing elements
 * template<EntityElement T>
 * EntityId extractEntityId(T const& elem) {
 *     return elem.id();
 * }
 * ```
 * 
 * @see AnalogTimeSeries::TimeValuePoint
 * @see RaggedAnalogTimeSeries::FlatElement
 * @see EventWithId
 * @see IntervalWithId
 * @see RaggedTimeSeries<TData>::RaggedElement
 */

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <concepts>
#include <type_traits>
#include <unordered_set>

namespace WhiskerToolbox::Concepts {

/**
 * @brief Concept for time series element types
 * 
 * Any type satisfying this concept can be used in generic time series
 * algorithms that need to extract time information from elements.
 * 
 * @tparam T The element type to check
 * 
 * Requirements:
 * - `t.time()` must return a value convertible to `TimeFrameIndex`
 */
template<typename T>
concept TimeSeriesElement = requires(T const& t) {
    { t.time() } -> std::convertible_to<TimeFrameIndex>;
};

/**
 * @brief Concept for entity-bearing time series element types
 * 
 * This concept extends `TimeSeriesElement` with EntityId support.
 * Elements satisfying this concept can be used in entity-aware
 * algorithms like filtering by EntityId set.
 * 
 * @tparam T The element type to check
 * 
 * Requirements:
 * - Must satisfy `TimeSeriesElement`
 * - `t.id()` must return a value convertible to `EntityId`
 */
template<typename T>
concept EntityElement = TimeSeriesElement<T> && requires(T const& t) {
    { t.id() } -> std::convertible_to<EntityId>;
};

/**
 * @brief Concept for value-bearing time series element types
 * 
 * This concept extends `TimeSeriesElement` with value access.
 * The value type is specified as a template parameter to allow
 * different data types across time series.
 * 
 * @tparam T The element type to check
 * @tparam V The expected value type
 * 
 * Requirements:
 * - Must satisfy `TimeSeriesElement`
 * - `t.value()` must return a value convertible to `V`
 */
template<typename T, typename V>
concept ValueElement = TimeSeriesElement<T> && requires(T const& t) {
    { t.value() } -> std::convertible_to<V>;
};

/**
 * @brief Concept for complete time series elements with both entity and value
 * 
 * This concept combines `EntityElement` and `ValueElement` requirements,
 * representing the most feature-complete element types.
 * 
 * @tparam T The element type to check
 * @tparam V The expected value type
 */
template<typename T, typename V>
concept FullElement = EntityElement<T> && ValueElement<T, V>;

// ========== Type Traits for Concept Detection ==========

/**
 * @brief Type trait to check if a type satisfies TimeSeriesElement
 */
template<typename T>
struct is_time_series_element : std::bool_constant<TimeSeriesElement<T>> {};

template<typename T>
inline constexpr bool is_time_series_element_v = is_time_series_element<T>::value;

/**
 * @brief Type trait to check if a type satisfies EntityElement
 */
template<typename T>
struct is_entity_element : std::bool_constant<EntityElement<T>> {};

template<typename T>
inline constexpr bool is_entity_element_v = is_entity_element<T>::value;

// ========== Concept-Based Utility Functions ==========

/**
 * @brief Extract time from any time series element
 * 
 * @tparam T Element type satisfying TimeSeriesElement
 * @param elem The element to extract time from
 * @return TimeFrameIndex The time value
 */
template<TimeSeriesElement T>
[[nodiscard]] constexpr TimeFrameIndex getTime(T const& elem) {
    return elem.time();
}

/**
 * @brief Extract EntityId from any entity-bearing element
 * 
 * @tparam T Element type satisfying EntityElement
 * @param elem The element to extract EntityId from
 * @return EntityId The entity identifier
 */
template<EntityElement T>
[[nodiscard]] constexpr EntityId getEntityId(T const& elem) {
    return elem.id();
}

/**
 * @brief Check if an element's time is within a range
 * 
 * @tparam T Element type satisfying TimeSeriesElement
 * @param elem The element to check
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @return true if element's time is within [start, end]
 */
template<TimeSeriesElement T>
[[nodiscard]] constexpr bool isInTimeRange(
    T const& elem, 
    TimeFrameIndex start, 
    TimeFrameIndex end) {
    auto const time = elem.time();
    return time >= start && time <= end;
}

/**
 * @brief Check if an element's EntityId is in a set
 * 
 * @tparam T Element type satisfying EntityElement
 * @param elem The element to check
 * @param ids Set of EntityIds to check against
 * @return true if element's EntityId is in the set
 */
template<EntityElement T>
[[nodiscard]] bool isInEntitySet(
    T const& elem,
    std::unordered_set<EntityId> const& ids) {
    return ids.contains(elem.id());
}

} // namespace WhiskerToolbox::Concepts

#endif // TIME_SERIES_CONCEPTS_HPP
