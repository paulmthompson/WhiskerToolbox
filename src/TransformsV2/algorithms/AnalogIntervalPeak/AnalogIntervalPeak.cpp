#include "AnalogIntervalPeak.hpp"

#include "core/ComputeContext.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

std::shared_ptr<DigitalEventSeries> analogIntervalPeak(
        DigitalIntervalSeries const & intervals,
        AnalogTimeSeries const & analog,
        AnalogIntervalPeakParams const & params,
        ComputeContext const & ctx) {

    // Report initial progress
    if (ctx.progress) {
        ctx.progress(5);
    }

    // Check for cancellation
    if (ctx.is_cancelled && ctx.is_cancelled()) {
        return std::make_shared<DigitalEventSeries>();
    }

    // Get interval data
    auto const & interval_data = intervals.view();
    if (interval_data.empty()) {
        if (ctx.progress) ctx.progress(100);
        return std::make_shared<DigitalEventSeries>();
    }

    // Get the interval series timeframe (may be nullptr if not set)
    auto interval_timeframe = intervals.getTimeFrame();

    // Build search ranges based on search mode
    std::vector<std::pair<TimeFrameIndex, TimeFrameIndex>> search_ranges;

    if (params.search_mode == AnalogIntervalPeakParams::SearchMode::within_intervals) {
        // Search within each interval: [start, end]
        assert(interval_timeframe != nullptr && "analogIntervalPeak requires interval time frame");
        for (auto const & interval: interval_data) {
            auto const tf_interval = toTimeFrameInterval(interval.value(), *interval_timeframe);
            search_ranges.emplace_back(tf_interval.start, tf_interval.end);
        }
    } else {
        // Search between interval starts: [start_i, start_{i+1})
        assert(interval_timeframe != nullptr && "analogIntervalPeak requires interval time frame");
        for (size_t i = 0; i < intervals.size() - 1; ++i) {
            auto const start_i = toTimeFrameInterval(interval_data[i].value(), *interval_timeframe).start;
            auto const start_next = toTimeFrameInterval(interval_data[i + 1].value(), *interval_timeframe).start;
            search_ranges.emplace_back(start_i, start_next - TimeFrameIndex{1});
        }
        // For the last interval, search from its start to its end
        if (!interval_data.empty()) {
            auto const last_interval = toTimeFrameInterval(interval_data.back().value(), *interval_timeframe);
            search_ranges.emplace_back(last_interval.start, last_interval.end);
        }
    }

    if (ctx.progress) {
        ctx.progress(10);
    }

    // Check if analog data exists
    auto const & values = analog.getAnalogTimeSeries();
    if (values.empty()) {
        if (ctx.progress) ctx.progress(100);
        return std::make_shared<DigitalEventSeries>();
    }

    if (ctx.progress) {
        ctx.progress(15);
    }

    // Find peaks in each search range
    std::vector<TimeFrameIndex> peak_events;
    size_t const total_ranges = search_ranges.size();

    for (size_t range_idx = 0; range_idx < total_ranges; ++range_idx) {
        // Check for cancellation
        if (ctx.is_cancelled && ctx.is_cancelled()) {
            break;
        }

        auto const & [range_start, range_end] = search_ranges[range_idx];

        TimeFrameIndex const start_index(range_start);
        TimeFrameIndex const end_index(range_end);

        // Get data and corresponding time indices in this range
        // If interval_timeframe is set, pass it for automatic conversion
        auto time_value_pair = interval_timeframe
                                       ? analog.getTimeValueSpanInTimeFrameIndexRange(start_index, end_index, interval_timeframe.get())
                                       : analog.getTimeValueSpanInTimeFrameIndexRange(start_index, end_index);

        auto const & data_span = time_value_pair.values;
        if (data_span.empty()) {
            // No data in this range, skip it
            continue;
        }

        // Find the peak value and its index within the span
        size_t peak_idx_in_span = 0;

        if (params.peak_type == AnalogIntervalPeakParams::PeakType::maximum) {
            auto max_it = std::ranges::max_element(data_span);
            peak_idx_in_span = static_cast<size_t>(std::distance(data_span.begin(), max_it));
        } else {
            auto min_it = std::ranges::min_element(data_span);
            peak_idx_in_span = static_cast<size_t>(std::distance(data_span.begin(), min_it));
        }

        // Get the actual TimeFrameIndex for the peak from the time_indices iterator
        auto time_iter = time_value_pair.time_indices.begin();
        for (size_t i = 0; i < peak_idx_in_span; ++i) {
            ++(*time_iter);
        }
        TimeFrameIndex const peak_time_index = **time_iter;

        // Peak indices are stored in the analog series coordinate system.
        peak_events.push_back(peak_time_index);

        // Report progress
        if (ctx.progress && total_ranges > 0) {
            int const progress = 15 + static_cast<int>(((range_idx + 1) * 80.0) / static_cast<double>(total_ranges));
            ctx.progress(progress);
        }
    }

    // Attach a TimeFrame so view() can resolve stored indices to ClockTicks.
    //
    // Peak events are stored as analog TimeFrameIndex values. ElementRegistry
    // binary propagation would copy input1's (interval) TimeFrame, which is wrong
    // when the analog and interval series use different time bases. Prefer the
    // analog TimeFrame; fall back to the interval TimeFrame only when analog has none.
    // DataManager::setData still owns the long-term TimeFrame assignment in production.
    auto event_series = std::make_shared<DigitalEventSeries>(peak_events);
    if (auto analog_timeframe = analog.getTimeFrame()) {
        event_series->setTimeFrame(analog_timeframe);
    } else if (interval_timeframe) {
        event_series->setTimeFrame(interval_timeframe);
    }

    if (ctx.progress) {
        ctx.progress(100);
    }

    return event_series;
}

}// namespace Neuralyzer::Transforms::V2
