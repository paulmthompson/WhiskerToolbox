#ifndef PLOT_ALIGNMENT_GATHER_HPP
#define PLOT_ALIGNMENT_GATHER_HPP

/**
 * @file PlotAlignmentGather.hpp
 * @brief Free functions for creating aligned GatherResults from plot alignment settings
 *
 * This header provides testable free functions that create GatherResult objects
 * based on alignment configuration. These functions support:
 *
 * 1. **DigitalEventSeries alignment**: Events expanded to intervals using window size
 * 2. **DigitalIntervalSeries alignment**: Intervals with start/end alignment point selection
 * 3. **Dynamic window sizing**: Configurable pre/post window around alignment events
 *
 * ## Usage Examples
 *
 * @code
 * // Basic usage with DataManager and alignment state
 * auto result = createAlignedGatherResult<DigitalEventSeries>(
 *     data_manager, "spikes", alignment_state);
 *
 * // Direct usage with explicit parameters
 * auto result = gatherWithEventAlignment<DigitalEventSeries>(
 *     spikes, alignment_events, 100.0, 100.0);  // ±100 window
 *
 * auto result = gatherWithIntervalAlignment<DigitalEventSeries>(
 *     spikes, trial_intervals, AlignmentPoint::Start);
 * @endcode
 *
 * @see GatherResult for the returned container type
 * @see PlotAlignmentState for the alignment configuration source
 * @see IntervalAdapters.hpp for the underlying adapter implementations
 */

#include "DataManager/DataManager.hpp"
#include "GatherResult/GatherResult.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "TransformsV2/extension/IntervalAdapters.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiskerToolbox::Plots {

using WhiskerToolbox::Transforms::V2::AlignmentPoint;
using WhiskerToolbox::Transforms::V2::expandEvents;
using WhiskerToolbox::Transforms::V2::withAlignment;

// =============================================================================
// Type Conversion Helpers
// =============================================================================

/**
 * @brief Convert IntervalAlignmentType to AlignmentPoint
 *
 * Maps the UI-facing enum to the GatherResult API enum.
 *
 * @param type UI alignment type (Beginning/End)
 * @return Corresponding AlignmentPoint for GatherResult adapters
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

// =============================================================================
// Low-Level Gather Functions (Testable Building Blocks)
// =============================================================================

/**
 * @brief Gather data aligned to a DigitalEventSeries with window expansion
 *
 * Each event in the alignment series becomes an interval centered on
 * the event time, extended by pre_window before and post_window after.
 *
 * @tparam T Data type to gather (e.g., DigitalEventSeries, AnalogTimeSeries)
 * @param source Source data to create views from
 * @param alignment_events Events defining alignment points
 * @param pre_window Time units before each event to include
 * @param post_window Time units after each event to include
 * @return GatherResult with one view per alignment event
 *
 * @example
 * @code
 * auto spikes = dm->getData<DigitalEventSeries>("spikes");
 * auto stim_events = dm->getData<DigitalEventSeries>("stimuli");
 *
 * // Each stimulus ± 100 ms
 * auto raster = gatherWithEventAlignment(spikes, stim_events, 100.0, 100.0);
 *
 * // Access trial 0's spikes relative to first stimulus
 * for (auto const& event : raster[0]->view()) {
 *     auto relative_time = event.time().getValue() - raster.intervalAt(0).alignment_time;
 * }
 * @endcode
 */
template<typename T>
[[nodiscard]] GatherResult<T> gatherWithEventAlignment(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalEventSeries> alignment_events,
        double pre_window,
        double post_window) {

    if (!source || !alignment_events) {
        return GatherResult<T>{};
    }

    auto adapter = expandEvents(
            alignment_events,
            static_cast<int64_t>(pre_window),
            static_cast<int64_t>(post_window));

    return gather(source, adapter);
}


/**
 * @brief Gather data aligned to a DigitalIntervalSeries with alignment point selection
 *
 * Uses the full interval bounds for data gathering, but allows specifying
 * which point within each interval to use as the alignment reference.
 *
 * @tparam T Data type to gather (e.g., DigitalEventSeries, AnalogTimeSeries)
 * @param source Source data to create views from
 * @param alignment_intervals Intervals defining trial boundaries
 * @param align Which point in each interval to use for alignment (Start, End, Center)
 * @return GatherResult with one view per interval
 *
 * @example
 * @code
 * auto spikes = dm->getData<DigitalEventSeries>("spikes");
 * auto trials = dm->getData<DigitalIntervalSeries>("trials");
 *
 * // Align to trial end (e.g., for response-locked analysis)
 * auto raster = gatherWithIntervalAlignment(spikes, trials, AlignmentPoint::End);
 * @endcode
 */
template<typename T>
[[nodiscard]] GatherResult<T> gatherWithIntervalAlignment(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries> alignment_intervals,
        AlignmentPoint align = AlignmentPoint::Start) {

    if (!source || !alignment_intervals) {
        return GatherResult<T>{};
    }

    auto adapter = withAlignment(alignment_intervals, align);
    return gather(source, adapter);
}

/**
 * @brief Gather data aligned to a DigitalIntervalSeries with explicit window
 *
 * Uses the specified alignment point (Start/End/Center) of each interval,
 * but creates a view window of [alignment - pre_window, alignment + post_window]
 * in absolute time units rather than using the raw interval bounds.
 *
 * This is essential for raster plots where the user specifies a display window
 * that extends beyond the interval boundaries.
 *
 * @tparam T Data type to gather (e.g., DigitalEventSeries, AnalogTimeSeries)
 * @param source Source data to create views from
 * @param alignment_intervals Intervals defining alignment points
 * @param align Which point in each interval to use for alignment
 * @param pre_window Absolute time units before alignment to include
 * @param post_window Absolute time units after alignment to include
 * @return GatherResult with one view per interval
 */
template<typename T>
[[nodiscard]] GatherResult<T> gatherWithIntervalAlignment(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries> alignment_intervals,
        AlignmentPoint align,
        double pre_window,
        double post_window) {

    if (!source || !alignment_intervals) {
        return GatherResult<T>{};
    }

    auto adapter = withAlignment(
            alignment_intervals, align,
            static_cast<int64_t>(pre_window),
            static_cast<int64_t>(post_window));
    return gather(source, adapter);
}

// =============================================================================
// Overlap Pruning Functions
// =============================================================================

/**
 * @brief Prune alignment times whose expanded windows would overlap
 *
 * Uses a greedy left-to-right scan: keeps the first alignment time, then for
 * each subsequent time checks whether its window [time - pre_window,
 * time + post_window] overlaps with the window of the last *kept* time.
 * If it overlaps, the event is skipped; otherwise it is kept.
 *
 * Alignment times must be sorted in ascending order.
 *
 * @param alignment_times Sorted absolute alignment times
 * @param pre_window Time units before each alignment time
 * @param post_window Time units after each alignment time
 * @return Indices into alignment_times that survive pruning
 *
 * @pre alignment_times is sorted in ascending order
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
 * @brief Extract absolute alignment times from a DigitalEventSeries
 *
 * Converts each event's TimeFrameIndex to absolute time using the series'
 * TimeFrame. If the series has no TimeFrame, uses the raw index value.
 *
 * @param events Event series to extract times from
 * @return Vector of absolute times (sorted, matching event order)
 */
[[nodiscard]] inline std::vector<int64_t> extractAlignmentTimes(
        DigitalEventSeries const & events) {

    std::vector<int64_t> times;
    times.reserve(events.size());

    auto tf = events.getTimeFrame();
    for (auto const & ev: events.view()) {
        if (tf) {
            times.push_back(tf->getTimeAtIndex(ev.time()));
        } else {
            times.push_back(ev.time().getValue());
        }
    }
    return times;
}

/**
 * @brief Extract absolute alignment times from a DigitalIntervalSeries
 *
 * Extracts the alignment point (Start, End, or Center) for each interval
 * and converts it to absolute time using the series' TimeFrame.
 *
 * @param intervals Interval series to extract from
 * @param align Which point in each interval to use
 * @return Vector of absolute alignment times (sorted, matching interval order)
 */
[[nodiscard]] inline std::vector<int64_t> extractAlignmentTimes(
        DigitalIntervalSeries const & intervals,
        AlignmentPoint align) {

    std::vector<int64_t> times;
    times.reserve(intervals.size());

    auto tf = intervals.getTimeFrame();
    for (auto const & iv: intervals.view()) {
        int64_t index{};
        switch (align) {
            case AlignmentPoint::Start:
                index = iv.interval.start;
                break;
            case AlignmentPoint::End:
                index = iv.interval.end;
                break;
            case AlignmentPoint::Center:
                index = (iv.interval.start + iv.interval.end) / 2;
                break;
        }
        if (tf) {
            times.push_back(tf->getTimeAtIndex(TimeFrameIndex(index)));
        } else {
            times.push_back(index);
        }
    }
    return times;
}

/**
 * @brief Create a filtered copy of a DigitalEventSeries keeping only specified indices
 *
 * The returned series shares the same TimeFrame as the source but is a new
 * owning series (not registered in DataManager).
 *
 * @param source Source event series
 * @param kept_indices Indices (into the source's view order) to keep
 * @return New DigitalEventSeries containing only the specified events
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> createFilteredEventSeries(
        std::shared_ptr<DigitalEventSeries> const & source,
        std::vector<size_t> const & kept_indices) {

    std::vector<TimeFrameIndex> kept_events;
    kept_events.reserve(kept_indices.size());

    size_t idx = 0;
    size_t kept_pos = 0;
    for (auto const & ev: source->view()) {
        if (kept_pos < kept_indices.size() && idx == kept_indices[kept_pos]) {
            kept_events.push_back(ev.time());
            ++kept_pos;
        }
        ++idx;
    }

    auto filtered = std::make_shared<DigitalEventSeries>(std::move(kept_events));
    filtered->setTimeFrame(source->getTimeFrame());
    return filtered;
}

/**
 * @brief Create a filtered copy of a DigitalIntervalSeries keeping only specified indices
 *
 * @param source Source interval series
 * @param kept_indices Indices (into the source's view order) to keep
 * @return New DigitalIntervalSeries containing only the specified intervals
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries> createFilteredIntervalSeries(
        std::shared_ptr<DigitalIntervalSeries> const & source,
        std::vector<size_t> const & kept_indices) {

    std::vector<Interval> kept_intervals;
    kept_intervals.reserve(kept_indices.size());

    size_t idx = 0;
    size_t kept_pos = 0;
    for (auto const & iv: source->view()) {
        if (kept_pos < kept_indices.size() && idx == kept_indices[kept_pos]) {
            kept_intervals.push_back(iv.interval);
            ++kept_pos;
        }
        ++idx;
    }

    auto filtered = std::make_shared<DigitalIntervalSeries>(std::move(kept_intervals));
    filtered->setTimeFrame(source->getTimeFrame());
    return filtered;
}

/**
 * @brief Compute the number of alignment events that survive overlap pruning
 *
 * Lightweight function for UI display — computes the count without creating
 * a filtered series.
 *
 * @param data_manager DataManager to query
 * @param alignment_data Alignment configuration (includes window_size and prevent_overlap)
 * @return Number of non-overlapping alignment events, or 0 on error
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
        auto events = data_manager->getData<DigitalEventSeries>(
                alignment_data.alignment_event_key);
        if (!events) return 0;
        auto times = extractAlignmentTimes(*events);
        return pruneOverlappingAlignmentTimes(times, pre, post).size();
    } else if (type == DM_DataType::DigitalInterval) {
        auto intervals = data_manager->getData<DigitalIntervalSeries>(
                alignment_data.alignment_event_key);
        if (!intervals) return 0;
        AlignmentPoint const align = toAlignmentPoint(alignment_data.interval_alignment_type);
        auto times = extractAlignmentTimes(*intervals, align);
        return pruneOverlappingAlignmentTimes(times, pre, post).size();
    }

    return 0;
}

// =============================================================================
// High-Level Integration Functions
// =============================================================================

/**
 * @brief Result of alignment source lookup
 *
 * Contains either the alignment series or an error message.
 */
struct AlignmentSourceResult {
    std::shared_ptr<DigitalEventSeries> event_series;
    std::shared_ptr<DigitalIntervalSeries> interval_series;
    bool is_event_series = false;
    bool is_interval_series = false;
    std::string error_message;

    [[nodiscard]] bool isValid() const noexcept {
        return is_event_series || is_interval_series;
    }
};

/**
 * @brief Look up alignment series from DataManager
 *
 * Determines whether the alignment key refers to a DigitalEventSeries
 * or DigitalIntervalSeries and retrieves the appropriate type.
 *
 * @param data_manager DataManager to query
 * @param alignment_key Key of the alignment series
 * @return AlignmentSourceResult with the series or error info
 */
[[nodiscard]] inline AlignmentSourceResult getAlignmentSource(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & alignment_key) {

    AlignmentSourceResult result;

    if (!data_manager || alignment_key.empty()) {
        result.error_message = "Invalid data manager or empty alignment key";
        return result;
    }

    DM_DataType type = data_manager->getType(alignment_key);

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
 * @brief Look up alignment source with optional overlap pruning
 *
 * When alignment_data.prevent_overlap is true, the returned series is a
 * filtered copy containing only non-overlapping alignment events.
 * When false, this behaves identically to getAlignmentSource().
 *
 * Pruning for intervals is based on the **alignment point** (Start/End/Center),
 * not raw interval bounds. An interval whose raw end overlaps another
 * interval's alignment-point window is NOT pruned.
 *
 * @param data_manager DataManager to query
 * @param alignment_data Full alignment configuration
 * @return AlignmentSourceResult with (optionally filtered) series
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
        auto times = extractAlignmentTimes(*result.event_series);
        auto kept = pruneOverlappingAlignmentTimes(times, pre, post);
        if (kept.size() < result.event_series->size()) {
            result.event_series = createFilteredEventSeries(result.event_series, kept);
        }
    } else if (result.is_interval_series) {
        AlignmentPoint const align = toAlignmentPoint(alignment_data.interval_alignment_type);
        auto times = extractAlignmentTimes(*result.interval_series, align);
        auto kept = pruneOverlappingAlignmentTimes(times, pre, post);
        if (kept.size() < result.interval_series->size()) {
            result.interval_series = createFilteredIntervalSeries(result.interval_series, kept);
        }
    }

    return result;
}

/**
 * @brief Create aligned GatherResult using PlotAlignmentData configuration
 *
 * This is the main entry point for widgets. It automatically handles:
 * - Determining alignment source type (event vs interval series)
 * - Using the appropriate adapter (expandEvents vs withAlignment)
 * - Applying window size for event alignment
 * - Applying alignment point for interval alignment
 *
 * @tparam T Data type to gather (e.g., DigitalEventSeries, AnalogTimeSeries)
 * @param data_manager DataManager containing all data
 * @param source_key Key of the source data to gather
 * @param alignment_data Alignment configuration (from PlotAlignmentState)
 * @return GatherResult with aligned views, or empty result on error
 *
 * @example
 * @code
 * // In a widget, using PlotAlignmentState
 * auto alignment_state = _state->alignmentState();
 * auto result = createAlignedGatherResult<DigitalEventSeries>(
 *     _data_manager,
 *     "spikes",
 *     alignment_state->data());
 * @endcode
 */
template<typename T>
[[nodiscard]] GatherResult<T> createAlignedGatherResult(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & source_key,
        PlotAlignmentData const & alignment_data) {

    if (!data_manager || source_key.empty()) {
        return GatherResult<T>{};
    }

    // Get source data
    auto source = data_manager->getData<T>(source_key);
    if (!source) {
        return GatherResult<T>{};
    }

    // Get alignment source (with optional overlap pruning)
    auto alignment_source = getFilteredAlignmentSource(data_manager, alignment_data);
    if (!alignment_source.isValid()) {
        return GatherResult<T>{};
    }

    // Create GatherResult based on alignment source type
    if (alignment_source.is_event_series) {
        // For event series: use window_size to create symmetric window
        // The window is centered on each event, so we use window_size/2 on each side
        double half_window = alignment_data.window_size / 2.0;
        return gatherWithEventAlignment<T>(
                source,
                alignment_source.event_series,
                half_window,
                half_window);
    } else {
        // For interval series: use the specified alignment point with window expansion
        double half_window = alignment_data.window_size / 2.0;
        AlignmentPoint align = toAlignmentPoint(alignment_data.interval_alignment_type);
        return gatherWithIntervalAlignment<T>(
                source,
                alignment_source.interval_series,
                align,
                half_window,
                half_window);
    }
}

/**
 * @brief Overload using PlotAlignmentState pointer
 *
 * Convenience overload that extracts data() from the state object.
 */
template<typename T>
[[nodiscard]] GatherResult<T> createAlignedGatherResult(
        std::shared_ptr<DataManager> const & data_manager,
        std::string const & source_key,
        PlotAlignmentState const * alignment_state) {

    if (!alignment_state) {
        return GatherResult<T>{};
    }

    return createAlignedGatherResult<T>(data_manager, source_key, alignment_state->data());
}

}// namespace WhiskerToolbox::Plots

#endif// PLOT_ALIGNMENT_GATHER_HPP
