#ifndef COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP
#define COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP

#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "MapperConcepts.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <ranges>
#include <vector>

namespace CorePlotting {

/**
 * @brief Mapper for time-series data (DataViewer-style visualization)
 * 
 * Transforms time-indexed data from DataManager types into world-space coordinates
 * suitable for rendering and hit testing. X coordinates come from TimeFrame conversion,
 * Y coordinates from layout allocation.
 * 
 * ## Choosing the Right Mapper
 * 
 * **Events (DigitalEventSeries):**
 * - mapEvents(): Lazy range over ALL events - use when iterating full series
 * - mapEventsInRange(): Filtered by time range with cross-TimeFrame support - use for visible window
 * - mapEventsToVector(): Materialized full series - use when random access needed
 * 
 * **Intervals (DigitalIntervalSeries):**
 * - mapIntervals(): Lazy range over ALL intervals
 * - mapIntervalsInRange(): Filtered by time range with clipping - use for visible window
 * - mapIntervalsToVector(): Materialized full series
 * 
 * **Analog (AnalogTimeSeries):**
 * - mapAnalogSeries(): Lazy range over time range - basic vertex output
 * - mapAnalogSeriesWithIndices(): Includes time_index for gap detection
 * - mapAnalogSeriesFull(): Lazy range over full series
 * - mapAnalogToVector(): Materialized version
 * 
 * ## Model-Matrix Rendering Pattern
 * 
 * For OpenGL rendering with model matrices, create a "local-space" SeriesLayout
 * with y_center=0 and height=2 (for [-1,1] range). The model matrix handles
 * world-space positioning:
 * 
 * @code
 * SeriesLayout local_layout{SeriesLayoutResult{0.0f, 2.0f}, "", 0};
 * auto events = mapEventsInRange(series, local_layout, time_frame, start, end);
 * @endcode
 * 
 * This mapper is used for:
 * - DataViewer: stacked time-series plots
 * - Event traces: discrete events along time axis
 * - Interval display: temporal regions/epochs
 * 
 * All range-returning methods support zero-copy single-traversal consumption.
 */
namespace TimeSeriesMapper {

// ============================================================================
// Event Mapping: DigitalEventSeries → MappedElement range
// ============================================================================

/**
 * @brief Map events to world-space positions
 * 
 * Transforms DigitalEventSeries events into (x, y, entity_id) tuples where:
 * - X = absolute time from TimeFrame
 * - Y = layout.y_transform.offset (constant for all events)
 * 
 * Returns a lazy range over ALL events. For visible-window filtering,
 * use mapEventsInRange() instead.
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning
 * @param time_frame TimeFrame for index→time conversion
 * @return Range of MappedElement
 * 
 * @see mapEventsInRange For time-range filtering with cross-TimeFrame support
 * @see mapEventsToVector For materialized output
 */
[[nodiscard]] inline auto mapEvents(
        DigitalEventSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame) {
    float const y_center = layout.y_transform.offset;

    return series.view() | std::views::transform([&time_frame, y_center](auto const & event_with_id) {
               float x = static_cast<float>(time_frame.getTimeAtIndex(event_with_id.event_time));
               return MappedElement{x, y_center, event_with_id.entity_id};
           });
}

/**
 * @brief Map events in a time range to world-space positions
 * 
 * Uses getEventsWithIdsInRange for proper cross-TimeFrame support and
 * EntityId preservation. The query_time_frame defines the coordinate
 * system for start_time/end_time, while the series' internal TimeFrame
 * is used for the actual event positions.
 * 
 * Returns an owning range that can be iterated or materialized at the call site.
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning  
 * @param query_time_frame TimeFrame for range query (defines start/end coordinate space)
 * @param start_time Start of visible range (in query_time_frame coordinates)
 * @param end_time End of visible range (in query_time_frame coordinates)
 * @return Owning range of MappedElement for events in range
 * 
 * @see mapEvents For mapping all events without range filtering
 * @see mapEventsToVector For materializing full series
 */
[[nodiscard]] inline auto mapEventsInRange(
        DigitalEventSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & query_time_frame,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time) {
    float const y_center = layout.y_transform.offset;
    auto const * series_tf = series.getTimeFrame().get();

    // views::all on an rvalue vector creates an owning_view
    return series.viewInRange(start_time, end_time, query_time_frame) |
           std::views::transform([series_tf, y_center](auto const & event) {
               float const x = static_cast<float>(series_tf->getTimeAtIndex(event.event_time));
               return MappedElement{x, y_center, event.entity_id};
           });
}

// ============================================================================
// Interval Mapping: DigitalIntervalSeries → MappedRectElement range
// ============================================================================

/**
 * @brief Map intervals to world-space rectangles
 * 
 * Transforms DigitalIntervalSeries into (x, y, width, height, entity_id) tuples where:
 * - X = absolute start time from TimeFrame
 * - Width = absolute end time - start time
 * - Y = layout.y_transform.offset - height/2
 * - Height = layout.y_transform.gain * 2 (full allocated height)
 * 
 * @param series Interval series to map
 * @param layout Layout allocation for Y positioning and height
 * @param time_frame TimeFrame for index→time conversion
 * @return Range of MappedRectElement
 * 
 * @see mapIntervalsInRange For mapping with time range filtering
 * @see mapIntervalsToVector For materializing to a vector
 */
[[nodiscard]] inline auto mapIntervals(
        DigitalIntervalSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame) {
    float const y_center = layout.y_transform.offset;
    float const height = layout.y_transform.gain * 2.0f;// gain is half-height
    float const y_bottom = y_center - height / 2.0f;

    return series.view() | std::views::transform([&time_frame, y_bottom, height](auto const & interval_with_id) {
               float x_start = static_cast<float>(
                       time_frame.getTimeAtIndex(TimeFrameIndex{interval_with_id.interval.start}));
               float x_end = static_cast<float>(
                       time_frame.getTimeAtIndex(TimeFrameIndex{interval_with_id.interval.end}));
               float width = x_end - x_start;
               return MappedRectElement{x_start, y_bottom, width, height, interval_with_id.entity_id};
           });
}

/**
 * @brief Map intervals in a time range to world-space rectangles
 * 
 * Uses getIntervalsWithIdsInRange for proper cross-TimeFrame support.
 * Intervals are clipped to the visible range.
 * 
 * Returns an owning range that can be iterated or materialized at the call site.
 * 
 * @param series Interval series to map
 * @param layout Layout allocation for Y positioning and height
 * @param query_time_frame TimeFrame for range query (defines start/end coordinate space)
 * @param start_time Start of visible range (in query_time_frame coordinates)
 * @param end_time End of visible range (in query_time_frame coordinates)
 * @return Owning range of MappedRectElement for intervals in range
 * 
 * @see mapIntervals For mapping all intervals without range filtering
 * @see mapIntervalsToVector For materializing full series
 */
[[nodiscard]] inline auto mapIntervalsInRange(
        DigitalIntervalSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & query_time_frame,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time) {
    float const y_center = layout.y_transform.offset;
    float const height = layout.y_transform.gain * 2.0f;// gain is half-height
    float const y_bottom = y_center - height / 2.0f;

    float const start_time_f = static_cast<float>(query_time_frame.getTimeAtIndex(start_time));
    float const end_time_f = static_cast<float>(query_time_frame.getTimeAtIndex(end_time));

    auto const * series_tf = series.getTimeFrame().get();

    // views::all on an rvalue vector creates an owning_view
    return series.viewInRange(start_time, end_time, query_time_frame) | std::views::transform([series_tf, y_bottom, height, start_time_f, end_time_f](auto const & interval_with_id) {
               auto x_start = static_cast<float>(
                       series_tf->getTimeAtIndex(TimeFrameIndex(interval_with_id.interval.start)));
               auto x_end = static_cast<float>(
                       series_tf->getTimeAtIndex(TimeFrameIndex(interval_with_id.interval.end)));

               // Clip to visible range
               x_start = std::max(x_start, start_time_f);
               x_end = std::min(x_end, end_time_f);

               float const width = x_end - x_start;

               return MappedRectElement{x_start, y_bottom, width, height, interval_with_id.entity_id};
           });
}

// ============================================================================
// Analog Mapping: AnalogTimeSeries → MappedVertex/MappedAnalogVertex range
// ============================================================================

// MappedAnalogVertex is defined in MappedElement.hpp

/**
 * @brief Map analog time series to polyline vertices
 * 
 * Transforms AnalogTimeSeries samples into (x, y) vertices where:
 * - X = absolute time from TimeFrame
 * - Y = layout-transformed data value
 * 
 * The layout transform scales and offsets the raw values:
 * Y = value * y_scale + y_offset
 * 
 * Where:
 * - y_scale = allocated_height / (2.0 * data_range) if normalizing
 * - y_offset = allocated_y_center
 * 
 * @param series Analog time series to map
 * @param layout Layout allocation for Y positioning and scaling
 * @param time_frame TimeFrame for index→time conversion
 * @param y_scale Scaling factor for Y values (e.g., 1/std_dev for normalization)
 * @param start_time Start of visible range
 * @param end_time End of visible range
 * @return Range of MappedVertex
 * 
 * @see mapAnalogSeriesWithIndices For gap detection support
 * @see mapAnalogSeriesFull For mapping the full series
 * @see mapAnalogToVector For materializing to a vector
 */
[[nodiscard]] inline auto mapAnalogSeries(
        AnalogTimeSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & query_time_frame,
        float y_scale,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time) {
    float const y_offset = layout.y_transform.offset;
    auto const * series_tf = series.getTimeFrame().get();

    // Use cross-timeframe query: start_time/end_time are in query_time_frame coordinates
    return series.getTimeValueRangeInTimeFrameIndexRange(start_time, end_time, query_time_frame) | std::views::transform([series_tf, y_scale, y_offset](auto const & tv_point) {
               // Data's TimeFrameIndex is in series timeframe, use series timeframe for X
               float x = series_tf ? static_cast<float>(series_tf->getTimeAtIndex(tv_point.time_frame_index))
                                   : static_cast<float>(tv_point.time_frame_index.getValue());
               float y = tv_point.value() * y_scale + y_offset;
               return MappedVertex{x, y};
           });
}

/**
 * @brief Map entire analog time series to polyline vertices
 * 
 * Convenience overload for mapping the full series.
 * 
 * @param series Analog time series to map
 * @param layout Layout allocation for Y positioning and scaling
 * @param time_frame TimeFrame for index→time conversion  
 * @param y_scale Scaling factor for Y values
 * @return Range of MappedVertex
 * 
 * @see mapAnalogSeries For range-filtered mapping
 * @see mapAnalogSeriesWithIndices For gap detection support
 */
[[nodiscard]] inline auto mapAnalogSeriesFull(
        AnalogTimeSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame,
        float y_scale) {
    float const y_offset = layout.y_transform.offset;

    return series.view() | std::views::transform([&time_frame, y_scale, y_offset](auto const & tv_point) {
               float x = static_cast<float>(time_frame.getTimeAtIndex(tv_point.time_frame_index));
               float y = tv_point.value() * y_scale + y_offset;
               return MappedVertex{x, y};
           });
}

/**
 * @brief Map analog time series to vertices with time indices for gap detection
 * 
 * Returns MappedAnalogVertex which includes the original time frame index,
 * enabling the caller to detect gaps based on index discontinuities.
 * 
 * @param series Analog time series to map
 * @param layout Layout allocation for Y positioning and scaling
 * @param time_frame TimeFrame for index→time conversion
 * @param y_scale Scaling factor for Y values
 * @param start_time Start of visible range
 * @param end_time End of visible range
 * @return Range of MappedAnalogVertex
 * 
 * @see mapAnalogSeries For basic mapping without gap detection
 * @see MappedAnalogVertex For the vertex structure with time_index
 */
[[nodiscard]] inline auto mapAnalogSeriesWithIndices(
        AnalogTimeSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & query_time_frame,
        float y_scale,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time) {
    float const y_offset = layout.y_transform.offset;
    auto const * series_tf = series.getTimeFrame().get();

    // Use cross-timeframe query: start_time/end_time are in query_time_frame coordinates
    return series.getTimeValueRangeInTimeFrameIndexRange(start_time, end_time, query_time_frame) | std::views::transform([series_tf, y_scale, y_offset](auto const & tv_point) {
               // Data's TimeFrameIndex is in series timeframe, use series timeframe for X
               float x = series_tf ? static_cast<float>(series_tf->getTimeAtIndex(tv_point.time_frame_index))
                                   : static_cast<float>(tv_point.time_frame_index.getValue());
               float y = tv_point.value() * y_scale + y_offset;
               return MappedAnalogVertex{x, y, tv_point.time_frame_index.getValue()};
           });
}

// ============================================================================
// Materialized Versions (for cases needing random access or multiple passes)
// ============================================================================

/**
 * @brief Map events to a vector of MappedElement
 * 
 * Materializes the event mapping for cases requiring multiple passes
 * or random access.
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning
 * @param time_frame TimeFrame for index→time conversion
 * @return Vector of MappedElement
 */
[[nodiscard]] std::vector<MappedElement> mapEventsToVector(
        DigitalEventSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame);

/**
 * @brief Map intervals to a vector of MappedRectElement
 * 
 * @param series Interval series to map
 * @param layout Layout allocation
 * @param time_frame TimeFrame for conversion
 * @return Vector of MappedRectElement
 */
[[nodiscard]] std::vector<MappedRectElement> mapIntervalsToVector(
        DigitalIntervalSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame);

/**
 * @brief Map analog series to a vector of MappedVertex
 * 
 * @param series Analog series to map
 * @param layout Layout allocation
 * @param time_frame TimeFrame for conversion
 * @param y_scale Y scaling factor
 * @param start_time Start of range
 * @param end_time End of range
 * @return Vector of MappedVertex
 */
[[nodiscard]] std::vector<MappedVertex> mapAnalogToVector(
        AnalogTimeSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame,
        float y_scale,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time);

}// namespace TimeSeriesMapper

}// namespace CorePlotting

#endif// COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP
