/**
 * @file GetEntityIdsInRange.cpp
 * @brief Implementation of getEntityIdsInRange overloads for digital series types
 */

#include "GetEntityIdsInRange.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

namespace commands {

std::unordered_set<EntityId> getEntityIdsInRange(
        DigitalEventSeries const & data,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    std::unordered_set<EntityId> result;
    for (auto const & event: data.view()) {
        if (event.time() >= start && event.time() <= end) {
            result.insert(event.id());
        }
    }
    return result;
}

std::unordered_set<EntityId> getEntityIdsInRange(
        DigitalIntervalSeries const & data,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    std::unordered_set<EntityId> result;
    auto const time_frame = data.getTimeFrame();
    if (!time_frame) {
        return result;
    }

    ClockTicksInterval const query{
            time_frame->getTimeAtIndex(start),
            time_frame->getTimeAtIndex(end)};

    for (auto const & elem: data.view()) {
        if (is_overlapping(elem.interval, query)) {
            result.insert(elem.id());
        }
    }
    return result;
}

}// namespace commands
