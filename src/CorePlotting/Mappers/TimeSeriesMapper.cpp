#include "TimeSeriesMapper.hpp"

namespace CorePlotting {

namespace  TimeSeriesMapper {

// ============================================================================
// Materialized Versions (for cases needing random access or multiple passes)
// ============================================================================

std::vector<MappedElement> mapEventsToVector(
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

std::vector<MappedRectElement> mapIntervalsToVector(
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

std::vector<MappedVertex> mapAnalogToVector(
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