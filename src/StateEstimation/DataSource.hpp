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
 * A DataItem can be either:
 * 1. Tuple-like: std::tuple<DataType, EntityId, TimeFrameIndex>
 * 2. Accessor-based: struct with getData(), getEntityId(), getTimeFrameIndex() methods
 * 
 * This dual support allows both legacy tuple-based sources and new adapter-based sources.
 */
template<typename T, typename DataType>
concept DataItem = 
    // Option 1: Tuple-like access
    (requires(T item) {
        { std::get<0>(item) } -> std::convertible_to<DataType const&>;
        { std::get<1>(item) } -> std::convertible_to<EntityId>;
        { std::get<2>(item) } -> std::convertible_to<TimeFrameIndex>;
    })
    ||
    // Option 2: Accessor methods
    (requires(T item) {
        { item.getData() } -> std::convertible_to<DataType const&>;
        { item.getEntityId() } -> std::convertible_to<EntityId>;
        { item.getTimeFrameIndex() } -> std::convertible_to<TimeFrameIndex>;
    });

/**
 * @brief Concept for a data source that provides a range of data items.
 * 
 * This concept allows zero-copy iteration over data with associated entity IDs
 * and time frame indices. It can be satisfied by:
 * - Tuple-based ranges: vector<tuple<DataType, EntityId, TimeFrameIndex>>
 * - Adapter-based ranges: FlattenedDataAdapter yielding DataItem structs
 * - LineData range views
 * - Custom generators
 * 
 * Note: Removes references from Source to handle forwarding references correctly.
 */
template<typename Source, typename DataType>
concept DataSource = std::ranges::input_range<std::remove_reference_t<Source>> &&
    DataItem<std::ranges::range_value_t<std::remove_reference_t<Source>>, DataType>;

/**
 * @brief Helper to extract data from a DataItem (supports both tuple and accessor)
 */
template<typename Item>
inline auto const& getData(Item const& item) {
    if constexpr (requires { item.getData(); }) {
        return item.getData();
    } else {
        return std::get<0>(item);
    }
}

/**
 * @brief Helper to extract EntityId from a DataItem (supports both tuple and accessor)
 */
template<typename Item>
inline EntityId getEntityId(Item const& item) {
    if constexpr (requires { item.getEntityId(); }) {
        return item.getEntityId();
    } else {
        return std::get<1>(item);
    }
}

/**
 * @brief Helper to extract TimeFrameIndex from a DataItem (supports both tuple and accessor)
 */
template<typename Item>
inline TimeFrameIndex getTimeFrameIndex(Item const& item) {
    if constexpr (requires { item.getTimeFrameIndex(); }) {
        return item.getTimeFrameIndex();
    } else {
        return std::get<2>(item);
    }
}

} // namespace StateEstimation

#endif // STATE_ESTIMATION_DATASOURCE_HPP
