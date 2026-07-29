/**
 * @file IntervalToEvent.cpp
 * @brief Extract DigitalEventSeries events from DigitalIntervalSeries alignment points
 */

#include "IntervalToEvent.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "core/ComputeContext.hpp"

#include <cstdint>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Compute the event time for an interval at the selected alignment point
 *
 * @param interval Input interval in TimeFrameIndex space
 * @param point Which point within the interval to use
 * @return Event time as int64_t TimeFrameIndex value
 */
TimeFrameIndex eventTimeForInterval(TimeFrameInterval const & interval, IntervalEventPoint point) {
    switch (point) {
        case IntervalEventPoint::start:
            return interval.start;
        case IntervalEventPoint::end:
            return interval.end;
        case IntervalEventPoint::center:
            return TimeFrameIndex{(interval.start.getValue() + interval.end.getValue()) / 2};
    }
    return interval.start;
}

}// namespace Neuralyzer::Transforms::V2::Examples

namespace {

/**
 * @brief Create an empty DigitalEventSeries preserving the input TimeFrame
 *
 * @param time_frame TimeFrame shared pointer from the input series
 * @return Empty event series with TimeFrame set
 */
std::shared_ptr<DigitalEventSeries> makeEmptyResult(std::shared_ptr<TimeFrame> const & time_frame) {
    auto result = std::make_shared<DigitalEventSeries>();
    result->setTimeFrame(time_frame);
    return result;
}

}// namespace

namespace Neuralyzer::Transforms::V2::Examples {

std::shared_ptr<DigitalEventSeries> intervalToEvent(
        DigitalIntervalSeries const & input,
        IntervalToEventParams const & params,
        ComputeContext const & ctx) {
    auto const time_frame = input.getTimeFrame();

    if (input.size() == 0) {
        ctx.reportProgress(100);
        return makeEmptyResult(time_frame);
    }

    std::vector<TimeFrameIndex> events;
    events.reserve(input.size());

    std::size_t const total_intervals = input.size();
    std::size_t processed = 0;

    ctx.reportProgress(0);

    for (auto const & interval_with_id: input.view()) {
        if (processed % 100 == 0 && ctx.shouldCancel()) {
            return makeEmptyResult(time_frame);
        }

        auto const & interval = interval_with_id.value();
        events.emplace_back(eventTimeForInterval(interval, params.point));

        ++processed;
        if (processed % 100 == 0 || processed == total_intervals) {
            int const progress = static_cast<int>(
                    (static_cast<double>(processed) / static_cast<double>(total_intervals)) * 100.0);
            ctx.reportProgress(progress);
        }
    }

    ctx.reportProgress(100);
    auto result = std::make_shared<DigitalEventSeries>(std::move(events));
    result->setTimeFrame(time_frame);
    return result;
}

}// namespace Neuralyzer::Transforms::V2::Examples
