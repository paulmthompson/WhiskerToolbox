#ifndef DERIVED_TIMEFRAME_HPP
#define DERIVED_TIMEFRAME_HPP

#include "TimeFrame/TimeFrame.hpp"

#include <memory>

class DigitalIntervalSeries;
class DigitalEventSeries;

/**
 * @brief Mode for selecting which edge of intervals to use when creating derived TimeFrames
 */
enum class IntervalEdge {
    START,///< Use the start time of each interval
    END   ///< Use the end time of each interval
};

/**
 * @brief Options for creating a derived TimeFrame from a DigitalIntervalSeries
 */
struct DerivedTimeFrameFromIntervalsOptions {
    std::shared_ptr<TimeFrame> source_timeframe;           ///< The source TimeFrame to sample from
    std::shared_ptr<DigitalIntervalSeries> interval_series;///< The intervals defining which indices to sample
    IntervalEdge edge = IntervalEdge::START;               ///< Which edge of intervals to use
};

/**
 * @brief Options for creating a derived TimeFrame from a DigitalEventSeries
 */
struct DerivedTimeFrameFromEventsOptions {
    std::shared_ptr<TimeFrame> source_timeframe;     ///< The source TimeFrame to sample from
    std::shared_ptr<DigitalEventSeries> event_series;///< The events defining which indices to sample
};

/**
 * @brief Create a derived TimeFrame by sampling a source TimeFrame at indices from a DigitalIntervalSeries
 * 
 * This function creates a new TimeFrame by extracting time values from the source TimeFrame
 * at the indices specified by the start or end times of intervals in the DigitalIntervalSeries.
 * 
 * Use case: When you have a master clock (e.g., 30kHz acquisition) and camera trigger intervals,
 * you can create a camera-specific TimeFrame with the actual timestamps when each camera frame
 * was captured.
 * 
 * @param options Configuration options including source timeframe, interval series, and edge selection
 * @return A shared pointer to the created TimeFrame, or nullptr on failure
 * 
 * @example
 * ```cpp
 * // Given a 30kHz master clock and camera trigger intervals
 * DerivedTimeFrameFromIntervalsOptions opts;
 * opts.source_timeframe = master_clock;      // 30kHz TimeFrame
 * opts.interval_series = camera_triggers;    // DigitalIntervalSeries with camera frame triggers
 * opts.edge = IntervalEdge::START;           // Use start of each interval
 * 
 * auto camera_timeframe = createDerivedTimeFrame(opts);
 * // camera_timeframe now contains the ~500Hz timestamps for each camera frame
 * ```
 */
std::shared_ptr<TimeFrame> createDerivedTimeFrame(DerivedTimeFrameFromIntervalsOptions const & options);

/**
 * @brief Create a derived TimeFrame by sampling a source TimeFrame at indices from a DigitalEventSeries
 * 
 * This function creates a new TimeFrame by extracting time values from the source TimeFrame
 * at the indices specified by the events in the DigitalEventSeries.
 * 
 * Use case: When you have a master clock and discrete trigger events (e.g., TTL pulses marking
 * frame acquisitions), you can create a derived TimeFrame with the actual timestamps.
 * 
 * @param options Configuration options including source timeframe and event series
 * @return A shared pointer to the created TimeFrame, or nullptr on failure
 * 
 * @example
 * ```cpp
 * // Given a 30kHz master clock and camera trigger events
 * DerivedTimeFrameFromEventsOptions opts;
 * opts.source_timeframe = master_clock;    // 30kHz TimeFrame
 * opts.event_series = camera_triggers;     // DigitalEventSeries with camera frame triggers
 * 
 * auto camera_timeframe = createDerivedTimeFrame(opts);
 * // camera_timeframe now contains the ~500Hz timestamps for each camera frame
 * ```
 */
std::shared_ptr<TimeFrame> createDerivedTimeFrame(DerivedTimeFrameFromEventsOptions const & options);

/**
 * @brief Options for creating an upsampled TimeFrame by interpolating between source clock values
 */
struct DerivedTimeFrameFromUpsamplingOptions {
    std::shared_ptr<TimeFrame> source_timeframe;///< The source TimeFrame to upsample
    int upsampling_factor;                      ///< Integer upsampling factor (must be > 0)
};

/**
 * @brief Create an upsampled TimeFrame by linearly interpolating between consecutive clock values
 *
 * Given a source TimeFrame with N entries and clock values [t_0, t_1, ..., t_{N-1}],
 * produces a new TimeFrame with (N-1) * factor + 1 entries by inserting factor - 1
 * evenly spaced values between each consecutive pair.
 *
 * Non-integer interpolation results are rounded to the nearest integer tick.
 *
 * @pre options.source_timeframe must not be null
 * @pre options.upsampling_factor must be > 0
 *
 * @param options Configuration including source timeframe and upsampling factor
 * @return A shared pointer to the created TimeFrame, or nullptr on invalid input
 *
 * @example
 * ```cpp
 * // Source: [0, 60, 120], factor = 4
 * // Output: [0, 15, 30, 45, 60, 75, 90, 105, 120] (9 entries)
 * DerivedTimeFrameFromUpsamplingOptions opts;
 * opts.source_timeframe = source;
 * opts.upsampling_factor = 4;
 * auto upsampled = createUpsampledTimeFrame(opts);
 * ```
 */
std::shared_ptr<TimeFrame> createUpsampledTimeFrame(DerivedTimeFrameFromUpsamplingOptions const & options);

#endif// DERIVED_TIMEFRAME_HPP
