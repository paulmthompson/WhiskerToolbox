#ifndef STATE_ESTIMATION_DATASOURCE_HPP
#define STATE_ESTIMATION_DATASOURCE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <concepts>
#include <ranges>
#include <tuple>

namespace StateEstimation {

/**
 * @brief Concept for a single data item with entity and time information.
 * 
 * Represents a tuple-like structure containing:
 * - DataType const& : The actual data object (e.g., Line2D)
 * - EntityId : The entity identifier
 * - TimeFrameIndex : The time frame index
 */
template<typename T, typename DataType>
concept DataItem = requires(T item) {
    { std::get<0>(item) } -> std::convertible_to<DataType const&>;
    { std::get<1>(item) } -> std::convertible_to<EntityId>;
    { std::get<2>(item) } -> std::convertible_to<TimeFrameIndex>;
};

/**
 * @brief Concept for a data source that provides a range of data items.
 * 
 * This concept allows zero-copy iteration over data with associated entity IDs
 * and time frame indices. It can be satisfied by:
 * - LineData range views
 * - Custom generators
 * - Any container providing appropriate tuple-like elements
 */
template<typename Source, typename DataType>
concept DataSource = std::ranges::input_range<Source> &&
    DataItem<std::ranges::range_value_t<Source>, DataType>;

/**
 * @brief Helper to extract data from a DataItem
 */
template<typename Item>
inline auto const& getData(Item const& item) {
    return std::get<0>(item);
}

/**
 * @brief Helper to extract EntityId from a DataItem
 */
template<typename Item>
inline EntityId getEntityId(Item const& item) {
    return std::get<1>(item);
}

/**
 * @brief Helper to extract TimeFrameIndex from a DataItem
 */
template<typename Item>
inline TimeFrameIndex getTimeFrameIndex(Item const& item) {
    return std::get<2>(item);
}

} // namespace StateEstimation

#endif // STATE_ESTIMATION_DATASOURCE_HPP
