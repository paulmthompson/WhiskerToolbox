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
#include "DataManager/utils/GatherResult.hpp"
#include "DataManager/transforms/v2/extension/IntervalAdapters.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "PlotAlignmentWidget/Core/PlotAlignmentState.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

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
        std::string const& alignment_key) {

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
        std::string const& source_key,
        PlotAlignmentData const& alignment_data) {

    if (!data_manager || source_key.empty()) {
        return GatherResult<T>{};
    }

    // Get source data
    auto source = data_manager->getData<T>(source_key);
    if (!source) {
        return GatherResult<T>{};
    }

    // Get alignment source
    auto alignment_source = getAlignmentSource(data_manager, alignment_data.alignment_event_key);
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
        // For interval series: use the specified alignment point
        AlignmentPoint align = toAlignmentPoint(alignment_data.interval_alignment_type);
        return gatherWithIntervalAlignment<T>(
            source, 
            alignment_source.interval_series, 
            align);
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
        std::string const& source_key,
        PlotAlignmentState const* alignment_state) {

    if (!alignment_state) {
        return GatherResult<T>{};
    }

    return createAlignedGatherResult<T>(data_manager, source_key, alignment_state->data());
}

} // namespace WhiskerToolbox::Plots

#endif // PLOT_ALIGNMENT_GATHER_HPP
