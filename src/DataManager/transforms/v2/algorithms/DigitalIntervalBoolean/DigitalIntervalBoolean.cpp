#include "DigitalIntervalBoolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

std::shared_ptr<DigitalIntervalSeries> digitalIntervalBoolean(
        DigitalIntervalSeries const & input_series,
        DigitalIntervalSeries const & other_series,
        DigitalIntervalBooleanParams const & params,
        ComputeContext const & ctx) {

    // Validate parameters
    if (!params.isValidOperation()) {
        std::cerr << "digitalIntervalBoolean: Invalid operation: " << params.getOperation() << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & intervals = input_series.getDigitalIntervalSeries();
    auto input_timeframe = input_series.getTimeFrame();
    std::string const operation = params.getOperation();

    // Report initial progress
    ctx.reportProgress(0);

    // For NOT operation, we only use the first series
    if (operation == "not") {
        if (intervals.empty()) {
            // NOT of empty is still empty (no defined range)
            auto result = std::make_shared<DigitalIntervalSeries>();
            result->setTimeFrame(input_timeframe);
            ctx.reportProgress(100);
            return result;
        }

        // Find the overall range
        int64_t min_time = intervals[0].start;
        int64_t max_time = intervals[0].end;
        for (auto const & interval : intervals) {
            min_time = std::min(min_time, interval.start);
            max_time = std::max(max_time, interval.end);
        }

        ctx.reportProgress(20);

        // Create a boolean map for the range
        std::map<int64_t, bool> time_map;
        for (int64_t t = min_time; t <= max_time; ++t) {
            time_map[t] = false;
        }

        // Mark all intervals as true
        for (auto const & interval : intervals) {
            for (int64_t t = interval.start; t <= interval.end; ++t) {
                time_map[t] = true;
            }
        }

        ctx.reportProgress(60);

        // Invert the boolean values
        for (auto & [time, value] : time_map) {
            value = !value;
        }

        // Convert back to intervals
        std::vector<Interval> result_intervals;
        bool in_interval = false;
        int64_t interval_start = 0;

        for (auto const & [time, value] : time_map) {
            if (value && !in_interval) {
                interval_start = time;
                in_interval = true;
            } else if (!value && in_interval) {
                result_intervals.push_back({interval_start, time - 1});
                in_interval = false;
            }
        }

        // Close the last interval if needed
        if (in_interval) {
            result_intervals.push_back({interval_start, max_time});
        }

        ctx.reportProgress(100);

        auto result = std::make_shared<DigitalIntervalSeries>(result_intervals);
        result->setTimeFrame(input_timeframe);
        return result;
    }

    // For all other operations, we need both series
    auto const & other_intervals = other_series.getDigitalIntervalSeries();
    auto other_timeframe = other_series.getTimeFrame();

    if (intervals.empty() && other_intervals.empty()) {
        auto result = std::make_shared<DigitalIntervalSeries>();
        result->setTimeFrame(input_timeframe);
        ctx.reportProgress(100);
        return result;
    }

    ctx.reportProgress(10);

    // Convert other_intervals to input timeframe if they differ
    std::vector<Interval> converted_other_intervals;
    if (input_timeframe && other_timeframe && input_timeframe != other_timeframe) {
        // Need to convert other intervals to input timeframe
        for (auto const & interval : other_intervals) {
            // Convert start and end times from other timeframe to input timeframe
            auto start_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.start});
            auto end_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.end});
            
            auto converted_start = input_timeframe->getIndexAtTime(static_cast<float>(start_time), false);
            auto converted_end = input_timeframe->getIndexAtTime(static_cast<float>(end_time), true);
            
            converted_other_intervals.push_back({converted_start.getValue(), converted_end.getValue()});
        }
    } else {
        // Same timeframe or no timeframe, use intervals directly
        converted_other_intervals = other_intervals;
    }

    ctx.reportProgress(15);

    // Find the combined range (using input timeframe indices)
    int64_t min_time = std::numeric_limits<int64_t>::max();
    int64_t max_time = std::numeric_limits<int64_t>::min();

    for (auto const & interval : intervals) {
        min_time = std::min(min_time, interval.start);
        max_time = std::max(max_time, interval.end);
    }

    for (auto const & interval : converted_other_intervals) {
        min_time = std::min(min_time, interval.start);
        max_time = std::max(max_time, interval.end);
    }

    if (min_time > max_time) {
        // No valid range
        auto result = std::make_shared<DigitalIntervalSeries>();
        result->setTimeFrame(input_timeframe);
        ctx.reportProgress(100);
        return result;
    }

    ctx.reportProgress(20);

    // Create boolean time series for both inputs
    std::map<int64_t, bool> input_map;
    std::map<int64_t, bool> other_map;

    // Initialize all timestamps to false
    for (int64_t t = min_time; t <= max_time; ++t) {
        input_map[t] = false;
        other_map[t] = false;
    }

    ctx.reportProgress(30);

    // Mark intervals as true in input_map
    for (auto const & interval : intervals) {
        for (int64_t t = interval.start; t <= interval.end; ++t) {
            input_map[t] = true;
        }
    }

    ctx.reportProgress(50);

    // Mark converted intervals as true in other_map
    for (auto const & interval : converted_other_intervals) {
        for (int64_t t = interval.start; t <= interval.end; ++t) {
            other_map[t] = true;
        }
    }

    ctx.reportProgress(70);

    // Apply the boolean operation
    std::map<int64_t, bool> result_map;
    
    for (int64_t t = min_time; t <= max_time; ++t) {
        bool input_value = input_map[t];
        bool other_value = other_map[t];
        bool result_value = false;

        if (operation == "and") {
            result_value = input_value && other_value;
        } else if (operation == "or") {
            result_value = input_value || other_value;
        } else if (operation == "xor") {
            result_value = input_value != other_value;
        } else if (operation == "and_not") {
            result_value = input_value && !other_value;
        }

        result_map[t] = result_value;
    }

    ctx.reportProgress(85);

    // Convert result boolean map back to intervals
    std::vector<Interval> result_intervals;
    bool in_interval = false;
    int64_t interval_start = 0;

    for (auto const & [time, value] : result_map) {
        if (value && !in_interval) {
            interval_start = time;
            in_interval = true;
        } else if (!value && in_interval) {
            result_intervals.push_back({interval_start, time - 1});
            in_interval = false;
        }
    }

    // Close the last interval if needed
    if (in_interval) {
        result_intervals.push_back({interval_start, max_time});
    }

    ctx.reportProgress(100);

    auto result = std::make_shared<DigitalIntervalSeries>(result_intervals);
    result->setTimeFrame(input_timeframe);
    return result;
}

}// namespace WhiskerToolbox::Transforms::V2
