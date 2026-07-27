/**
 * @file PruneOverlappingIntervals.cpp
 * @brief Greedy keep-first pruning of overlapping intervals in a DigitalIntervalSeries
 */

#include "PruneOverlappingIntervals.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "core/ComputeContext.hpp"

#include <cstdint>
#include <vector>

namespace {

/**
 * @brief Create an empty disjoint DigitalIntervalSeries preserving the input TimeFrame
 *
 * @param time_frame TimeFrame shared pointer from the input series
 * @return Empty interval series with TimeFrame set
 */
std::shared_ptr<DigitalIntervalSeries> makeEmptyResult(std::shared_ptr<TimeFrame> const & time_frame) {
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->setTimeFrame(time_frame);
    return result;
}

/**
 * @brief Compute indices of intervals that survive greedy overlap pruning
 *
 * Local adaptation of Neuralyzer::Plots::pruneOverlappingAlignmentTimes applied to
 * interval bounds [start, end] instead of alignment times with pre/post windows.
 *
 * @param intervals Sorted intervals in TimeFrameIndex space
 * @return Indices into intervals that survive pruning
 *
 * @pre intervals is sorted by start time in ascending order
 */
[[nodiscard]] std::vector<size_t> pruneOverlappingIntervalIndices(std::vector<Interval> const & intervals) {
    std::vector<size_t> kept_indices;

    if (intervals.empty()) {
        return kept_indices;
    }

    kept_indices.reserve(intervals.size());
    kept_indices.push_back(0);

    int64_t last_kept_end = intervals[0].end;

    for (size_t i = 1; i < intervals.size(); ++i) {
        int64_t const current_start = intervals[i].start;
        if (current_start > last_kept_end) {
            kept_indices.push_back(i);
            last_kept_end = intervals[i].end;
        }
    }

    return kept_indices;
}

}// namespace

namespace Neuralyzer::Transforms::V2::Examples {

std::shared_ptr<DigitalIntervalSeries> pruneOverlappingIntervals(
        DigitalIntervalSeries const & input,
        PruneOverlappingIntervalsParams const & /*params*/,
        ComputeContext const & ctx) {
    auto const time_frame = input.getTimeFrame();

    if (input.size() == 0) {
        ctx.reportProgress(100);
        return makeEmptyResult(time_frame);
    }

    std::vector<Interval> intervals;
    intervals.reserve(input.size());
    for (auto const & interval_with_id: input.view()) {
        intervals.push_back(interval_with_id.interval);
    }

    auto const kept_indices = pruneOverlappingIntervalIndices(intervals);

    std::vector<Interval> kept_intervals;
    kept_intervals.reserve(kept_indices.size());

    std::size_t const total_intervals = kept_indices.size();
    std::size_t processed = 0;

    ctx.reportProgress(0);

    for (size_t const idx: kept_indices) {
        if (processed % 100 == 0 && ctx.shouldCancel()) {
            return makeEmptyResult(time_frame);
        }

        kept_intervals.push_back(intervals[idx]);

        ++processed;
        if (processed % 100 == 0 || processed == total_intervals) {
            int const progress = static_cast<int>(
                    (static_cast<double>(processed) / static_cast<double>(total_intervals)) * 100.0);
            ctx.reportProgress(progress);
        }
    }

    ctx.reportProgress(100);
    auto result = std::make_shared<DigitalIntervalSeries>(std::move(kept_intervals));
    result->setTimeFrame(time_frame);
    return result;
}

}// namespace Neuralyzer::Transforms::V2::Examples
