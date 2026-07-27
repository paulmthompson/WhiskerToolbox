/**
 * @file PlotAlignmentWindowPreparation.hpp
 * @brief Qt-free helpers for preparing plot alignment windows for GatherResult.
 */
#ifndef PLOT_ALIGNMENT_WINDOW_PREPARATION_HPP
#define PLOT_ALIGNMENT_WINDOW_PREPARATION_HPP

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/algorithms/IntervalToEvent/IntervalToEvent.hpp"
#include "TransformsV2/algorithms/PruneOverlappingIntervals/PruneOverlappingIntervals.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Neuralyzer::Plots {

/**
 * @brief Plot-facing interval alignment point.
 *
 * This mirrors the UI choices without depending on GatherResult interval adapters.
 */
enum class AlignmentPoint {
    Start,///< Use interval.start as alignment time.
    End,  ///< Use interval.end as alignment time.
    Center///< Use (interval.start + interval.end) / 2 as alignment time.
};

/**
 * @brief Prepared row metadata for a plot-aligned gather.
 */
struct PreparedAlignmentWindows {
    std::shared_ptr<DigitalIntervalSeries const> windows;      ///< Row-defining gather windows.
    std::shared_ptr<DigitalEventSeries const> alignment_points;///< Row-aligned t=0 events.

    /**
     * @brief Check whether both row metadata series are available.
     * @post Returns true only when windows and alignment points are non-null.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return windows != nullptr && alignment_points != nullptr;
    }
};

/**
 * @brief Result of alignment source lookup.
 */
struct AlignmentSourceResult {
    std::shared_ptr<DigitalEventSeries> event_series;
    std::shared_ptr<DigitalIntervalSeries> interval_series;
    bool is_event_series = false;
    bool is_interval_series = false;
    std::string error_message;

    /**
     * @brief Check whether an event or interval alignment source was found.
     * @post Returns true when exactly one supported source kind is available.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return is_event_series || is_interval_series;
    }
};

/**
 * @brief Convert IntervalAlignmentType to the plot-facing AlignmentPoint enum.
 *
 * @param type UI alignment type.
 * @return Corresponding plot alignment point.
 * @post End maps to AlignmentPoint::End; all other current values map to Start.
 */
[[nodiscard]] inline AlignmentPoint toAlignmentPoint(IntervalAlignmentType type) noexcept {
    switch (type) {
        case IntervalAlignmentType::End:
            return AlignmentPoint::End;
        case IntervalAlignmentType::Beginning:
        default:
            return AlignmentPoint::Start;
    }
}

/**
 * @brief Convert a plot alignment point to the TransformsV2 interval-to-event enum.
 *
 * @param point Plot-facing alignment point.
 * @return Equivalent TransformsV2 interval event point.
 * @post The returned value preserves start/end/center semantics.
 */
[[nodiscard]] inline Neuralyzer::Transforms::V2::Examples::IntervalEventPoint
toIntervalEventPoint(AlignmentPoint point) noexcept {
    using Neuralyzer::Transforms::V2::Examples::IntervalEventPoint;
    switch (point) {
        case AlignmentPoint::End:
            return IntervalEventPoint::end;
        case AlignmentPoint::Center:
            return IntervalEventPoint::center;
        case AlignmentPoint::Start:
        default:
            return IntervalEventPoint::start;
    }
}

/**
 * @brief Look up an alignment source from DataManager.
 *
 * @param data_manager DataManager to query.
 * @param alignment_key Key of a DigitalEventSeries or DigitalIntervalSeries.
 * @return AlignmentSourceResult containing the series or an error message.
 * @post The result is valid only for digital event or digital interval data.
 */
[[nodiscard]] inline AlignmentSourceResult getAlignmentSource(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & alignment_key) {
    AlignmentSourceResult result;

    if (!data_manager || alignment_key.empty()) {
        result.error_message = "Invalid data manager or empty alignment key";
        return result;
    }

    DM_DataType const type = data_manager->getType(alignment_key);

    switch (type) {
        case DM_DataType::DigitalEvent: {
            result.event_series = data_manager->getData<DigitalEventSeries>(alignment_key);
            result.is_event_series = (result.event_series != nullptr);
            if (!result.is_event_series) {
                result.error_message = "Failed to retrieve DigitalEventSeries: " + alignment_key;
            }
            break;
        }
        case DM_DataType::DigitalInterval: {
            result.interval_series = data_manager->getData<DigitalIntervalSeries>(alignment_key);
            result.is_interval_series = (result.interval_series != nullptr);
            if (!result.is_interval_series) {
                result.error_message = "Failed to retrieve DigitalIntervalSeries: " + alignment_key;
            }
            break;
        }
        default:
            result.error_message = "Alignment key is not a DigitalEventSeries or DigitalIntervalSeries: " + alignment_key;
            break;
    }

    return result;
}

/**
 * @brief Extract absolute alignment times from a DigitalEventSeries.
 *
 * @param events Event series to inspect.
 * @return Absolute-time values in event row order.
 * @post If the series has no TimeFrame, raw event indices are returned.
 */
[[nodiscard]] inline std::vector<int64_t> extractAlignmentTimes(DigitalEventSeries const & events) {
    std::vector<int64_t> times;
    times.reserve(events.size());

    auto const time_frame = events.getTimeFrame();
    for (auto const & event: events.view()) {
        times.push_back(time_frame ? time_frame->getTimeAtIndex(event.time()) : event.time().getValue());
    }
    return times;
}

/**
 * @brief Extract absolute alignment times from a DigitalIntervalSeries.
 *
 * @param intervals Interval series to inspect.
 * @param align Which interval point to use.
 * @return Absolute-time values in interval row order.
 * @post If the series has no TimeFrame, raw interval indices are returned.
 */
[[nodiscard]] inline std::vector<int64_t> extractAlignmentTimes(
        DigitalIntervalSeries const & intervals,
        AlignmentPoint align) {
    std::vector<int64_t> times;
    times.reserve(intervals.size());

    auto const time_frame = intervals.getTimeFrame();
    for (auto const & interval_with_id: intervals.view()) {
        auto const & interval = interval_with_id.interval;
        int64_t index{};
        switch (align) {
            case AlignmentPoint::End:
                index = interval.end;
                break;
            case AlignmentPoint::Center:
                index = (interval.start + interval.end) / 2;
                break;
            case AlignmentPoint::Start:
            default:
                index = interval.start;
                break;
        }
        times.push_back(time_frame ? time_frame->getTimeAtIndex(TimeFrameIndex(index)) : index);
    }
    return times;
}

/**
 * @brief Prune alignment times whose expanded windows would overlap.
 *
 * @param alignment_times Sorted absolute alignment times.
 * @param pre_window Time units before each alignment time.
 * @param post_window Time units after each alignment time.
 * @return Indices into alignment_times that survive greedy keep-first pruning.
 *
 * @pre alignment_times is sorted in ascending order.
 * @post Touching windows are treated as overlapping.
 */
[[nodiscard]] inline std::vector<size_t> pruneOverlappingAlignmentTimes(
        std::vector<int64_t> const & alignment_times,
        int64_t pre_window,
        int64_t post_window) {
    std::vector<size_t> kept_indices;

    if (alignment_times.empty()) {
        return kept_indices;
    }

    kept_indices.reserve(alignment_times.size());
    kept_indices.push_back(0);

    int64_t last_kept_end = alignment_times[0] + post_window;

    for (size_t i = 1; i < alignment_times.size(); ++i) {
        int64_t const current_start = alignment_times[i] - pre_window;
        if (current_start > last_kept_end) {
            kept_indices.push_back(i);
            last_kept_end = alignment_times[i] + post_window;
        }
    }

    return kept_indices;
}

/**
 * @brief Create a filtered copy of a DigitalEventSeries.
 *
 * @param source Source event series.
 * @param kept_indices Indices in source view order to keep.
 * @return Owning event series with the same TimeFrame as source.
 * @pre source must be non-null and kept_indices must be sorted ascending.
 * @post Output row order follows kept_indices.
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> createFilteredEventSeries(
        std::shared_ptr<DigitalEventSeries const> const & source,
        std::vector<size_t> const & kept_indices) {
    assert(source && "createFilteredEventSeries: source must not be null");

    std::vector<TimeFrameIndex> kept_events;
    kept_events.reserve(kept_indices.size());

    size_t index = 0;
    size_t kept_pos = 0;
    for (auto const & event: source->view()) {
        if (kept_pos < kept_indices.size() && index == kept_indices[kept_pos]) {
            kept_events.push_back(event.time());
            ++kept_pos;
        }
        ++index;
    }

    auto filtered = std::make_shared<DigitalEventSeries>(std::move(kept_events));
    filtered->setTimeFrame(source->getTimeFrame());
    return filtered;
}

/**
 * @brief Create a filtered copy of a DigitalIntervalSeries.
 *
 * @param source Source interval series.
 * @param kept_indices Indices in source view order to keep.
 * @return Owning interval series with the same TimeFrame as source.
 * @pre source must be non-null and kept_indices must be sorted ascending.
 * @post Output row order follows kept_indices.
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries> createFilteredIntervalSeries(
        std::shared_ptr<DigitalIntervalSeries const> const & source,
        std::vector<size_t> const & kept_indices) {
    assert(source && "createFilteredIntervalSeries: source must not be null");

    std::vector<Interval> kept_intervals;
    kept_intervals.reserve(kept_indices.size());

    size_t index = 0;
    size_t kept_pos = 0;
    for (auto const & interval_with_id: source->view()) {
        if (kept_pos < kept_indices.size() && index == kept_indices[kept_pos]) {
            kept_intervals.push_back(interval_with_id.interval);
            ++kept_pos;
        }
        ++index;
    }

    auto filtered = std::make_shared<DigitalIntervalSeries>(std::move(kept_intervals));
    filtered->setTimeFrame(source->getTimeFrame());
    return filtered;
}

/**
 * @brief Convert an absolute-time window to interval indices in a target TimeFrame.
 *
 * @param alignment_time Absolute alignment time.
 * @param pre_window Time units before alignment.
 * @param post_window Time units after alignment.
 * @param target_time_frame TimeFrame for the output interval, or null for raw time indices.
 * @return Interval expressed in target_time_frame indices.
 * @post The start uses the next index at/after the window start; the end uses the nearest preceding index.
 */
[[nodiscard]] inline Interval physicalWindowToInterval(
        int64_t alignment_time,
        int64_t pre_window,
        int64_t post_window,
        std::shared_ptr<TimeFrame> const & target_time_frame) {
    int64_t const start_time = alignment_time - pre_window;
    int64_t const end_time = alignment_time + post_window;

    if (!target_time_frame) {
        return Interval{start_time, end_time};
    }

    return Interval{
            target_time_frame->getIndexAtTime(static_cast<float>(start_time), false).getValue(),
            target_time_frame->getIndexAtTime(static_cast<float>(end_time)).getValue()};
}

/**
 * @brief Build overlapping window intervals around alignment events.
 *
 * @param alignment_events Events providing t=0 points.
 * @param pre_window Absolute time units before each event.
 * @param post_window Absolute time units after each event.
 * @param window_time_frame TimeFrame in which output windows should be expressed.
 * @return Overlapping DigitalIntervalSeries row windows.
 * @pre alignment_events must be non-null.
 * @post Output row count matches alignment_events size.
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries> createWindowsAroundEvents(
        std::shared_ptr<DigitalEventSeries const> const & alignment_events,
        int64_t pre_window,
        int64_t post_window,
        std::shared_ptr<TimeFrame> const & window_time_frame) {
    assert(alignment_events && "createWindowsAroundEvents: alignment_events must not be null");

    std::vector<Interval> intervals;
    intervals.reserve(alignment_events->size());

    auto const alignment_time_frame = alignment_events->getTimeFrame();
    for (auto const & event: alignment_events->view()) {
        int64_t const alignment_time = alignment_time_frame
                                               ? alignment_time_frame->getTimeAtIndex(event.time())
                                               : event.time().getValue();
        intervals.push_back(physicalWindowToInterval(
                alignment_time,
                pre_window,
                post_window,
                window_time_frame));
    }

    return DigitalIntervalSeries::createOverlapping(std::move(intervals), window_time_frame);
}

/**
 * @brief Compute kept row indices for greedy overlap pruning of interval bounds.
 *
 * @param windows Candidate window series.
 * @return Kept row indices in input order.
 * @pre windows must be non-null and sorted by start time.
 * @post Touching windows are treated as overlapping.
 */
[[nodiscard]] inline std::vector<size_t> keptWindowIndices(DigitalIntervalSeries const & windows) {
    std::vector<Interval> intervals;
    intervals.reserve(windows.size());
    for (auto const & window: windows.view()) {
        intervals.push_back(window.interval);
    }

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

/**
 * @brief Prune windows and companion alignment points together.
 *
 * @param windows Candidate window series.
 * @param alignment_points Companion event series with matching row order.
 * @return PreparedAlignmentWindows containing pruned windows and alignment events.
 * @pre windows and alignment_points must be non-null and have the same row count.
 * @post Output windows and alignment points have matching row counts.
 */
[[nodiscard]] inline PreparedAlignmentWindows prunePreparedAlignmentWindows(
        std::shared_ptr<DigitalIntervalSeries const> const & windows,
        std::shared_ptr<DigitalEventSeries const> const & alignment_points) {
    assert(windows && "prunePreparedAlignmentWindows: windows must not be null");
    assert(alignment_points && "prunePreparedAlignmentWindows: alignment_points must not be null");
    assert(windows->size() == alignment_points->size() &&
           "prunePreparedAlignmentWindows: row counts must match");

    auto const kept_indices = keptWindowIndices(*windows);

    Neuralyzer::Transforms::V2::ComputeContext context;
    auto pruned_windows = Neuralyzer::Transforms::V2::Examples::pruneOverlappingIntervals(
            *windows,
            Neuralyzer::Transforms::V2::Examples::PruneOverlappingIntervalsParams{},
            context);

    return PreparedAlignmentWindows{
            .windows = std::move(pruned_windows),
            .alignment_points = createFilteredEventSeries(alignment_points, kept_indices)};
}

/**
 * @brief Prepare event-aligned gather windows.
 *
 * @param alignment_events Events defining one row per event.
 * @param pre_window Absolute time units before each event.
 * @param post_window Absolute time units after each event.
 * @param window_time_frame TimeFrame in which windows should be expressed.
 * @param prevent_overlap Whether to greedily prune overlapping windows.
 * @return Prepared windows and companion alignment events.
 * @pre alignment_events must be non-null.
 * @post If valid, windows and alignment_points have matching row counts.
 */
[[nodiscard]] inline PreparedAlignmentWindows prepareEventAlignmentWindows(
        std::shared_ptr<DigitalEventSeries const> const & alignment_events,
        int64_t pre_window,
        int64_t post_window,
        std::shared_ptr<TimeFrame> const & window_time_frame,
        bool prevent_overlap = false) {
    if (!alignment_events) {
        return {};
    }

    auto windows = createWindowsAroundEvents(
            alignment_events,
            pre_window,
            post_window,
            window_time_frame);
    PreparedAlignmentWindows prepared{
            .windows = std::move(windows),
            .alignment_points = alignment_events};

    if (prevent_overlap) {
        prepared = prunePreparedAlignmentWindows(prepared.windows, prepared.alignment_points);
    }

    return prepared;
}

/**
 * @brief Prepare interval-aligned gather windows that use the full interval bounds.
 *
 * @param alignment_intervals Intervals defining row windows.
 * @param align Which interval point to use as t=0.
 * @return Prepared windows and companion alignment events.
 * @pre alignment_intervals must be non-null.
 * @post If valid, windows and alignment_points have matching row counts.
 */
[[nodiscard]] inline PreparedAlignmentWindows prepareIntervalAlignmentWindows(
        std::shared_ptr<DigitalIntervalSeries const> const & alignment_intervals,
        AlignmentPoint align = AlignmentPoint::Start) {
    if (!alignment_intervals) {
        return {};
    }

    Neuralyzer::Transforms::V2::ComputeContext context;
    auto alignment_points = Neuralyzer::Transforms::V2::Examples::intervalToEvent(
            *alignment_intervals,
            Neuralyzer::Transforms::V2::Examples::IntervalToEventParams{
                    .point = toIntervalEventPoint(align)},
            context);

    return PreparedAlignmentWindows{
            .windows = alignment_intervals,
            .alignment_points = std::move(alignment_points)};
}

/**
 * @brief Prepare interval-aligned gather windows around selected interval points.
 *
 * @param alignment_intervals Intervals that provide alignment points.
 * @param align Which interval point to use as t=0.
 * @param pre_window Absolute time units before each alignment point.
 * @param post_window Absolute time units after each alignment point.
 * @param window_time_frame TimeFrame in which windows should be expressed.
 * @param prevent_overlap Whether to greedily prune overlapping windows.
 * @return Prepared windows and companion alignment events.
 * @pre alignment_intervals must be non-null.
 * @post If valid, windows and alignment_points have matching row counts.
 */
[[nodiscard]] inline PreparedAlignmentWindows prepareIntervalAlignmentWindows(
        std::shared_ptr<DigitalIntervalSeries const> const & alignment_intervals,
        AlignmentPoint align,
        int64_t pre_window,
        int64_t post_window,
        std::shared_ptr<TimeFrame> const & window_time_frame,
        bool prevent_overlap = false) {
    auto prepared = prepareIntervalAlignmentWindows(alignment_intervals, align);
    if (!prepared.isValid()) {
        return prepared;
    }

    auto windows = createWindowsAroundEvents(
            prepared.alignment_points,
            pre_window,
            post_window,
            window_time_frame);
    prepared.windows = std::move(windows);

    if (prevent_overlap) {
        prepared = prunePreparedAlignmentWindows(prepared.windows, prepared.alignment_points);
    }

    return prepared;
}

/**
 * @brief Look up alignment source with optional overlap pruning.
 *
 * @param data_manager DataManager to query.
 * @param alignment_data Full alignment configuration.
 * @return AlignmentSourceResult with a filtered copy when prevent_overlap is enabled.
 * @post Filtering is based on physical display windows around alignment points.
 */
[[nodiscard]] inline AlignmentSourceResult getFilteredAlignmentSource(
        std::shared_ptr<DataManager> const & data_manager,
        PlotAlignmentData const & alignment_data) {
    auto result = getAlignmentSource(data_manager, alignment_data.alignment_event_key);
    if (!result.isValid() || !alignment_data.prevent_overlap) {
        return result;
    }

    double const half_window = alignment_data.window_size / 2.0;
    auto const pre = static_cast<int64_t>(half_window);
    auto const post = static_cast<int64_t>(half_window);

    if (result.is_event_series) {
        auto const times = extractAlignmentTimes(*result.event_series);
        auto const kept = pruneOverlappingAlignmentTimes(times, pre, post);
        if (kept.size() < result.event_series->size()) {
            result.event_series = createFilteredEventSeries(result.event_series, kept);
        }
    } else if (result.is_interval_series) {
        AlignmentPoint const align = toAlignmentPoint(alignment_data.interval_alignment_type);
        auto const times = extractAlignmentTimes(*result.interval_series, align);
        auto const kept = pruneOverlappingAlignmentTimes(times, pre, post);
        if (kept.size() < result.interval_series->size()) {
            result.interval_series = createFilteredIntervalSeries(result.interval_series, kept);
        }
    }

    return result;
}

/**
 * @brief Compute the number of alignment rows that survive overlap pruning.
 *
 * @param data_manager DataManager to query.
 * @param alignment_data Alignment configuration.
 * @return Number of non-overlapping alignment rows, or 0 on error.
 * @post The count uses the same physical-window pruning policy as plot preparation.
 */
[[nodiscard]] inline size_t countNonOverlappingAlignmentEvents(
        std::shared_ptr<DataManager> const & data_manager,
        PlotAlignmentData const & alignment_data) {
    if (!data_manager || alignment_data.alignment_event_key.empty()) {
        return 0;
    }

    double const half_window = alignment_data.window_size / 2.0;
    auto const pre = static_cast<int64_t>(half_window);
    auto const post = static_cast<int64_t>(half_window);

    DM_DataType const type = data_manager->getType(alignment_data.alignment_event_key);

    if (type == DM_DataType::DigitalEvent) {
        auto events = data_manager->getData<DigitalEventSeries>(alignment_data.alignment_event_key);
        if (!events) {
            return 0;
        }
        auto const times = extractAlignmentTimes(*events);
        return pruneOverlappingAlignmentTimes(times, pre, post).size();
    }

    if (type == DM_DataType::DigitalInterval) {
        auto intervals = data_manager->getData<DigitalIntervalSeries>(alignment_data.alignment_event_key);
        if (!intervals) {
            return 0;
        }
        AlignmentPoint const align = toAlignmentPoint(alignment_data.interval_alignment_type);
        auto const times = extractAlignmentTimes(*intervals, align);
        return pruneOverlappingAlignmentTimes(times, pre, post).size();
    }

    return 0;
}

/**
 * @brief Prepare windows from DataManager and PlotAlignmentData.
 *
 * @param data_manager DataManager containing the alignment source.
 * @param alignment_data Alignment configuration.
 * @param window_time_frame TimeFrame in which generated display windows should be expressed.
 * @return Prepared window/alignment metadata, or an invalid result on error.
 * @post Event and interval display-window paths use physical window units.
 */
[[nodiscard]] inline PreparedAlignmentWindows prepareAlignmentWindows(
        std::shared_ptr<DataManager> const & data_manager,
        PlotAlignmentData const & alignment_data,
        std::shared_ptr<TimeFrame> const & window_time_frame) {
    auto const alignment_source = getAlignmentSource(data_manager, alignment_data.alignment_event_key);
    if (!alignment_source.isValid()) {
        return {};
    }

    double const half_window = alignment_data.window_size / 2.0;
    auto const pre = static_cast<int64_t>(half_window);
    auto const post = static_cast<int64_t>(half_window);

    if (alignment_source.is_event_series) {
        return prepareEventAlignmentWindows(
                alignment_source.event_series,
                pre,
                post,
                window_time_frame,
                alignment_data.prevent_overlap);
    }

    AlignmentPoint const align = toAlignmentPoint(alignment_data.interval_alignment_type);
    return prepareIntervalAlignmentWindows(
            alignment_source.interval_series,
            align,
            pre,
            post,
            window_time_frame,
            alignment_data.prevent_overlap);
}

}// namespace Neuralyzer::Plots

#endif// PLOT_ALIGNMENT_WINDOW_PREPARATION_HPP
