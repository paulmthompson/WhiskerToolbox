
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
    std::span<TimeFrameIndex const> frames)
{
    std::vector<EntityId> ids;
    ids.reserve(frames.size());

    TimeKey const key(time_key_str);
    for (auto const & frame : frames) {
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
    std::string const & time_key_str)
{
    // Use a set to deduplicate overlapping intervals and produce sorted output
    std::set<int64_t> all_times;

    for (auto const & iv : intervals.view()) {
        for (int64_t t = iv.interval.start; t <= iv.interval.end; ++t) {
            all_times.insert(t);
        }
    }

    std::vector<EntityId> ids;
    ids.reserve(all_times.size());

    TimeKey const key(time_key_str);
    for (int64_t t : all_times) {
        ids.push_back(dm.ensureTimeEntityId(key, TimeFrameIndex(t)));
    }
    return ids;
}

// ============================================================================
// timeEntitiesToIntervals
// ============================================================================

std::shared_ptr<DigitalIntervalSeries> timeEntitiesToIntervals(
    DataManager & dm,
    GroupId group_id,
    std::string const & time_key_str)
{
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

    int64_t interval_start = frame_indices[0].getValue();
    int64_t interval_end   = interval_start;

    for (std::size_t i = 1; i < frame_indices.size(); ++i) {
        int64_t const t = frame_indices[i].getValue();
        if (t == interval_end + 1) {
            // Extend current interval
            interval_end = t;
        } else {
            // Gap — flush and start new
            series->addEvent(Interval{interval_start, interval_end});
            interval_start = t;
            interval_end = t;
        }
    }
    // Flush the final interval
    series->addEvent(Interval{interval_start, interval_end});

    return series;
}

} // namespace MLCore