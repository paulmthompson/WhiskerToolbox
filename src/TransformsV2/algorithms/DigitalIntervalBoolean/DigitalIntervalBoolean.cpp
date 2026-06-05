#include "DigitalIntervalBoolean.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "core/ComputeContext.hpp"

#include <rfl/Result.hpp>
#include <rfl/internal/enums/StringConverter.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using WhiskerToolbox::Transforms::V2::DigitalIntervalBooleanParams;

/// Extract plain Interval values from a DigitalIntervalSeries view
std::vector<Interval> extract_intervals(auto const & view_data) {
    std::vector<Interval> result;
    for (auto const & item: view_data) {
        result.push_back(item.value());
    }
    return result;
}

/// Sort intervals by start time, then merge any overlapping/adjacent ones
std::vector<Interval> sort_and_merge(std::vector<Interval> intervals) {
    if (intervals.empty()) {
        return {};
    }
    std::sort(intervals.begin(), intervals.end(), [](auto const & a, auto const & b) {
        return a.start < b.start;
    });
    std::vector<Interval> merged;
    merged.push_back(intervals[0]);
    for (size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i].start <= merged.back().end + 1) {
            merged.back().end = std::max(merged.back().end, intervals[i].end);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

/// Compute NOT (gaps) of merged intervals within their bounding range
std::vector<Interval> compute_not(std::vector<Interval> const & merged) {
    if (merged.empty()) {
        return {};
    }
    std::vector<Interval> result;
    for (size_t i = 1; i < merged.size(); ++i) {
        if (merged[i].start > merged[i - 1].end + 1) {
            result.push_back({merged[i - 1].end + 1, merged[i].start - 1});
        }
    }
    return result;
}

struct SweepEvent {
    int64_t time;
    int8_t input_delta;
    int8_t other_delta;
};

/// Apply a binary boolean operation using sweep-line over interval endpoints.
/// Complexity: O((n+m) log(n+m)) where n,m are interval counts.
std::vector<Interval> sweep_line_boolean(
        std::vector<Interval> const & input_intervals,
        std::vector<Interval> const & other_intervals,
        DigitalIntervalBooleanParams::Operation operation) {

    std::vector<SweepEvent> events;
    events.reserve((input_intervals.size() + other_intervals.size()) * 2);

    for (auto const & iv: input_intervals) {
        events.push_back({iv.start, +1, 0});
        events.push_back({iv.end + 1, -1, 0});
    }
    for (auto const & iv: other_intervals) {
        events.push_back({iv.start, 0, +1});
        events.push_back({iv.end + 1, 0, -1});
    }

    std::sort(events.begin(), events.end(), [](auto const & a, auto const & b) {
        return a.time < b.time;
    });

    auto evaluate = [operation](bool a, bool b) -> bool {
        switch (operation) {
            case DigitalIntervalBooleanParams::Operation::And:
                return a && b;
            case DigitalIntervalBooleanParams::Operation::Or:
                return a || b;
            case DigitalIntervalBooleanParams::Operation::Xor:
                return a != b;
            case DigitalIntervalBooleanParams::Operation::and_not:
                return a && !b;
            case DigitalIntervalBooleanParams::Operation::Not:
                return false;
        }
        return false;
    };

    std::vector<Interval> result;
    int input_count = 0;
    int other_count = 0;
    bool prev_result = false;
    int64_t interval_start = 0;

    size_t i = 0;
    while (i < events.size()) {
        int64_t const t = events[i].time;

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
            result.push_back({interval_start, t - 1});
        }

        prev_result = current_result;
    }

    return result;
}

}// anonymous namespace

namespace WhiskerToolbox::Transforms::V2 {

std::shared_ptr<DigitalIntervalSeries> digitalIntervalBoolean(
        DigitalIntervalSeries const & input_series,
        DigitalIntervalSeries const & other_series,
        DigitalIntervalBooleanParams const & params,
        ComputeContext const & ctx) {

    auto input_timeframe = input_series.getTimeFrame();
    auto const operation = params.operation;

    ctx.reportProgress(0);

    auto input_ivs = sort_and_merge(extract_intervals(input_series.view()));

    ctx.reportProgress(10);

    // NOT: compute gaps in the sorted/merged input intervals
    if (operation == DigitalIntervalBooleanParams::Operation::Not) {
        auto result_intervals = compute_not(input_ivs);

        ctx.reportProgress(100);

        auto result = std::make_shared<DigitalIntervalSeries>(std::move(result_intervals));
        result->setTimeFrame(input_timeframe);
        return result;
    }

    // Binary operations require the other series
    auto const & other_view = other_series.view();
    auto other_timeframe = other_series.getTimeFrame();

    ctx.reportProgress(20);

    // Convert other_intervals to input timeframe if they differ
    std::vector<Interval> converted_other_intervals;
    if (input_timeframe && other_timeframe && input_timeframe != other_timeframe) {
        for (auto const & interval: other_view) {
            auto start_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.value().start});
            auto end_time = other_timeframe->getTimeAtIndex(TimeFrameIndex{interval.value().end});

            auto converted_start = input_timeframe->getIndexAtTime(static_cast<float>(start_time), false);
            auto converted_end = input_timeframe->getIndexAtTime(static_cast<float>(end_time), true);

            converted_other_intervals.push_back({converted_start.getValue(), converted_end.getValue()});
        }
    } else {
        converted_other_intervals = extract_intervals(other_view);
    }

    auto other_ivs = sort_and_merge(std::move(converted_other_intervals));

    ctx.reportProgress(40);

    // Sweep-line boolean — O((n+m) log(n+m))
    auto result_intervals = sweep_line_boolean(input_ivs, other_ivs, operation);

    ctx.reportProgress(100);

    auto result = std::make_shared<DigitalIntervalSeries>(std::move(result_intervals));
    result->setTimeFrame(input_timeframe);
    return result;
}

}// namespace WhiskerToolbox::Transforms::V2

namespace rfl::internal::enums {

template <>
class StringConverter<WhiskerToolbox::Transforms::V2::DigitalIntervalBooleanParams::Operation> {
public:
    using Operation = WhiskerToolbox::Transforms::V2::DigitalIntervalBooleanParams::Operation;

    static std::string enum_to_string(Operation const op) {
        switch (op) {
            case Operation::And:
                return "and";
            case Operation::Or:
                return "or";
            case Operation::Xor:
                return "xor";
            case Operation::Not:
                return "not";
            case Operation::and_not:
                return "and_not";
        }
        return {};
    }

    static rfl::Result<Operation> string_to_enum(std::string const & str) {
        if (str == "and") {
            return Operation::And;
        }
        if (str == "or") {
            return Operation::Or;
        }
        if (str == "xor") {
            return Operation::Xor;
        }
        if (str == "not") {
            return Operation::Not;
        }
        if (str == "and_not") {
            return Operation::and_not;
        }
        return rfl::Error("Invalid operation: " + str);
    }
};

}// namespace rfl::internal::enums
