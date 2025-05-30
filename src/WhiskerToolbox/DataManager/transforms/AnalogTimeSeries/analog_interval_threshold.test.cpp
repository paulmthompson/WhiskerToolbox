#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/transforms/AnalogTimeSeries/analog_interval_threshold.hpp"
#include "transforms/data_transforms.hpp"

#include <vector>
#include <memory>
#include <functional>

TEST_CASE("Interval Threshold Happy Path", "[transforms][analog_interval_threshold]") {
    std::vector<float> values;
    std::vector<size_t> times;
    std::shared_ptr<AnalogTimeSeries> ats;
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    IntervalThresholdParams params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Positive threshold - simple case") {
        values = {0.5f, 1.5f, 2.0f, 1.8f, 0.8f, 2.5f, 1.2f, 0.3f};
        times = {100, 200, 300, 400, 500, 600, 700, 800};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two intervals: [200-400] and [600-700]
        
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 400);
        REQUIRE(intervals[1].start == 600);
        REQUIRE(intervals[1].end == 700);

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_intervals = interval_threshold(ats.get(), params, cb);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Negative threshold") {
        values = {0.5f, -1.5f, -2.0f, -1.8f, 0.8f, -2.5f, -1.2f, 0.3f};
        times = {100, 200, 300, 400, 500, 600, 700, 800};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = -1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two intervals: [200-400] and [600-700]
        
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 400);
        REQUIRE(intervals[1].start == 600);
        REQUIRE(intervals[1].end == 700);
    }

    SECTION("Absolute threshold") {
        values = {0.5f, 1.5f, -2.0f, 1.8f, 0.8f, -2.5f, 1.2f, 0.3f};
        times = {100, 200, 300, 400, 500, 600, 700, 800};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two intervals: [200-400] and [600-700]
        
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 400);
        REQUIRE(intervals[1].start == 600);
        REQUIRE(intervals[1].end == 700);
    }

    SECTION("With lockout time") {
        values = {0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f, 0.3f};
        times = {100, 200, 250, 300, 400, 450, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 100.0; // 100 time units lockout
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2); // Should merge close intervals due to lockout
        
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 300);
        REQUIRE(intervals[1].start == 450);
        REQUIRE(intervals[1].end == 450);
    }

    SECTION("With minimum duration") {
        values = {0.5f, 1.5f, 0.8f, 1.8f, 1.2f, 1.1f, 0.5f};
        times = {100, 200, 250, 300, 400, 500, 600};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 150.0; // Minimum 150 time units

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1); // Only one interval meets minimum duration
        
        REQUIRE(intervals[0].start == 300);
        REQUIRE(intervals[0].end == 500);
    }

    SECTION("Signal ends while above threshold") {
        values = {0.5f, 1.5f, 2.0f, 1.8f, 1.2f};
        times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1);
        
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 500); // Should extend to end of signal
    }

    SECTION("No intervals detected") {
        values = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
        times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.empty());
    }

    SECTION("Progress callback detailed check") {
        values = {0.5f, 1.5f, 0.8f, 2.0f, 0.3f};
        times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
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
            REQUIRE(progress_values_seen[i] >= progress_values_seen[i-1]);
        }
    }

    SECTION("Complex signal with multiple parameters") {
        values = {0.0f, 2.0f, 1.8f, 1.5f, 0.5f, 2.5f, 2.2f, 1.9f, 0.8f, 1.1f, 0.3f};
        times = {0, 100, 150, 200, 300, 400, 450, 500, 600, 700, 800};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 50.0;
        params.minDuration = 100.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1); // Should merge and filter based on parameters
        
        REQUIRE(intervals[0].start == 100);
        REQUIRE(intervals[0].end == 500);
    }
}

TEST_CASE("Interval Threshold Error and Edge Cases", "[transforms][analog_interval_threshold]") {
    std::shared_ptr<AnalogTimeSeries> ats;
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    IntervalThresholdParams params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Null input AnalogTimeSeries") {
        ats = nullptr;
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->getIntervalSeries().empty());
        
        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_intervals = interval_threshold(ats.get(), params, cb);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->getIntervalSeries().empty());
        // Progress callback should not be called for null input
        REQUIRE(call_count == 0);
    }

    SECTION("Empty time series") {
        std::vector<float> empty_values;
        std::vector<size_t> empty_times;
        ats = std::make_shared<AnalogTimeSeries>(empty_values, empty_times);
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->getIntervalSeries().empty());
    }

    SECTION("Single sample above threshold") {
        std::vector<float> values = {2.0f};
        std::vector<size_t> times = {100};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 100);
        REQUIRE(intervals[0].end == 100);
    }

    SECTION("Single sample below threshold") {
        std::vector<float> values = {0.5f};
        std::vector<size_t> times = {100};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.empty());
    }

    SECTION("All values above threshold") {
        std::vector<float> values = {1.5f, 2.0f, 1.8f, 2.5f, 1.2f};
        std::vector<size_t> times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 100);
        REQUIRE(intervals[0].end == 500);
    }

    SECTION("Zero threshold") {
        std::vector<float> values = {-1.0f, 0.0f, 1.0f, -0.5f, 0.5f};
        std::vector<size_t> times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 0.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2);
        REQUIRE(intervals[0].start == 300);
        REQUIRE(intervals[0].end == 300);
        REQUIRE(intervals[1].start == 500);
        REQUIRE(intervals[1].end == 500);
    }

    SECTION("Negative threshold value") {
        std::vector<float> values = {-2.0f, -1.0f, -0.5f, -1.5f, -0.8f};
        std::vector<size_t> times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = -1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2);
        REQUIRE(intervals[0].start == 100);
        REQUIRE(intervals[0].end == 100);
        REQUIRE(intervals[1].start == 400);
        REQUIRE(intervals[1].end == 400);
    }

    SECTION("Very large lockout time") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f};
        std::vector<size_t> times = {100, 200, 300, 400, 500, 600};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 1000.0; // Very large lockout
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1); // Only first interval should be detected
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 200);
    }

    SECTION("Very large minimum duration") {
        std::vector<float> values = {0.5f, 1.5f, 1.8f, 1.2f, 0.5f};
        std::vector<size_t> times = {100, 200, 300, 400, 500};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 1000.0; // Very large minimum duration

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.empty()); // No intervals meet minimum duration
    }

    SECTION("Irregular timestamp spacing") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f, 0.5f};
        std::vector<size_t> times = {0, 1, 100, 101, 1000}; // Irregular spacing
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.thresholdValue = 1.0;
        params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;
        params.minDuration = 0.0;

        result_intervals = interval_threshold(ats.get(), params);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2);
        REQUIRE(intervals[0].start == 1);
        REQUIRE(intervals[0].end == 1);
        REQUIRE(intervals[1].start == 101);
        REQUIRE(intervals[1].end == 101);
    }
}

TEST_CASE("IntervalThresholdOperation Class Tests", "[transforms][analog_interval_threshold][operation]") {
    IntervalThresholdOperation operation;
    DataTypeVariant variant;
    IntervalThresholdParams params;
    params.thresholdValue = 1.0;
    params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
    params.lockoutTime = 0.0;
    params.minDuration = 0.0;

    SECTION("Operation metadata") {
        REQUIRE(operation.getName() == "Threshold Interval Detection");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("canApply with valid data") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        REQUIRE(operation.canApply(variant));
    }

    SECTION("canApply with null shared_ptr") {
        std::shared_ptr<AnalogTimeSeries> null_ats = nullptr;
        variant = null_ats;

        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("canApply with wrong type") {
        variant = std::string("wrong type");

        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("execute with valid parameters") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));
        
        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 2);
    }

    SECTION("execute with null parameters") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        auto result = operation.execute(variant, nullptr);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));
        
        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
    }

    SECTION("execute with progress callback") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        volatile int progress_val = -1;
        volatile int call_count = 0;
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

    SECTION("execute with invalid variant") {
        variant = std::string("wrong type");

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::monostate>(result));
    }

    SECTION("execute with wrong parameter type") {
        std::vector<float> values = {0.5f, 1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
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
        std::vector<float> values = {0.5f, -1.5f, 0.8f, 1.8f};
        std::vector<size_t> times = {100, 200, 300, 400};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        // Test negative threshold
        params.direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
        params.thresholdValue = -1.0;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));
        
        auto result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
        
        auto const & intervals = result_intervals->getIntervalSeries();
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 200);
        REQUIRE(intervals[0].end == 200);

        // Test absolute threshold
        params.direction = IntervalThresholdParams::ThresholdDirection::ABSOLUTE;
        params.thresholdValue = 1.0;

        result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));
        
        result_intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_intervals != nullptr);
        
        auto const & abs_intervals = result_intervals->getIntervalSeries();
        REQUIRE(abs_intervals.size() == 2);
    }
} 