/**
 * @file GetEntityIdsInRange.hpp
 * @brief Free functions to collect EntityIds within a time range from various data types
 *
 * These are pure query functions with no side effects, used by commands such as
 * MoveByTimeRange and CopyByTimeRange to determine which entities fall within
 * a specified frame range.
 */

#ifndef GET_ENTITY_IDS_IN_RANGE_HPP
#define GET_ENTITY_IDS_IN_RANGE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <unordered_set>

class DigitalEventSeries;
class DigitalIntervalSeries;

namespace commands {

/// @brief Collect EntityIds in [start, end] from a RaggedTimeSeries type
///        (LineData, PointData, MaskData) using flattened_data().
template<typename RaggedT>
    requires requires(RaggedT const & r) { r.flattened_data(); }
std::unordered_set<EntityId> getEntityIdsInRange(
        RaggedT const & data,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    std::unordered_set<EntityId> result;
    for (auto const & [time, entity_id, data_ref]: data.flattened_data()) {
        (void) data_ref;
        if (time >= start && time <= end) {
            result.insert(entity_id);
        }
    }
    return result;
}

/// @brief Collect EntityIds in [start, end] from a DigitalEventSeries using view().
std::unordered_set<EntityId> getEntityIdsInRange(
        DigitalEventSeries const & data,
        TimeFrameIndex start,
        TimeFrameIndex end);

/// @brief Collect EntityIds of intervals overlapping [start, end] from a DigitalIntervalSeries.
std::unordered_set<EntityId> getEntityIdsInRange(
        DigitalIntervalSeries const & data,
        TimeFrameIndex start,
        TimeFrameIndex end);

}// namespace commands

#endif// GET_ENTITY_IDS_IN_RANGE_HPP
