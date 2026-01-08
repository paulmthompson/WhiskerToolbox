#ifndef TIME_SERIES_FILTERS_HPP
#define TIME_SERIES_FILTERS_HPP

/**
 * @file TimeSeriesFilters.hpp
 * @brief Generic filter utilities for time series data types
 * 
 * This header provides concept-constrained free function templates for filtering
 * time series data by time range and EntityId. These functions work with any
 * range of elements satisfying the appropriate concepts from TimeSeriesConcepts.hpp.
 * 
 * ## Design Philosophy
 * 
 * The filter functions are designed to:
 * 1. **Be lazy**: Return views that don't materialize data until iteration
 * 2. **Be composable**: Can be chained with other range adaptors
 * 3. **Be type-safe**: Concepts enforce correct usage at compile time
 * 4. **Work uniformly**: Same API across all time series types
 * 
 * ## Usage Examples
 * 
 * ```cpp
 * // Filter AnalogTimeSeries elements by time range
 * auto series = std::make_shared<AnalogTimeSeries>();
 * // ... populate series ...
 * 
 * // Get lazy view of elements in time range [100, 200]
 * for (auto const& elem : filterByTimeRange(series->elementsView(), 
 *                                            TimeFrameIndex(100), 
 *                                            TimeFrameIndex(200))) {
 *     // Process elem
 * }
 * 
 * // Filter DigitalEventSeries by EntityIds
 * std::unordered_set<EntityId> ids{EntityId(1), EntityId(3)};
 * for (auto const& event : filterByEntityIds(eventSeries->view(), ids)) {
 *     // Process event
 * }
 * 
 * // Compose multiple filters
 * auto result = filterByTimeRange(
 *     filterByEntityIds(series->view(), ids),
 *     TimeFrameIndex(100), TimeFrameIndex(200));
 * 
 * // Materialize to vector when needed
 * auto vec = materializeToVector(filterByTimeRange(elements, start, end));
 * ```
 * 
 * ## Supported Types
 * 
 * ### filterByTimeRange
 * Works with any range of elements satisfying `TimeSeriesElement`:
 * - `AnalogTimeSeries::TimeValuePoint`
 * - `RaggedAnalogTimeSeries::FlatElement`
 * - `EventWithId` (DigitalEventSeries)
 * - `IntervalWithId` (DigitalIntervalSeries)
 * - `RaggedElement<T>` (RaggedTimeSeries<T>)
 * 
 * ### filterByEntityIds
 * Works with any range of elements satisfying `EntityElement`:
 * - `EventWithId` (DigitalEventSeries)
 * - `IntervalWithId` (DigitalIntervalSeries)
 * - `RaggedElement<T>` (RaggedTimeSeries<T>)
 * 
 * Note: `TimeValuePoint` and `FlatElement` do NOT support EntityId filtering
 * as they don't have EntityIds. This is enforced at compile time.
 * 
 * @see TimeSeriesConcepts.hpp for concept definitions
 * @see AnalogTimeSeries, DigitalEventSeries, DigitalIntervalSeries, RaggedTimeSeries
 */

#include "TimeSeriesConcepts.hpp"

#include <ranges>
#include <vector>
#include <algorithm>

namespace WhiskerToolbox::Filters {

// Import concepts from Concepts namespace
using namespace WhiskerToolbox::Concepts;

// ============================================================================
// Time Range Filtering
// ============================================================================

/**
 * @brief Filter a range of time series elements by time range
 * 
 * Returns a lazy view containing only elements whose time() falls within
 * the inclusive range [start, end]. The view does not copy data and
 * evaluates lazily during iteration.
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range to filter
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @return A lazy view of elements in the time range
 * 
 * @note The returned view holds a reference to `start` and `end` by value,
 *       so they are safely captured.
 * 
 * @par Example
 * @code
 * auto series = createAnalogTimeSeries();
 * auto filtered = filterByTimeRange(series->elementsView(),
 *                                    TimeFrameIndex(100),
 *                                    TimeFrameIndex(200));
 * for (auto const& elem : filtered) {
 *     std::cout << elem.time().getValue() << ": " << elem.value() << "\n";
 * }
 * @endcode
 * 
 * @par Complexity
 * O(1) to create the view. O(n) to fully iterate where n = input size.
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto filterByTimeRange(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    return std::forward<R>(range) 
        | std::views::filter([start, end](auto const& elem) {
            return isInTimeRange(elem, start, end);
        });
}

/**
 * @brief Filter elements by time range with exclusive end
 * 
 * Like filterByTimeRange but uses exclusive end bound: [start, end)
 * This matches the convention used by many STL algorithms.
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range to filter
 * @param start Start of time range (inclusive)
 * @param end End of time range (exclusive)
 * @return A lazy view of elements in the time range
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto filterByTimeRangeExclusive(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    return std::forward<R>(range)
        | std::views::filter([start, end](auto const& elem) {
            auto const time = elem.time();
            return time >= start && time < end;
        });
}

// ============================================================================
// EntityId Filtering
// ============================================================================

/**
 * @brief Filter a range of entity elements by EntityId set
 * 
 * Returns a lazy view containing only elements whose id() is present
 * in the provided set of EntityIds. The view does not copy data and
 * evaluates lazily during iteration.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range to filter
 * @param entity_ids Set of EntityIds to include
 * @return A lazy view of elements with matching EntityIds
 * 
 * @note This function is only available for types satisfying EntityElement.
 *       Types like TimeValuePoint and FlatElement will fail at compile time.
 * 
 * @warning The returned view holds a reference to `entity_ids`. The caller
 *          must ensure the set outlives the view's usage.
 * 
 * @par Example
 * @code
 * auto events = createDigitalEventSeries();
 * std::unordered_set<EntityId> selected{EntityId(1), EntityId(3), EntityId(5)};
 * auto filtered = filterByEntityIds(events->view(), selected);
 * for (auto const& event : filtered) {
 *     // Only events with EntityId 1, 3, or 5
 * }
 * @endcode
 * 
 * @par Complexity
 * O(1) to create the view. O(n) to fully iterate where n = input size.
 * Each element check is O(1) average due to hash set lookup.
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto filterByEntityIds(
    R&& range,
    std::unordered_set<EntityId> const& entity_ids)
{
    return std::forward<R>(range)
        | std::views::filter([&entity_ids](auto const& elem) {
            return isInEntitySet(elem, entity_ids);
        });
}

/**
 * @brief Filter a range of entity elements by a single EntityId
 * 
 * Convenience overload for filtering by a single EntityId.
 * More efficient than creating a set for single-ID queries.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range to filter
 * @param entity_id The EntityId to match
 * @return A lazy view of elements with the specified EntityId
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto filterByEntityId(
    R&& range,
    EntityId entity_id)
{
    return std::forward<R>(range)
        | std::views::filter([entity_id](auto const& elem) {
            return elem.id() == entity_id;
        });
}

// ============================================================================
// Combined Filtering
// ============================================================================

/**
 * @brief Filter by both time range and EntityId set
 * 
 * Convenience function that combines time range and EntityId filtering
 * in a single pass. More efficient than chaining separate filters when
 * both criteria are known upfront.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range to filter
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @param entity_ids Set of EntityIds to include
 * @return A lazy view of elements matching both criteria
 * 
 * @warning The returned view holds a reference to `entity_ids`. The caller
 *          must ensure the set outlives the view's usage.
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto filterByTimeRangeAndEntityIds(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end,
    std::unordered_set<EntityId> const& entity_ids)
{
    return std::forward<R>(range)
        | std::views::filter([start, end, &entity_ids](auto const& elem) {
            return isInTimeRange(elem, start, end) && 
                   isInEntitySet(elem, entity_ids);
        });
}

// ============================================================================
// Materialization Utilities
// ============================================================================

/**
 * @brief Materialize a range into a vector
 * 
 * Converts any range (including lazy views) into an owning vector.
 * Useful when you need to:
 * - Pass filtered results to APIs expecting vectors
 * - Iterate multiple times over filtered results
 * - Ensure data lifetime beyond the source range
 * 
 * @tparam R Range type
 * @param range The input range to materialize
 * @return std::vector containing copies of all elements
 * 
 * @par Example
 * @code
 * auto filtered = filterByTimeRange(series->view(), start, end);
 * auto vec = materializeToVector(filtered);
 * // vec is now an independent std::vector
 * @endcode
 */
template<std::ranges::input_range R>
[[nodiscard]] auto materializeToVector(R&& range)
{
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    // Reserve if we can determine size (for sized_range)
    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::size(range));
    }
    
    for (auto&& elem : range) {
        result.push_back(std::forward<decltype(elem)>(elem));
    }
    
    return result;
}

/**
 * @brief Count elements matching time range criteria
 * 
 * Counts elements without materializing to a container.
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range to count
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @return Number of elements in the time range
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr std::size_t countInTimeRange(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(std::forward<R>(range), [start, end](auto const& elem) {
            return isInTimeRange(elem, start, end);
        }));
}

/**
 * @brief Count elements matching EntityId criteria
 * 
 * Counts elements without materializing to a container.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range to count
 * @param entity_ids Set of EntityIds to match
 * @return Number of elements with matching EntityIds
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] std::size_t countWithEntityIds(
    R&& range,
    std::unordered_set<EntityId> const& entity_ids)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(std::forward<R>(range), [&entity_ids](auto const& elem) {
            return isInEntitySet(elem, entity_ids);
        }));
}

// ============================================================================
// Predicate Utilities
// ============================================================================

/**
 * @brief Check if any element exists in time range
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range to check
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @return true if at least one element is in the range
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr bool anyInTimeRange(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    return std::ranges::any_of(std::forward<R>(range), [start, end](auto const& elem) {
        return isInTimeRange(elem, start, end);
    });
}

/**
 * @brief Check if all elements are in time range
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range to check
 * @param start Start of time range (inclusive)
 * @param end End of time range (inclusive)
 * @return true if all elements are in the range (or range is empty)
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr bool allInTimeRange(
    R&& range,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    return std::ranges::all_of(std::forward<R>(range), [start, end](auto const& elem) {
        return isInTimeRange(elem, start, end);
    });
}

/**
 * @brief Check if any element has a matching EntityId
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range to check
 * @param entity_ids Set of EntityIds to match
 * @return true if at least one element has a matching EntityId
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] bool anyWithEntityIds(
    R&& range,
    std::unordered_set<EntityId> const& entity_ids)
{
    return std::ranges::any_of(std::forward<R>(range), [&entity_ids](auto const& elem) {
        return isInEntitySet(elem, entity_ids);
    });
}

// ============================================================================
// Transformation Utilities (Time Extraction)
// ============================================================================

/**
 * @brief Extract times from a range of elements
 * 
 * Returns a lazy view that transforms each element to its time value.
 * Useful for getting just the time values without other data.
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range
 * @return A lazy view of TimeFrameIndex values
 * 
 * @par Example
 * @code
 * auto times = extractTimes(series->elementsView());
 * for (auto time : times) {
 *     std::cout << time.getValue() << "\n";
 * }
 * @endcode
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto extractTimes(R&& range)
{
    return std::forward<R>(range)
        | std::views::transform([](auto const& elem) {
            return elem.time();
        });
}

/**
 * @brief Extract EntityIds from a range of elements
 * 
 * Returns a lazy view that transforms each element to its EntityId.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range
 * @return A lazy view of EntityId values
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto extractEntityIds(R&& range)
{
    return std::forward<R>(range)
        | std::views::transform([](auto const& elem) {
            return elem.id();
        });
}

/**
 * @brief Get unique EntityIds from a range
 * 
 * Materializes and returns a set of all unique EntityIds in the range.
 * 
 * @tparam R Range type (must be an input_range of EntityElement)
 * @param range The input range
 * @return std::unordered_set<EntityId> containing all unique EntityIds
 */
template<std::ranges::input_range R>
    requires EntityElement<std::ranges::range_value_t<R>>
[[nodiscard]] std::unordered_set<EntityId> uniqueEntityIds(R&& range)
{
    std::unordered_set<EntityId> result;
    for (auto const& elem : range) {
        result.insert(elem.id());
    }
    return result;
}

// ============================================================================
// Boundary Utilities
// ============================================================================

/**
 * @brief Find the minimum time in a range
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range (must not be empty)
 * @return std::optional<TimeFrameIndex> The minimum time, or nullopt if empty
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] std::optional<TimeFrameIndex> minTime(R&& range)
{
    auto it = std::ranges::min_element(std::forward<R>(range), 
        [](auto const& a, auto const& b) {
            return a.time() < b.time();
        });
    
    if (it == std::ranges::end(range)) {
        return std::nullopt;
    }
    return (*it).time();
}

/**
 * @brief Find the maximum time in a range
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range (must not be empty)
 * @return std::optional<TimeFrameIndex> The maximum time, or nullopt if empty
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] std::optional<TimeFrameIndex> maxTime(R&& range)
{
    auto it = std::ranges::max_element(std::forward<R>(range),
        [](auto const& a, auto const& b) {
            return a.time() < b.time();
        });
    
    if (it == std::ranges::end(range)) {
        return std::nullopt;
    }
    return (*it).time();
}

/**
 * @brief Find the time bounds (min and max) of a range
 * 
 * @tparam R Range type (must be an input_range of TimeSeriesElement)
 * @param range The input range
 * @return std::optional<std::pair<TimeFrameIndex, TimeFrameIndex>> 
 *         The (min, max) time pair, or nullopt if empty
 */
template<std::ranges::input_range R>
    requires TimeSeriesElement<std::ranges::range_value_t<R>>
[[nodiscard]] std::optional<std::pair<TimeFrameIndex, TimeFrameIndex>> timeBounds(R&& range)
{
    auto [min_it, max_it] = std::ranges::minmax_element(std::forward<R>(range),
        [](auto const& a, auto const& b) {
            return a.time() < b.time();
        });
    
    if (min_it == std::ranges::end(range)) {
        return std::nullopt;
    }
    return std::make_pair((*min_it).time(), (*max_it).time());
}

} // namespace WhiskerToolbox::Filters

#endif // TIME_SERIES_FILTERS_HPP
