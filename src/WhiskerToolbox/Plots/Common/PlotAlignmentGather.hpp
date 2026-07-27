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
 * @see PlotAlignmentWindowPreparation.hpp for the underlying window preparation helpers
 */

#include "DataManager/DataManager.hpp"
#include "GatherResult/GatherResult.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentState.hpp"
#include "PlotAlignmentWindowPreparation.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace Neuralyzer::Plots {

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

    auto prepared = prepareEventAlignmentWindows(
            alignment_events,
            static_cast<int64_t>(pre_window),
            static_cast<int64_t>(post_window),
            source->getTimeFrame());

    if (!prepared.isValid()) {
        return GatherResult<T>{};
    }

    return gather(source, prepared.windows, prepared.alignment_points);
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

    auto prepared = prepareIntervalAlignmentWindows(alignment_intervals, align);
    if (!prepared.isValid()) {
        return GatherResult<T>{};
    }

    return gather(source, prepared.windows, prepared.alignment_points);
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

    auto prepared = prepareIntervalAlignmentWindows(
            alignment_intervals,
            align,
            static_cast<int64_t>(pre_window),
            static_cast<int64_t>(post_window),
            source->getTimeFrame());

    if (!prepared.isValid()) {
        return GatherResult<T>{};
    }

    return gather(source, prepared.windows, prepared.alignment_points);
}

// =============================================================================
// High-Level Integration Functions
// =============================================================================

/**
 * @brief Create aligned GatherResult using PlotAlignmentData configuration
 *
 * This is the main entry point for widgets. It automatically handles:
 * - Determining alignment source type (event vs interval series)
 * - Preparing DigitalIntervalSeries windows and companion alignment events
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

    auto prepared = prepareAlignmentWindows(
            data_manager,
            alignment_data,
            source->getTimeFrame());
    if (!prepared.isValid()) {
        return GatherResult<T>{};
    }

    return gather(source, prepared.windows, prepared.alignment_points);
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

}// namespace Neuralyzer::Plots

#endif// PLOT_ALIGNMENT_GATHER_HPP
