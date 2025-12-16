#ifndef COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP
#define COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "MapperConcepts.hpp"
#include "Layout/SeriesLayout.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

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
 * This mapper is used for:
 * - DataViewer: stacked time-series plots
 * - Event traces: discrete events along time axis
 * - Interval display: temporal regions/epochs
 * 
 * All methods return ranges for zero-copy single-traversal consumption by SceneBuilder.
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
 * - Y = layout.result.allocated_y_center (constant for all events)
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning
 * @param time_frame TimeFrame for index→time conversion
 * @return Range of MappedElement
 */
[[nodiscard]] inline auto mapEvents(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame
) {
    float const y_center = layout.result.allocated_y_center;
    
    return series.view() 
        | std::views::transform([&time_frame, y_center](auto const & event_with_id) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(event_with_id.event_time));
            return MappedElement{x, y_center, event_with_id.entity_id};
        });
}

/**
 * @brief Map events in a time range to world-space positions
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning  
 * @param time_frame TimeFrame for index→time conversion
 * @param start_time Start of visible range (TimeFrameIndex)
 * @param end_time End of visible range (TimeFrameIndex)
 * @return Range of MappedElement for events in range
 */
[[nodiscard]] inline auto mapEventsInRange(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    TimeFrameIndex start_time,
    TimeFrameIndex end_time
) {
    float const y_center = layout.result.allocated_y_center;
    
    return series.getEventsInRange(start_time, end_time)
        | std::views::transform([&time_frame, y_center, &series](TimeFrameIndex event_time) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(event_time));
            // Note: This path doesn't have EntityId readily available
            // For full EntityId support, use view() with filter
            return MappedElement{x, y_center, EntityId{0}};
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
 * - Y = layout.result.allocated_y_center - height/2
 * - Height = layout.result.allocated_height
 * 
 * @param series Interval series to map
 * @param layout Layout allocation for Y positioning and height
 * @param time_frame TimeFrame for index→time conversion
 * @return Range of MappedRectElement
 */
[[nodiscard]] inline auto mapIntervals(
    DigitalIntervalSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame
) {
    float const y_center = layout.result.allocated_y_center;
    float const height = layout.result.allocated_height;
    float const y_bottom = y_center - height / 2.0f;
    
    return series.view()
        | std::views::transform([&time_frame, y_bottom, height](auto const & interval_with_id) {
            float x_start = static_cast<float>(
                time_frame.getTimeAtIndex(TimeFrameIndex{interval_with_id.interval.start}));
            float x_end = static_cast<float>(
                time_frame.getTimeAtIndex(TimeFrameIndex{interval_with_id.interval.end}));
            float width = x_end - x_start;
            return MappedRectElement{x_start, y_bottom, width, height, interval_with_id.entity_id};
        });
}

// ============================================================================
// Analog Mapping: AnalogTimeSeries → MappedVertex range (single polyline)
// ============================================================================

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
 */
[[nodiscard]] inline auto mapAnalogSeries(
    AnalogTimeSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    float y_scale,
    TimeFrameIndex start_time,
    TimeFrameIndex end_time
) {
    float const y_offset = layout.result.allocated_y_center;
    
    return series.getTimeValueRangeInTimeFrameIndexRange(start_time, end_time)
        | std::views::transform([&time_frame, y_scale, y_offset](auto const & tv_point) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(tv_point.time_frame_index));
            float y = tv_point.value * y_scale + y_offset;
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
 */
[[nodiscard]] inline auto mapAnalogSeriesFull(
    AnalogTimeSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    float y_scale
) {
    float const y_offset = layout.result.allocated_y_center;
    
    return series.view()
        | std::views::transform([&time_frame, y_scale, y_offset](auto const & tv_point) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(tv_point.time_frame_index));
            float y = tv_point.value * y_scale + y_offset;
            return MappedVertex{x, y};
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
[[nodiscard]] inline std::vector<MappedElement> mapEventsToVector(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame
) {
    std::vector<MappedElement> result;
    result.reserve(series.size());
    
    for (auto const & event : mapEvents(series, layout, time_frame)) {
        result.push_back(event);
    }
    
    return result;
}

/**
 * @brief Map intervals to a vector of MappedRectElement
 * 
 * @param series Interval series to map
 * @param layout Layout allocation
 * @param time_frame TimeFrame for conversion
 * @return Vector of MappedRectElement
 */
[[nodiscard]] inline std::vector<MappedRectElement> mapIntervalsToVector(
    DigitalIntervalSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame
) {
    std::vector<MappedRectElement> result;
    result.reserve(series.size());
    
    for (auto const & rect : mapIntervals(series, layout, time_frame)) {
        result.push_back(rect);
    }
    
    return result;
}

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
[[nodiscard]] inline std::vector<MappedVertex> mapAnalogToVector(
    AnalogTimeSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    float y_scale,
    TimeFrameIndex start_time,
    TimeFrameIndex end_time
) {
    std::vector<MappedVertex> result;
    // Estimate capacity based on range
    auto range = mapAnalogSeries(series, layout, time_frame, y_scale, start_time, end_time);
    
    for (auto const & vertex : range) {
        result.push_back(vertex);
    }
    
    return result;
}

} // namespace TimeSeriesMapper

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_TIMESERIESMAPPER_HPP
