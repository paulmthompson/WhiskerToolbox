/**
 * @file EventToInterval.cpp
 * @brief Expand DigitalEventSeries events into overlapping window intervals
 */

#include "EventToInterval.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "core/ComputeContext.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

std::shared_ptr<DigitalIntervalSeries> eventToInterval(
        DigitalEventSeries const & input,
        EventToIntervalParams const & params,
        ComputeContext const & ctx) {
    assert(params.pre_expansion.getValue() >= 0 && "eventToInterval: pre_expansion must be non-negative");
    assert(params.post_expansion.getValue() >= 0 && "eventToInterval: post_expansion must be non-negative");

    auto const time_frame = input.getTimeFrame();

    if (input.size() == 0) {
        ctx.reportProgress(100);
        return DigitalIntervalSeries::createOverlapping({}, time_frame);
    }

    int64_t const pre = params.pre_expansion.getValue();
    int64_t const post = params.post_expansion.getValue();

    std::vector<Interval> intervals;
    intervals.reserve(input.size());

    std::size_t const total_events = input.size();
    std::size_t processed = 0;

    ctx.reportProgress(0);

    for (auto const & event: input.view()) {
        if (processed % 100 == 0 && ctx.shouldCancel()) {
            return DigitalIntervalSeries::createOverlapping({}, time_frame);
        }

        int64_t const event_index = event.time().getValue();
        intervals.push_back(Interval{
                event_index - pre,
                event_index + post});

        ++processed;
        if (processed % 100 == 0 || processed == total_events) {
            int const progress = static_cast<int>(
                    (static_cast<double>(processed) / static_cast<double>(total_events)) * 100.0);
            ctx.reportProgress(progress);
        }
    }

    ctx.reportProgress(100);
    return DigitalIntervalSeries::createOverlapping(std::move(intervals), time_frame);
}

}// namespace Neuralyzer::Transforms::V2::Examples
