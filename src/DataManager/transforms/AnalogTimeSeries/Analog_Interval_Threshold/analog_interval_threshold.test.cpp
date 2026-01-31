#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "transforms/AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.hpp"
#include "transforms/data_transforms.hpp"
#include "DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"

#include "fixtures/scenarios/analog/interval_threshold_scenarios.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

// Helper function to validate that all values during intervals are above threshold
auto validateIntervalsAboveThreshold = [](
                                               auto const & values,
                                               auto const & times,
                                               std::ranges::range auto const & intervals,
                                               IntervalThresholdParams const & params) -> bool {
    for (auto const & interval: intervals) {
        // Find all time indices that fall within this interval
        for (size_t i = 0; i < times.size(); ++i) {
            if (times[i].getValue() >= static_cast<int64_t>(interval.value().start) &&
                times[i].getValue() <= static_cast<int64_t>(interval.value().end)) {

                // Check if this value meets the threshold criteria
                bool meetsThreshold = false;
                switch (params.direction) {
                    case IntervalThresholdParams::ThresholdDirection::POSITIVE:
                        meetsThreshold = values[i] > static_cast<float>(params.thresholdValue);
                        break;
                    case IntervalThresholdParams::ThresholdDirection::NEGATIVE:
                        meetsThreshold = values[i] < static_cast<float>(params.thresholdValue);
                        break;
                    case IntervalThresholdParams::ThresholdDirection::ABSOLUTE:
                        meetsThreshold = std::abs(values[i]) > static_cast<float>(params.thresholdValue);
                        break;
                }

                if (!meetsThreshold) {
                    return false;// Found a value in interval that doesn't meet threshold
                }
            }
        }
    }
    return true;// All values in all intervals meet threshold
};

TEST_CASE("Data Transform: Interval Threshold - Happy Path", "[transforms][analog_interval_threshold]") {
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    IntervalThresholdParams params;
    // Set default to IGNORE mode to preserve original test behavior
    params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;
    int volatile progress_val = -1;
    int volatile call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Positive threshold - simple case") {
        auto ats = analog_scenarios::positive_simple();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_intervals = interval_threshold(ats.get(), params, cb);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Negative threshold") {
        auto ats = analog_scenarios::negative_threshold();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = -1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);

        // Validate that all values during intervals meet negative threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("Absolute threshold") {
        auto ats = analog_scenarios::absolute_threshold();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);

        // Validate that all values during intervals meet absolute threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("With lockout time") {
        auto ats = analog_scenarios::with_lockout();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 100.0;// 100 time units lockout
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 3);// Three separate intervals - lockout prevents starting too soon

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 300);
        REQUIRE(intervals[1].value().end == 300);
        REQUIRE(intervals[2].value().start == 450);
        REQUIRE(intervals[2].value().end == 450);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("With minimum duration") {
        auto ats = analog_scenarios::with_min_duration();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 150.0;// Minimum 150 time units

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);// Only one interval meets minimum duration

        REQUIRE(intervals[0].value().start == 300);
        REQUIRE(intervals[0].value().end == 500);
        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("Signal ends while above threshold") {
        auto ats = analog_scenarios::ends_above_threshold();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 500);// Should extend to end of signal
        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("No intervals detected") {
        auto ats = analog_scenarios::no_intervals();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(intervals.empty());
    }

    SECTION("Progress callback detailed check") {
        auto ats = analog_scenarios::progress_callback();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count++;
            progress_values_seen.push_back(p);
        };

        result_intervals = interval_threshold(ats.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);

        // Check that we see increasing progress values
        REQUIRE(!progress_values_seen.empty());
        REQUIRE(progress_values_seen.front() >= 0);
        REQUIRE(progress_values_seen.back() == 100);

        // Verify progress is monotonically increasing
        for (size_t i = 1; i < progress_values_seen.size(); ++i) {
            REQUIRE(progress_values_seen[i] >= progress_values_seen[i - 1]);
        }
    }

    SECTION("Complex signal with multiple parameters") {
        auto ats = analog_scenarios::complex_signal();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 50.0;
        params.minDuration = 100.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals that meet minimum duration

        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 400);
        REQUIRE(intervals[1].value().end == 500);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }
}

TEST_CASE( "Data Transform: Interval Threshold - Error and Edge Cases", "[transforms][analog_interval_threshold]") {
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    IntervalThresholdParams params;
    // Set default to IGNORE mode to preserve original test behavior
    params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;
    int volatile progress_val = -1;
    int volatile call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Null input AnalogTimeSeries") {
        AnalogTimeSeries* ats = nullptr;
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;

        result_intervals = interval_threshold(ats, params);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_intervals = interval_threshold(ats, params, cb);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());
        // Progress callback should not be called for null input
        REQUIRE(call_count == 0);
    }

    SECTION("Empty time series") {
        auto ats = analog_scenarios::empty_signal();
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());
    }

    SECTION("Single sample above threshold") {
        auto ats = analog_scenarios::single_above();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 100);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("Single sample below threshold") {
        auto ats = analog_scenarios::single_below();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(intervals.empty());
    }

    SECTION("All values above threshold") {
        auto ats = analog_scenarios::all_above();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 500);
    }

    SECTION("Zero threshold") {
        auto ats = analog_scenarios::zero_threshold();

        params.thresholdValue = 0.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
        REQUIRE(intervals[0].value().start == 300);
        REQUIRE(intervals[0].value().end == 300);
        REQUIRE(intervals[1].value().start == 500);
        REQUIRE(intervals[1].value().end == 500);
    }

    SECTION("Negative threshold value") {
        auto ats = analog_scenarios::negative_value();

        params.thresholdValue = -1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 100);
        REQUIRE(intervals[1].value().start == 400);
        REQUIRE(intervals[1].value().end == 400);
    }

    SECTION("Very large lockout time") {
        auto ats = analog_scenarios::large_lockout();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 1000.0;// Very large lockout
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);// Only first interval should be detected
        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);
    }

    SECTION("Very large minimum duration") {
        auto ats = analog_scenarios::large_min_duration();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 1000.0;// Very large minimum duration

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(intervals.empty());// No intervals meet minimum duration
    }

    SECTION("Irregular timestamp spacing") {
        auto ats = analog_scenarios::irregular_spacing();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
        REQUIRE(intervals[0].value().start == 1);
        REQUIRE(intervals[0].value().end == 1);
        REQUIRE(intervals[1].value().start == 101);
        REQUIRE(intervals[1].value().end == 101);
    }
}

TEST_CASE( "Data Transform: Interval Threshold - Single Sample Above Threshold Zero Lockout", "[transforms][analog_interval_threshold]") {
    SECTION("Single sample above threshold followed by below threshold") {
        auto ats = analog_scenarios::single_sample_lockout();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        IntervalThresholdParams params;
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;// Zero lockout period
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

        auto result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);

        // The interval should start and end at time 200 (the single sample above threshold)
        // This ensures all signals during the detected interval are above threshold
        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("Multiple single samples above threshold") {
        auto ats = analog_scenarios::multiple_single_samples();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        IntervalThresholdParams params;
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;// Zero lockout period
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

        auto result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 3);// Three isolated single samples above threshold

        // Each interval should be a single point in time
        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 400);
        REQUIRE(intervals[1].value().end == 400);
        REQUIRE(intervals[2].value().start == 600);
        REQUIRE(intervals[2].value().end == 600);
        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }
}

TEST_CASE( "Data Transform: Interval Threshold - Operation Class Tests", "[transforms][analog_interval_threshold][operation]") {
    IntervalThresholdOperation operation;
    DataTypeVariant variant;
    IntervalThresholdParams params;
    params.thresholdValue = 1.0;
    params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
    params.lockoutTime = 0.0;
    params.minDuration = 0.0;
    params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

    SECTION("Operation metadata") {
        REQUIRE(operation.getName() == "Threshold Interval Detection");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("canApply with valid data") {
        auto ats = analog_scenarios::operation_interface();
        variant = ats;

        REQUIRE(operation.canApply(variant));
    }

    SECTION("canApply with null shared_ptr") {
        std::shared_ptr<AnalogTimeSeries> null_ats = nullptr;
        variant = null_ats;

        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("execute with valid parameters") {
        auto ats = analog_scenarios::operation_interface();
        variant = ats;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
    }

    SECTION("execute with null parameters") {
        auto ats = analog_scenarios::operation_interface();
        variant = ats;

        auto result = operation.execute(variant, nullptr);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
    }

    SECTION("execute with progress callback") {
        auto ats = analog_scenarios::operation_interface();
        variant = ats;

        int volatile progress_val = -1;
        int volatile call_count = 0;
        ProgressCallback cb = [&](int p) {
            progress_val = p;
            call_count++;
        };

        auto result = operation.execute(variant, &params, cb);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("execute with wrong parameter type") {
        auto ats = analog_scenarios::operation_interface();
        variant = ats;

        // Create a different parameter type
        struct WrongParams : public TransformParametersBase {
            int dummy = 42;
        } wrong_params;

        auto result = operation.execute(variant, &wrong_params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        // Should still work with default parameters
        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
    }

    SECTION("execute with different threshold directions") {
        auto ats = analog_scenarios::operation_different_directions();
        variant = ats;

        // Test negative threshold
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.thresholdValue = -1.0;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);

        // Test absolute threshold
        params.direction = IntervalThresholdParams::ThresholdDirection::ABSOLUTE;
        params.thresholdValue = 1.0;

        result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);

        auto const & abs_intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
    }
}

TEST_CASE( "Data Transform: Interval Threshold - Missing Data Handling", "[transforms][analog_interval_threshold]") {
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    IntervalThresholdParams params;

    SECTION("Missing data treated as zero - positive threshold") {
        auto ats = analog_scenarios::missing_data_positive();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        // Should get: [101,102] (ends when gap with zeros starts), [153,153] (single sample)
        REQUIRE(result_intervals->size() == 2);

        REQUIRE(intervals[0].value().start == 101);
        REQUIRE(intervals[0].value().end == 102);// Ends before gap because zeros don't meet threshold
        REQUIRE(intervals[1].value().start == 153);
        REQUIRE(intervals[1].value().end == 153);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("Missing data treated as zero - negative threshold") {
        auto ats = analog_scenarios::missing_data_negative();

        params.thresholdValue = -0.5;
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        // Should get: [101,101], [152,152] (gap with zeros that don't meet negative threshold)
        // Note: zeros (0.0) > -0.5, so they don't meet the negative threshold
        REQUIRE(result_intervals->size() == 2);

        REQUIRE(intervals[0].value().start == 101);
        REQUIRE(intervals[0].value().end == 101);
        REQUIRE(intervals[1].value().start == 152);
        REQUIRE(intervals[1].value().end == 152);
    }

    SECTION("Missing data treated as zero - negative threshold where zeros DO meet threshold") {
        auto ats = analog_scenarios::missing_data_negative();

        params.thresholdValue = 0.5;// threshold > 0, so zeros (0.0) < 0.5 meet negative threshold
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        // Should get: [101,150] (interval continues through gap with zeros), [152,152]
        // Note: zeros (0.0) < 0.5, so they meet the negative threshold and extend the interval
        REQUIRE(result_intervals->size() == 2);

        REQUIRE(intervals[0].value().start == 101);
        REQUIRE(intervals[0].value().end == 150);// Interval extends through gap to just before next sample
        REQUIRE(intervals[1].value().start == 152);
        REQUIRE(intervals[1].value().end == 152);
    }

    SECTION("Missing data ignored mode") {
        auto ats = analog_scenarios::missing_data_ignore();
        auto const & values = ats->getAnalogTimeSeries();
        auto const & times = ats->getTimeSeries();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        // With IGNORE mode, gaps are not considered, so we get: [101,102] and [153,153]
        REQUIRE(result_intervals->size() == 2);

        REQUIRE(intervals[0].value().start == 101);
        REQUIRE(intervals[0].value().end == 102);// Continuous despite time gap
        REQUIRE(intervals[1].value().start == 153);
        REQUIRE(intervals[1].value().end == 153);

        // Validate that all values during intervals are above threshold
        REQUIRE(validateIntervalsAboveThreshold(values, times, intervals, params));
    }

    SECTION("No gaps in data") {
        auto ats = analog_scenarios::no_gaps();

        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        // Test both modes give same result for continuous data
        params.missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;
        auto result_zero_mode = interval_threshold(ats.get(), params);

        params.missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;
        auto result_ignore_mode = interval_threshold(ats.get(), params);

        REQUIRE(result_zero_mode != nullptr);
        REQUIRE(result_ignore_mode != nullptr);

        auto const & intervals_zero = result_zero_mode->view();
        auto const & intervals_ignore = result_ignore_mode->view();

        REQUIRE(result_zero_mode->size() == result_ignore_mode->size());
        REQUIRE(result_zero_mode->size() == 2);// [101,102] and [104,104]

        for (size_t i = 0; i < result_zero_mode->size(); ++i) {
            REQUIRE(intervals_zero[i].value().start == intervals_ignore[i].value().start);
            REQUIRE(intervals_zero[i].value().end == intervals_ignore[i].value().end);
        }
    }
}

TEST_CASE( "Data Transform: Analog Interval Threshold - JSON pipeline", "[transforms][analog_interval_threshold][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "interval_threshold_step_1"},
            {"transform_name", "Threshold Interval Detection"},
            {"input_key", "TestSignal.channel1"},
            {"output_key", "DetectedIntervals"},
            {"parameters", {
                {"threshold_value", 1.0},
                {"direction", "Positive (Rising)"},
                {"lockout_time", 0.0},
                {"min_duration", 0.0},
                {"missing_data_mode", "Ignore"}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto ats = analog_scenarios::test_signal();
    auto time_frame = ats->getTimeFrame();
    dm.setTime(TimeKey("default"), time_frame);
    dm.setData("TestSignal.channel1", ats, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto interval_series = dm.getData<DigitalIntervalSeries>("DetectedIntervals");
    REQUIRE(interval_series != nullptr);

    auto const & intervals = interval_series->view();
    REQUIRE(interval_series->size() == 2); // Two intervals: [200-400] and [600-700]

    REQUIRE(intervals[0].value().start == 200);
    REQUIRE(intervals[0].value().end == 400);
    REQUIRE(intervals[1].value().start == 600);
    REQUIRE(intervals[1].value().end == 700);
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Analog Interval Threshold - Parameter Factory", "[transforms][analog_interval_threshold][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<IntervalThresholdParams>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"threshold_value", 2.5},
        {"direction", "Negative (Falling)"},
        {"lockout_time", 123.45},
        {"min_duration", 50.0},
        {"missing_data_mode", "Treat as Zero"}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Threshold Interval Detection", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<IntervalThresholdParams*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->thresholdValue == 2.5);
    REQUIRE(params->direction == IntervalThresholdParams::ThresholdDirection::NEGATIVE);
    REQUIRE(params->lockoutTime == 123.45);
    REQUIRE(params->minDuration == 50.0);
    REQUIRE(params->missingDataMode == IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO);
}

TEST_CASE( "Data Transform: Analog Interval Threshold - load_data_from_json_config", "[transforms][analog_interval_threshold][json_config]") {
    // Create DataManager and populate it with AnalogTimeSeries from fixture
    DataManager dm;

    // Get test data from fixture
    auto test_analog = analog_scenarios::test_signal();
    auto time_frame = test_analog->getTimeFrame();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Store the analog data in DataManager with a known key
    dm.setData("test_signal", test_analog, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Interval Threshold Detection Pipeline\",\n"
        "            \"description\": \"Test interval threshold detection on analog signal\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Threshold Interval Detection\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"detected_intervals\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold_value\": 1.0,\n"
        "                    \"direction\": \"Positive (Rising)\",\n"
        "                    \"lockout_time\": 0.0,\n"
        "                    \"min_duration\": 0.0,\n"
        "                    \"missing_data_mode\": \"Ignore\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "analog_interval_threshold_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_intervals = dm.getData<DigitalIntervalSeries>("detected_intervals");
    REQUIRE(result_intervals != nullptr);
    
    // Verify the interval detection results
    auto const & intervals = result_intervals->view();
    REQUIRE(result_intervals->size() == 2); // Two intervals: [200-400] and [600-700]

    REQUIRE(intervals[0].value().start == 200);
    REQUIRE(intervals[0].value().end == 400);
    REQUIRE(intervals[1].value().start == 600);
    REQUIRE(intervals[1].value().end == 700);
    
    // Test another pipeline with different parameters (lockout time and minimum duration)
    const char* json_config_advanced = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Interval Threshold with Advanced Parameters\",\n"
        "            \"description\": \"Test interval detection with lockout and minimum duration\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Threshold Interval Detection\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"detected_intervals_advanced\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold_value\": 1.0,\n"
        "                    \"direction\": \"Positive (Rising)\",\n"
        "                    \"lockout_time\": 100.0,\n"
        "                    \"min_duration\": 150.0,\n"
        "                    \"missing_data_mode\": \"Ignore\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_advanced = test_dir / "pipeline_config_advanced.json";
    {
        std::ofstream json_file(json_filepath_advanced);
        REQUIRE(json_file.is_open());
        json_file << json_config_advanced;
        json_file.close();
    }
    
    // Execute the advanced pipeline
    auto data_info_list_advanced = load_data_from_json_config(&dm, json_filepath_advanced.string());
    
    // Verify the advanced results
    auto result_intervals_advanced = dm.getData<DigitalIntervalSeries>("detected_intervals_advanced");
    REQUIRE(result_intervals_advanced != nullptr);
    
    auto const & intervals_advanced = result_intervals_advanced->view();
    REQUIRE(result_intervals_advanced->size() == 1); // Only one interval meets minimum duration: [200-400]
    REQUIRE(intervals_advanced[0].value().start == 200);
    REQUIRE(intervals_advanced[0].value().end == 400);
    
    // Test absolute threshold detection
    const char* json_config_absolute = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Absolute Interval Threshold Detection\",\n"
        "            \"description\": \"Test absolute threshold interval detection\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Threshold Interval Detection\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"detected_intervals_absolute\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold_value\": 1.3,\n"
        "                    \"direction\": \"Absolute (Magnitude)\",\n"
        "                    \"lockout_time\": 0.0,\n"
        "                    \"min_duration\": 0.0,\n"
        "                    \"missing_data_mode\": \"Ignore\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_absolute = test_dir / "pipeline_config_absolute.json";
    {
        std::ofstream json_file(json_filepath_absolute);
        REQUIRE(json_file.is_open());
        json_file << json_config_absolute;
        json_file.close();
    }
    
    // Execute the absolute threshold pipeline
    auto data_info_list_absolute = load_data_from_json_config(&dm, json_filepath_absolute.string());
    
    // Verify the absolute threshold results
    auto result_intervals_absolute = dm.getData<DigitalIntervalSeries>("detected_intervals_absolute");
    REQUIRE(result_intervals_absolute != nullptr);
    
    auto const & intervals_absolute = result_intervals_absolute->view();
    REQUIRE(result_intervals_absolute->size() == 2); // Two intervals: [200-400] and [600-600]
    REQUIRE(intervals_absolute[0].value().start == 200);
    REQUIRE(intervals_absolute[0].value().end == 400);
    REQUIRE(intervals_absolute[1].value().start == 600);
    REQUIRE(intervals_absolute[1].value().end == 600);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}