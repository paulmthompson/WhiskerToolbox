#include "AnalogIntervalThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/analog/interval_threshold_scenarios.hpp"

#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Note: AnalogIntervalThreshold is registered in RegisteredTransforms.cpp
// ============================================================================

// ============================================================================
// Tests: Algorithm Correctness (using shared fixture)
// ============================================================================

TEST_CASE(
        "V2 Container Transform: Analog Interval Threshold - Happy Path",
        "[transforms][v2][container][analog_interval_threshold]") {

    auto & registry = ElementRegistry::instance();
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    AnalogIntervalThresholdParams params;
    ComputeContext ctx;

    // Progress tracking
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Positive threshold - simple case") {
        auto ats = analog_scenarios::positive_simple();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);

        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Negative threshold") {
        auto ats = analog_scenarios::negative_threshold();
        params.threshold_value = -1.0f;
        params.direction = "negative";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }

    SECTION("Absolute threshold") {
        auto ats = analog_scenarios::absolute_threshold();
        params.threshold_value = 1.0f;
        params.direction = "absolute";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals: [200-400] and [600-700]

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }

    SECTION("With lockout time") {
        auto ats = analog_scenarios::with_lockout();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 100.0f;// 100 time units lockout
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 3);// Three separate intervals - lockout prevents starting too soon

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 300);
        REQUIRE(intervals[1].value().end == 300);
        REQUIRE(intervals[2].value().start == 450);
        REQUIRE(intervals[2].value().end == 450);
    }

    SECTION("With minimum duration") {
        auto ats = analog_scenarios::with_min_duration();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 150.0f;// Minimum 150 time units
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);// Only one interval meets minimum duration

        REQUIRE(intervals[0].value().start == 300);
        REQUIRE(intervals[0].value().end == 500);
    }

    SECTION("Signal ends while above threshold") {
        auto ats = analog_scenarios::ends_above_threshold();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 500);// Should extend to end of signal
    }

    SECTION("No intervals detected") {
        auto ats = analog_scenarios::no_intervals();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(intervals.empty());
    }

    SECTION("Complex signal with multiple parameters") {
        auto ats = analog_scenarios::complex_signal();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 50.0f;
        params.min_duration = 100.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);// Two intervals that meet minimum duration

        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 400);
        REQUIRE(intervals[1].value().end == 500);
    }
}

TEST_CASE(
        "V2 Container Transform: Analog Interval Threshold - Edge Cases",
        "[transforms][v2][container][analog_interval_threshold]") {

    auto & registry = ElementRegistry::instance();
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    AnalogIntervalThresholdParams params;
    ComputeContext ctx;

    SECTION("Empty analog time series") {
        auto ats = analog_scenarios::empty_signal();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());
    }

    SECTION("Single sample above threshold") {
        auto ats = analog_scenarios::single_above();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 100);
    }

    SECTION("Single sample below threshold") {
        auto ats = analog_scenarios::single_below();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());
    }

    SECTION("All values above threshold") {
        auto ats = analog_scenarios::all_above();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 500);
    }

    SECTION("Cancellation support") {
        auto ats = analog_scenarios::positive_simple();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;

        // Set cancellation flag
        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        // Should return empty due to cancellation
        REQUIRE(result_intervals->view().empty());
    }

    SECTION("Very large lockout time") {
        auto ats = analog_scenarios::large_lockout();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 10000.0f;// Much larger than series
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        // Should only get first interval since lockout is so large
        REQUIRE(result_intervals->size() == 1);
    }

    SECTION("Very large minimum duration") {
        auto ats = analog_scenarios::large_min_duration();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 10000.0f;// Much larger than any interval
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        REQUIRE(result_intervals->view().empty());
    }
}

TEST_CASE(
        "V2 Container Transform: Analog Interval Threshold - Missing Data",
        "[transforms][v2][container][analog_interval_threshold]") {

    auto & registry = ElementRegistry::instance();
    std::shared_ptr<DigitalIntervalSeries> result_intervals;
    AnalogIntervalThresholdParams params;
    ComputeContext const ctx;

    SECTION("Missing data treated as zero - positive threshold") {
        auto ats = analog_scenarios::missing_data_positive();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "treat_as_zero";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        // With treat_as_zero, the gap should break the interval
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
    }

    SECTION("Missing data ignore mode") {
        auto ats = analog_scenarios::missing_data_ignore();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "ignore";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        // With ignore mode, the gap doesn't break the interval
        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);
    }

    SECTION("No gaps in data") {
        auto ats = analog_scenarios::no_gaps();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        params.min_duration = 0.0f;
        params.missing_data_mode = "treat_as_zero";

        result_intervals = registry.executeContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
                "AnalogIntervalThreshold", *ats, params, ctx);

        REQUIRE(result_intervals != nullptr);
        auto const & intervals = result_intervals->view();
        // Should detect intervals normally when there are no gaps
        REQUIRE(result_intervals->size() == 2);
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogIntervalThresholdParams - JSON Loading",
          "[transforms][v2][params][json]") {

    SECTION("Load valid JSON with all fields") {
        std::string const json = R"({
            "threshold_value": 2.5,
            "direction": "negative",
            "lockout_time": 100.0,
            "min_duration": 50.0,
            "missing_data_mode": "treat_as_zero"
        })";

        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.getThresholdValue() == 2.5f);
        REQUIRE(params.getDirection() == "negative");
        REQUIRE(params.getLockoutTime() == 100.0f);
        REQUIRE(params.getMinDuration() == 50.0f);
        REQUIRE(params.getMissingDataMode() == "treat_as_zero");
    }

    SECTION("Load empty JSON (uses defaults)") {
        std::string const json = "{}";

        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.getThresholdValue() == 1.0f);
        REQUIRE(params.getDirection() == "positive");
        REQUIRE(params.getLockoutTime() == 0.0f);
        REQUIRE(params.getMinDuration() == 0.0f);
        REQUIRE(params.getMissingDataMode() == "treat_as_zero");
    }

    SECTION("Load with only some fields") {
        std::string const json = R"({
            "threshold_value": 3.0,
            "direction": "absolute"
        })";

        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.getThresholdValue() == 3.0f);
        REQUIRE(params.getDirection() == "absolute");
        REQUIRE(params.getLockoutTime() == 0.0f);// Default
        REQUIRE(params.getMinDuration() == 0.0f);// Default
    }

    SECTION("Reject negative lockout time") {
        std::string const json = R"({
            "lockout_time": -10.0
        })";

        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);

        // Should fail validation
        REQUIRE_FALSE(result);
    }

    SECTION("Reject negative min_duration") {
        std::string const json = R"({
            "min_duration": -5.0
        })";

        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);

        // Should fail validation
        REQUIRE_FALSE(result);
    }

    SECTION("JSON round-trip preserves values") {
        AnalogIntervalThresholdParams original;
        original.threshold_value = 1.5f;
        original.direction = "positive";
        original.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(50.0f);
        original.min_duration = rfl::Validator<float, rfl::Minimum<0.0f>>(25.0f);
        original.missing_data_mode = "ignore";

        // Serialize
        std::string const json = saveParametersToJson(original);

        // Deserialize
        auto result = loadParametersFromJson<AnalogIntervalThresholdParams>(json);
        REQUIRE(result);

        auto loaded = result.value();
        REQUIRE(loaded.getThresholdValue() == 1.5f);
        REQUIRE(loaded.getDirection() == "positive");
        REQUIRE(loaded.getLockoutTime() == 50.0f);
        REQUIRE(loaded.getMinDuration() == 25.0f);
        REQUIRE(loaded.getMissingDataMode() == "ignore");
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogIntervalThresholdParams - Validation",
          "[transforms][v2][params][validation]") {

    AnalogIntervalThresholdParams params;

    SECTION("Valid direction values") {
        params.direction = "positive";
        REQUIRE(params.isValidDirection());

        params.direction = "negative";
        REQUIRE(params.isValidDirection());

        params.direction = "absolute";
        REQUIRE(params.isValidDirection());
    }

    SECTION("Invalid direction values") {
        params.direction = "invalid";
        REQUIRE_FALSE(params.isValidDirection());

        params.direction = "POSITIVE";// Case sensitive
        REQUIRE_FALSE(params.isValidDirection());
    }

    SECTION("Valid missing data mode values") {
        params.missing_data_mode = "ignore";
        REQUIRE(params.isValidMissingDataMode());

        params.missing_data_mode = "treat_as_zero";
        REQUIRE(params.isValidMissingDataMode());
    }

    SECTION("Invalid missing data mode values") {
        params.missing_data_mode = "invalid";
        REQUIRE_FALSE(params.isValidMissingDataMode());
    }

    SECTION("Default direction is valid") {
        AnalogIntervalThresholdParams const default_params;
        REQUIRE(default_params.isValidDirection());
    }

    SECTION("Default missing data mode is valid") {
        AnalogIntervalThresholdParams const default_params;
        REQUIRE(default_params.isValidMissingDataMode());
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogIntervalThreshold - Registry Integration",
          "[transforms][v2][registry][container][analog_interval_threshold]") {

    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogIntervalThreshold"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogIntervalThreshold"));// Not an element transform
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("AnalogIntervalThreshold");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogIntervalThreshold");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->supports_cancellation == true);
    }

    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE_FALSE(transforms.empty());
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogIntervalThreshold") != transforms.end());
    }
}

// ============================================================================
// Tests: DataManager Pipeline Integration (V2 JSON format)
// ============================================================================

#include "TransformsV2/core/DataManagerIntegration.hpp"

namespace {

struct IntervalThresholdTestParams {
    float threshold;
    std::string direction;
    float lockout = 0.0f;
    float min_duration = 0.0f;
    std::string missing_data_mode = "ignore";
};

AnalogIntervalThresholdParams toIntervalThresholdParams(IntervalThresholdTestParams const& cfg) {
    AnalogIntervalThresholdParams params;
    params.threshold_value = cfg.threshold;
    params.direction = cfg.direction;
    params.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(cfg.lockout);
    params.min_duration = rfl::Validator<float, rfl::Minimum<0.0f>>(cfg.min_duration);
    params.missing_data_mode = cfg.missing_data_mode;
    return params;
}

}// namespace

TEST_CASE(
        "V2 Container Transform: AnalogIntervalThreshold - DataManagerPipelineExecutor",
        "[transforms][v2][container][analog_interval_threshold][json]") {

    using namespace pipeline_json_test;

    SECTION("Execute pipeline via DataManagerPipelineExecutor") {
        DataManager dm;
        auto ats = analog_scenarios::positive_simple();
        auto time_frame = ats->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("positive_simple", ats, TimeKey("default"));

        auto const params = toIntervalThresholdParams({.threshold = 1.0f, .direction = "positive"});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold",
                "positive_simple",
                "detected_intervals",
                params,
                "interval_threshold_step_1");

        auto const exec_result = executeViaExecutor(dm, pipeline);
        REQUIRE(exec_result.load_ok);
        REQUIRE(exec_result.execution.success);

        auto interval_series = dm.getData<DigitalIntervalSeries>("detected_intervals");
        REQUIRE(interval_series != nullptr);

        auto const & intervals = interval_series->view();
        REQUIRE(interval_series->size() == 2);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }

    SECTION("Execute pipeline with lockout and min_duration parameters") {
        DataManager dm;
        auto ats = analog_scenarios::complex_signal();
        auto time_frame = ats->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("complex_signal", ats, TimeKey("default"));

        auto const params = toIntervalThresholdParams(
                {.threshold = 1.0f, .direction = "positive", .lockout = 50.0f, .min_duration = 100.0f});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold",
                "complex_signal",
                "detected_intervals_advanced",
                params,
                "interval_threshold_advanced");

        auto const exec_result = executeViaExecutor(dm, pipeline);
        REQUIRE(exec_result.load_ok);
        REQUIRE(exec_result.execution.success);

        auto interval_series = dm.getData<DigitalIntervalSeries>("detected_intervals_advanced");
        REQUIRE(interval_series != nullptr);

        auto const & intervals = interval_series->view();
        REQUIRE(interval_series->size() == 2);

        REQUIRE(intervals[0].value().start == 100);
        REQUIRE(intervals[0].value().end == 200);
        REQUIRE(intervals[1].value().start == 400);
        REQUIRE(intervals[1].value().end == 500);
    }

    SECTION("Execute pipeline with absolute threshold") {
        DataManager dm;
        auto ats = analog_scenarios::absolute_threshold();
        auto time_frame = ats->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("absolute_threshold", ats, TimeKey("default"));

        auto const params = toIntervalThresholdParams({.threshold = 1.0f, .direction = "absolute"});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold",
                "absolute_threshold",
                "detected_intervals_absolute",
                params,
                "interval_threshold_absolute");

        auto const exec_result = executeViaExecutor(dm, pipeline);
        REQUIRE(exec_result.load_ok);
        REQUIRE(exec_result.execution.success);

        auto interval_series = dm.getData<DigitalIntervalSeries>("detected_intervals_absolute");
        REQUIRE(interval_series != nullptr);

        auto const & intervals = interval_series->view();
        REQUIRE(interval_series->size() == 2);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }

    SECTION("Execute pipeline with negative threshold") {
        DataManager dm;
        auto ats = analog_scenarios::negative_threshold();
        auto time_frame = ats->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("negative_threshold", ats, TimeKey("default"));

        auto const params = toIntervalThresholdParams({.threshold = -1.0f, .direction = "negative"});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold",
                "negative_threshold",
                "detected_intervals_negative",
                params,
                "interval_threshold_negative");

        auto const exec_result = executeViaExecutor(dm, pipeline);
        REQUIRE(exec_result.load_ok);
        REQUIRE(exec_result.execution.success);

        auto interval_series = dm.getData<DigitalIntervalSeries>("detected_intervals_negative");
        REQUIRE(interval_series != nullptr);

        auto const & intervals = interval_series->view();
        REQUIRE(interval_series->size() == 2);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }
}

TEST_CASE(
        "V2 Container Transform: AnalogIntervalThreshold - load_data_from_json_config_v2",
        "[transforms][v2][container][analog_interval_threshold][json_config]") {

    using namespace pipeline_json_test;

    SECTION("Execute V2 pipeline via load_data_from_json_config_v2") {
        DataManager dm;
        auto test_analog = analog_scenarios::test_signal();
        auto time_frame = test_analog->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("test_signal", test_analog, TimeKey("default"));

        auto const params = toIntervalThresholdParams({.threshold = 1.0f, .direction = "positive"});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold", "test_signal", "v2_detected_intervals", params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_intervals = dm.getData<DigitalIntervalSeries>("v2_detected_intervals");
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 2);

        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
        REQUIRE(intervals[1].value().start == 600);
        REQUIRE(intervals[1].value().end == 700);
    }

    SECTION("Execute V2 pipeline with advanced parameters") {
        DataManager dm;
        auto test_analog = analog_scenarios::test_signal();
        auto time_frame = test_analog->getTimeFrame();
        dm.setTime(TimeKey("default"), time_frame);
        dm.setData("test_signal", test_analog, TimeKey("default"));

        auto const params = toIntervalThresholdParams(
                {.threshold = 1.0f, .direction = "positive", .lockout = 100.0f, .min_duration = 150.0f});
        auto const pipeline = makeSingleStepPipeline(
                "AnalogIntervalThreshold",
                "test_signal",
                "v2_detected_intervals_advanced",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_intervals = dm.getData<DigitalIntervalSeries>("v2_detected_intervals_advanced");
        REQUIRE(result_intervals != nullptr);

        auto const & intervals = result_intervals->view();
        REQUIRE(result_intervals->size() == 1);
        REQUIRE(intervals[0].value().start == 200);
        REQUIRE(intervals[0].value().end == 400);
    }
}
