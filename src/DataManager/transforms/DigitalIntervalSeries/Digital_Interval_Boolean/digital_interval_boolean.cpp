#include "digital_interval_boolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams) {
    return apply_boolean_operation(digital_interval_series, booleanParams, [](int) {});
}

std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!digital_interval_series) {
        std::cerr << "apply_boolean_operation: Input DigitalIntervalSeries is null" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & intervals = digital_interval_series->view();
    auto input_timeframe = digital_interval_series->getTimeFrame();

    // For NOT operation, we don't need the other series
    if (booleanParams.operation == BooleanParams::BooleanOperation::NOT) {
        if (intervals.empty()) {
            // NOT of empty is still empty (no defined range)
            auto result = std::make_shared<DigitalIntervalSeries>();
            result->setTimeFrame(input_timeframe);
            return result;
        }

        // Find the overall range
        int64_t min_time = intervals[0].value().start;
        int64_t max_time = intervals[0].value().end;
        for (auto const & interval : intervals) {
            min_time = std::min(min_time, interval.value().start);
            max_time = std::max(max_time, interval.value().end);
        }

        if (progressCallback) {
            progressCallback(20);
        }

        // Create a boolean map for the range
        std::map<int64_t, bool> time_map;
        for (int64_t t = min_time; t <= max_time; ++t) {
            time_map[t] = false;
        }

        // Mark all intervals as true
        for (auto const & interval : intervals) {
            for (int64_t t = interval.value().start; t <= interval.value().end; ++t) {
                time_map[t] = true;
            }
        }

        if (progressCallback) {
            progressCallback(60);
        }

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

        if (progressCallback) {
            progressCallback(100);
        }

        auto result = std::make_shared<DigitalIntervalSeries>(result_intervals);
        result->setTimeFrame(input_timeframe);
        return result;
    }

    // For all other operations, we need the other series
    if (!booleanParams.other_series) {
        std::cerr << "apply_boolean_operation: other_series is null for operation requiring two inputs" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & other_intervals = booleanParams.other_series->view();
    auto other_timeframe = booleanParams.other_series->getTimeFrame();

    if (intervals.empty() && other_intervals.empty()) {
        auto result = std::make_shared<DigitalIntervalSeries>();
        result->setTimeFrame(input_timeframe);
        return result;
    }

    if (progressCallback) {
        progressCallback(10);
    }

    // Convert other_intervals to input timeframe if they differ
    std::vector<Interval> converted_other_intervals;
    if (input_timeframe && other_timeframe && input_timeframe != other_timeframe) {
        // Need to convert other intervals to input timeframe
        for (auto const & interval : other_intervals) {
            // Convert start and end times from other timeframe to input timeframe
            auto start_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.value().start});
            auto end_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.value().end});
            
            auto converted_start = input_timeframe->getIndexAtTime(static_cast<float>(start_time), false);
            auto converted_end = input_timeframe->getIndexAtTime(static_cast<float>(end_time), true);
            
            converted_other_intervals.push_back({converted_start.getValue(), converted_end.getValue()});
        }
    } else {
        // Same timeframe or no timeframe, use intervals directly
        for (auto const & interval : other_intervals) {
            converted_other_intervals.push_back(interval.value());
        }
    }

    if (progressCallback) {
        progressCallback(15);
    }

    // Find the combined range (using input timeframe indices)
    int64_t min_time = std::numeric_limits<int64_t>::max();
    int64_t max_time = std::numeric_limits<int64_t>::min();

    for (auto const & interval : intervals) {
        min_time = std::min(min_time, interval.value().start);
        max_time = std::max(max_time, interval.value().end);
    }

    for (auto const & interval : converted_other_intervals) {
        min_time = std::min(min_time, interval.start);
        max_time = std::max(max_time, interval.end);
    }

    if (min_time > max_time) {
        // No valid range
        auto result = std::make_shared<DigitalIntervalSeries>();
        result->setTimeFrame(input_timeframe);
        return result;
    }

    if (progressCallback) {
        progressCallback(20);
    }

    // Create boolean time series for both inputs
    std::map<int64_t, bool> input_map;
    std::map<int64_t, bool> other_map;

    // Initialize all timestamps to false
    for (int64_t t = min_time; t <= max_time; ++t) {
        input_map[t] = false;
        other_map[t] = false;
    }

    if (progressCallback) {
        progressCallback(30);
    }

    // Mark intervals as true in input_map
    for (auto const & interval : intervals) {
        for (int64_t t = interval.value().start; t <= interval.value().end; ++t) {
            input_map[t] = true;
        }
    }

    if (progressCallback) {
        progressCallback(50);
    }

    // Mark converted intervals as true in other_map
    for (auto const & interval : converted_other_intervals) {
        for (int64_t t = interval.start; t <= interval.end; ++t) {
            other_map[t] = true;
        }
    }

    if (progressCallback) {
        progressCallback(70);
    }

    // Apply the boolean operation
    std::map<int64_t, bool> result_map;
    
    for (int64_t t = min_time; t <= max_time; ++t) {
        bool input_value = input_map[t];
        bool other_value = other_map[t];
        bool result_value = false;

        switch (booleanParams.operation) {
            case BooleanParams::BooleanOperation::AND:
                result_value = input_value && other_value;
                break;
            case BooleanParams::BooleanOperation::OR:
                result_value = input_value || other_value;
                break;
            case BooleanParams::BooleanOperation::XOR:
                result_value = input_value != other_value;
                break;
            case BooleanParams::BooleanOperation::AND_NOT:
                result_value = input_value && !other_value;
                break;
            case BooleanParams::BooleanOperation::NOT:
                // Already handled above
                result_value = false;
                break;
        }

        result_map[t] = result_value;
    }

    if (progressCallback) {
        progressCallback(85);
    }

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

    if (progressCallback) {
        progressCallback(100);
    }

    auto result = std::make_shared<DigitalIntervalSeries>(result_intervals);
    result->setTimeFrame(input_timeframe);
    return result;
}

///////////////////////////////////////////////////////////////////////////////

std::string BooleanOperation::getName() const {
    return "Boolean Operation";
}

std::type_index BooleanOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<DigitalIntervalSeries>);
}

bool BooleanOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<DigitalIntervalSeries>(dataVariant);
}

std::unique_ptr<TransformParametersBase> BooleanOperation::getDefaultParameters() const {
    return std::make_unique<BooleanParams>();
}

DataTypeVariant BooleanOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant BooleanOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "BooleanOperation::execute: Invalid input data variant" << std::endl;
        return {};
    }

    DigitalIntervalSeries const * digital_raw_ptr = (*ptr_ptr).get();

    BooleanParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams =
                dynamic_cast<BooleanParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "BooleanOperation::execute: Incompatible parameter type, using defaults" << std::endl;
        }
    }

    std::shared_ptr<DigitalIntervalSeries> result = apply_boolean_operation(
            digital_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "BooleanOperation::execute: Boolean operation failed" << std::endl;
        return {};
    }

    return result;
}
