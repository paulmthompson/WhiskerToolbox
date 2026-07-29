#include "digital_interval_boolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams) {
    return apply_boolean_operation(digital_interval_series, booleanParams, [](int) {});
}

namespace {

/// Extract plain Interval values from storage indices
std::vector<TimeFrameInterval> extract_intervals(DigitalIntervalSeries const & series) {
    std::vector<TimeFrameInterval> result;
    result.reserve(series.size());
    for (size_t i = 0; i < series.size(); ++i) {
        result.push_back(series.getStoredInterval(i));
    }
    return result;
}

/// Sort intervals by start time, then merge any overlapping/adjacent ones
std::vector<TimeFrameInterval> sort_and_merge(std::vector<TimeFrameInterval> intervals) {
    if (intervals.empty()) {
        return {};
    }
    std::sort(intervals.begin(), intervals.end(), [](auto const & a, auto const & b) {
        return a.start < b.start;
    });
    std::vector<TimeFrameInterval> merged;
    merged.push_back(intervals[0]);
    for (size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i].start <= merged.back().end + TimeFrameIndex{1}) {
            merged.back().end = std::max(merged.back().end, intervals[i].end);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

/// Compute NOT (gaps) of merged intervals within their bounding range [min, max]
std::vector<TimeFrameInterval> compute_not(std::vector<TimeFrameInterval> const & merged) {
    if (merged.empty()) {
        return {};
    }
    std::vector<TimeFrameInterval> result;
    for (size_t i = 1; i < merged.size(); ++i) {
        if (merged[i].start > merged[i - 1].end + TimeFrameIndex{1}) {
            result.push_back({merged[i - 1].end + TimeFrameIndex{1}, merged[i].start - TimeFrameIndex{1}});
        }
    }
    return result;
}

struct SweepEvent {
    TimeFrameIndex time;
    int8_t input_delta;
    int8_t other_delta;
};

/// Apply a binary boolean operation using sweep-line over interval endpoints.
/// Complexity: O((n+m) log(n+m)) where n,m are interval counts.
std::vector<TimeFrameInterval> sweep_line_boolean(
        std::vector<TimeFrameInterval> const & input_intervals,
        std::vector<TimeFrameInterval> const & other_intervals,
        BooleanParams::BooleanOperation operation) {

    std::vector<SweepEvent> events;
    events.reserve((input_intervals.size() + other_intervals.size()) * 2);

    for (auto const & iv: input_intervals) {
        events.push_back({iv.start, + 1, 0});
        events.push_back({iv.end + TimeFrameIndex{1}, -1, 0});
    }
    for (auto const & iv: other_intervals) {
        events.push_back({iv.start, 0, +1});
        events.push_back({iv.end + TimeFrameIndex{1}, 0, -1});
    }

    std::sort(events.begin(), events.end(), [](auto const & a, auto const & b) {
        return a.time < b.time;
    });

    auto evaluate = [operation](bool a, bool b) -> bool {
        switch (operation) {
            case BooleanParams::BooleanOperation::AND:
                return a && b;
            case BooleanParams::BooleanOperation::OR:
                return a || b;
            case BooleanParams::BooleanOperation::XOR:
                return a != b;
            case BooleanParams::BooleanOperation::AND_NOT:
                return a && !b;
            default:
                return false;
        }
    };

    std::vector<TimeFrameInterval> result;
    int input_count = 0;
    int other_count = 0;
    bool prev_result = false;
    TimeFrameIndex interval_start = TimeFrameIndex{0};

    size_t i = 0;
    while (i < events.size()) {
        TimeFrameIndex const t = events[i].time;

        // Process all events at the same timestamp
        while (i < events.size() && events[i].time == t) {
            input_count += events[i].input_delta;
            other_count += events[i].other_delta;
            ++i;
        }

        bool const current_result = evaluate(input_count > 0, other_count > 0);

        if (current_result && !prev_result) {
            interval_start = t;
        } else if (!current_result && prev_result) {
            result.push_back({interval_start, t - TimeFrameIndex{1}});
        }

        prev_result = current_result;
    }

    return result;
}

}// anonymous namespace

std::shared_ptr<DigitalIntervalSeries> apply_boolean_operation(
        DigitalIntervalSeries const * digital_interval_series,
        BooleanParams const & booleanParams,
        const ProgressCallback& progressCallback) {

    if (!digital_interval_series) {
        std::cerr << "apply_boolean_operation: Input DigitalIntervalSeries is null" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto input_timeframe = digital_interval_series->getTimeFrame();

    // Binary operations require the other series
    if (booleanParams.operation != BooleanParams::BooleanOperation::NOT && !booleanParams.other_series) {
        std::cerr << "apply_boolean_operation: other_series is null for operation requiring two inputs" << std::endl;
        auto result = std::make_shared<DigitalIntervalSeries>();
        result->setTimeFrame(input_timeframe);
        return result;
    }

    auto input_ivs = sort_and_merge(extract_intervals(*digital_interval_series));

    if (progressCallback) {
        progressCallback(10);
    }

    // NOT: compute gaps in the sorted/merged input intervals
    if (booleanParams.operation == BooleanParams::BooleanOperation::NOT) {
        auto result_intervals = compute_not(input_ivs);

        if (progressCallback) {
            progressCallback(100);
        }

        auto result = std::make_shared<DigitalIntervalSeries>(std::move(result_intervals));
        result->setTimeFrame(input_timeframe);
        return result;
    }

    // Binary operations require the other series
    auto other_timeframe = booleanParams.other_series->getTimeFrame();

    if (progressCallback) {
        progressCallback(20);
    }

    // Convert other_intervals to input timeframe if they differ
    std::vector<TimeFrameInterval> converted_other_intervals;
    if (input_timeframe && other_timeframe && input_timeframe != other_timeframe) {
        for (size_t i = 0; i < booleanParams.other_series->size(); ++i) {
            auto const stored = booleanParams.other_series->getStoredInterval(i);
            auto const start_time = other_timeframe->getTimeAtIndex(stored.start);
            auto const end_time = other_timeframe->getTimeAtIndex(stored.end);

            auto converted_start = input_timeframe->getIndexAtTime(start_time, false);
            auto converted_end = input_timeframe->getIndexAtTime(end_time, true);

            converted_other_intervals.push_back({converted_start, converted_end});
        }
    } else {
        converted_other_intervals = extract_intervals(*booleanParams.other_series);
    }

    auto other_ivs = sort_and_merge(std::move(converted_other_intervals));

    if (progressCallback) {
        progressCallback(40);
    }

    // Sweep-line boolean — O((n+m) log(n+m))
    auto result_intervals = sweep_line_boolean(input_ivs, other_ivs, booleanParams.operation);

    if (progressCallback) {
        progressCallback(100);
    }

    auto result = std::make_shared<DigitalIntervalSeries>(std::move(result_intervals));
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
