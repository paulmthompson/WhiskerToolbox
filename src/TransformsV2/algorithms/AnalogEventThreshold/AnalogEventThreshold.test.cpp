#include "AnalogEventThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"//registerContainerTransform
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/vectors/analog/analog_event_threshold_test_helpers.hpp"
#include "fixtures/vectors/analog/analog_event_threshold_vectors.hpp"

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

using analog_event_threshold_test::buildAnalogTimeSeries;
using analog_event_threshold_test::requireEventTimes;
using analog_event_threshold_vectors::algorithmCases;
using analog_event_threshold_vectors::Direction;
using analog_event_threshold_vectors::findCaseByDmKey;

bool isHappyPathCase(std::string_view dm_key) {
    return dm_key != "empty_signal" && dm_key != "lockout_larger_than_duration" &&
           dm_key != "events_at_threshold" && dm_key != "zero_based_timestamps";
}

AnalogEventThresholdParams toAnalogEventThresholdParams(analog_event_threshold_vectors::Case const & tc) {
    AnalogEventThresholdParams params;
    params.threshold_value = tc.threshold;
    params.lockout_time = tc.lockout;
    switch (tc.direction) {
        case Direction::positive:
            params.direction = AnalogEventThresholdParams::Direction::positive;
            break;
        case Direction::negative:
            params.direction = AnalogEventThresholdParams::Direction::negative;
            break;
        case Direction::absolute:
            params.direction = AnalogEventThresholdParams::Direction::absolute;
            break;
    }
    return params;
}

}// namespace

// ============================================================================
// Tests: Algorithm Correctness (shared I/O vectors)
// ============================================================================

TEST_CASE("V2 Container Transform: Analog Event Threshold - Happy Path",
          "[transforms][v2][container][analog_event_threshold]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };

    for (auto const & tc: algorithmCases()) {
        if (!isHappyPathCase(tc.dm_key)) {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toAnalogEventThresholdParams(tc);

            auto result_events =
                    registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                       AnalogEventThresholdParams>(
                            "AnalogEventThreshold", *ats, params, ctx);

            requireEventTimes(*result_events, tc.expected_event_times);

            if (tc.dm_key == "positive_no_lockout") {
                REQUIRE(progress_val == 100);
                REQUIRE(call_count > 0);
            }
        }
    }
}

TEST_CASE("V2 Container Transform: Analog Event Threshold - Edge Cases",
          "[transforms][v2][container][analog_event_threshold]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    for (auto const & tc: algorithmCases()) {
        if (tc.dm_key != "empty_signal" && tc.dm_key != "lockout_larger_than_duration" &&
            tc.dm_key != "events_at_threshold" && tc.dm_key != "zero_based_timestamps") {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toAnalogEventThresholdParams(tc);

            auto result_events =
                    registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                       AnalogEventThresholdParams>(
                            "AnalogEventThreshold", *ats, params, ctx);

            REQUIRE(result_events != nullptr);
            if (tc.dm_key == "lockout_larger_than_duration") {
                REQUIRE(result_events->size() == 1);
            } else {
                requireEventTimes(*result_events, tc.expected_event_times);
            }
        }
    }

    SECTION("Cancellation support") {
        auto const * tc = findCaseByDmKey("all_events_low_threshold");
        REQUIRE(tc != nullptr);
        auto ats = buildAnalogTimeSeries(*tc);
        auto const params = toAnalogEventThresholdParams(*tc);

        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };

        auto result_events =
                registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                   AnalogEventThresholdParams>(
                        "AnalogEventThreshold", *ats, params, ctx);

        REQUIRE(result_events->view().empty());
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogEventThresholdParams - JSON Loading",
          "[transforms][v2][params][json]") {

    SECTION("Load valid JSON with all fields") {
        std::string const json = R"({
            "threshold_value": 2.5,
            "direction": "negative",
            "lockout_time": 100.0
        })";

        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.threshold_value == 2.5f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::negative);
        REQUIRE(params.lockout_time.value() == 100.0f);
    }

    SECTION("Load empty JSON (uses defaults)") {
        std::string const json = "{}";

        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.threshold_value == 1.0f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::positive);
        REQUIRE(params.lockout_time.value() == 0.0f);
    }

    SECTION("Load with only some fields") {
        std::string const json = R"({
            "threshold_value": 3.0,
            "direction": "absolute"
        })";

        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);

        REQUIRE(result);
        auto params = result.value();

        REQUIRE(params.threshold_value == 3.0f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::absolute);
        REQUIRE(params.lockout_time.value() == 0.0f);// Default
    }

    SECTION("Reject negative lockout time") {
        std::string const json = R"({
            "lockout_time": -10.0
        })";

        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);

        // Should fail validation
        REQUIRE_FALSE(result);
    }

    SECTION("JSON round-trip preserves values") {
        AnalogEventThresholdParams original;
        original.threshold_value = 1.5f;
        original.direction = AnalogEventThresholdParams::Direction::positive;
        original.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(50.0f);

        // Serialize
        std::string const json = saveParametersToJson(original);

        // Deserialize
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        REQUIRE(result);
        auto recovered = result.value();

        // Verify values match
        REQUIRE(recovered.threshold_value == 1.5f);
        REQUIRE(recovered.direction == AnalogEventThresholdParams::Direction::positive);
        REQUIRE(recovered.lockout_time.value() == 50.0f);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: Registry Integration",
          "[transforms][v2][registry][container]") {

    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogEventThreshold"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogEventThreshold"));// Not an element transform
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("AnalogEventThreshold");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogEventThreshold");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->supports_cancellation == true);
    }

    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE_FALSE(transforms.empty());
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogEventThreshold") != transforms.end());
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

namespace {

void populateDataManagerWithThresholdVectors(DataManager & dm) {
    for (auto const & tc: algorithmCases()) {
        std::string const time_key = std::string(tc.dm_key) + "_time";
        dm.setData(std::string(tc.dm_key), buildAnalogTimeSeries(tc), TimeKey(time_key));
    }
}

}// namespace

TEST_CASE("V2 Container Transform: AnalogEventThreshold - load_data_from_json_config_v2",
          "[transforms][v2][container][analog_event_threshold][json_config]") {

    using namespace pipeline_json_test;

    DataManager dm;
    populateDataManagerWithThresholdVectors(dm);

    for (auto const & tc: algorithmCases()) {
        DYNAMIC_SECTION("JSON: " + std::string(tc.name)) {
            auto const params = toAnalogEventThresholdParams(tc);
            std::string const output_key = "v2_" + std::string(tc.dm_key) + "_out";
            auto const pipeline = makeSingleStepPipeline(
                    "AnalogEventThreshold", std::string(tc.dm_key), output_key, params);

            executeViaLoadDataFromJsonConfigV2(dm, pipeline);

            auto result_events = dm.getData<DigitalEventSeries>(output_key);
            REQUIRE(result_events != nullptr);

            if (tc.dm_key == "lockout_larger_than_duration") {
                REQUIRE(result_events->size() == 1);
            } else {
                requireEventTimes(*result_events, tc.expected_event_times);
            }
        }
    }
}
