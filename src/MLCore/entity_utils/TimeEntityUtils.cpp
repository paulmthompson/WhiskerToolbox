/**
 * @file TimeEntityUtils.cpp
 * @brief Implementation of TimeEntity bulk registration and interval/entity conversions
 */

#include "TimeEntityUtils.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace MLCore {

// ============================================================================
// registerTimeEntities
// ============================================================================

std::vector<EntityId> registerTimeEntities(
        DataManager & dm,
        std::string const & time_key_str,
        std::span<TimeFrameIndex const> frames) {
    std::vector<EntityId> ids;
    ids.reserve(frames.size());

    TimeKey const key(time_key_str);
    for (auto const & frame: frames) {
        ids.push_back(dm.ensureTimeEntityId(key, frame));
    }
    return ids;
}

// ============================================================================
// intervalsToTimeEntities
// ============================================================================

std::vector<EntityId> intervalsToTimeEntities(
        DataManager & dm,
        DigitalIntervalSeries const & intervals,
        std::string const & time_key_str) {
    // Use a set to deduplicate overlapping intervals and produce sorted output
    std::set<TimeFrameIndex> all_times;

    for (auto const & iv: intervals.view()) {
        for (TimeFrameIndex t = iv.interval.start; t <= iv.interval.end; ++t) {
            all_times.insert(t);
        }
    }

    std::vector<EntityId> ids;
    ids.reserve(all_times.size());

    TimeKey const key(time_key_str);
    for (TimeFrameIndex t: all_times) {
        ids.push_back(dm.ensureTimeEntityId(key, t));
    }
    return ids;
}

// ============================================================================
// timeEntitiesToIntervals
// ============================================================================

std::shared_ptr<DigitalIntervalSeries> timeEntitiesToIntervals(
        DataManager & dm,
        GroupId group_id,
        std::string const & time_key_str) {
    // Retrieve sorted TimeFrameIndex values from the group
    TimeKey const key(time_key_str);
    auto frame_indices = dm.getTimeIndicesInGroup(group_id, key);

    if (frame_indices.empty()) {
        return nullptr;
    }

    // Sort by value for contiguous-run detection
    std::sort(frame_indices.begin(), frame_indices.end(),
              [](TimeFrameIndex const & a, TimeFrameIndex const & b) {
                  return a.getValue() < b.getValue();
              });

    // Remove duplicates (shouldn't happen, but defensive)
    auto last = std::unique(frame_indices.begin(), frame_indices.end(),
                            [](TimeFrameIndex const & a, TimeFrameIndex const & b) {
                                return a.getValue() == b.getValue();
                            });
    frame_indices.erase(last, frame_indices.end());

    // Merge adjacent frames into intervals
    auto series = std::make_shared<DigitalIntervalSeries>();

    TimeFrameIndex interval_start = frame_indices[0];
    TimeFrameIndex interval_end = interval_start;

    for (std::size_t i = 1; i < frame_indices.size(); ++i) {
        TimeFrameIndex const t = frame_indices[i];
        if (t == interval_end + TimeFrameIndex{1}) {
            // Extend current interval
            interval_end = t;
        } else {
            // Gap — flush and start new
            series->addEvent(TimeFrameInterval{interval_start, interval_end});
            interval_start = t;
            interval_end = t;
        }
    }
    // Flush the final interval
    series->addEvent(TimeFrameInterval{interval_start, interval_end});

    return series;
}

}// namespace MLCore