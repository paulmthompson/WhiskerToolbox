#ifndef INTERVAL_EDGE_EXTRACTION_HPP
#define INTERVAL_EDGE_EXTRACTION_HPP

/**
 * @file IntervalEdgeExtraction.hpp
 * @brief Utility for extracting edge events from DigitalIntervalSeries
 *
 * Converts a DigitalIntervalSeries into a DigitalEventSeries by extracting
 * either the start or end times of each interval. The resulting event series
 * inherits the TimeFrame from the source interval series.
 *
 * This is used by EventPlotWidget to support plotting interval edges as
 * raster events without modifying the core rendering pipeline.
 */

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"

#include <memory>

namespace WhiskerToolbox::Plots {

/**
 * @brief Extract edge events from a DigitalIntervalSeries
 *
 * Creates a new DigitalEventSeries containing either the start or end times
 * of each interval, depending on the specified edge type. The TimeFrame from
 * the source interval series is propagated to the result.
 *
 * @param intervals Source interval series to extract edges from
 * @param edge Which edge to extract (Beginning = start times, End = end times)
 * @return New DigitalEventSeries with one event per interval edge, or nullptr on error
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> extractEdgeEvents(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        IntervalAlignmentType edge) {

    if (!intervals) {
        return nullptr;
    }

    auto result = std::make_shared<DigitalEventSeries>();

    // Propagate TimeFrame from the source interval series
    auto time_frame = intervals->getTimeFrame();
    if (time_frame) {
        result->setTimeFrame(time_frame);
    }

    // Extract the appropriate edge from each interval
    for (auto const & item : intervals->view()) {
        if (edge == IntervalAlignmentType::Beginning) {
            result->addEvent(TimeFrameIndex(item.interval.start));
        } else {
            result->addEvent(TimeFrameIndex(item.interval.end));
        }
    }

    return result;
}

} // namespace WhiskerToolbox::Plots

#endif // INTERVAL_EDGE_EXTRACTION_HPP
