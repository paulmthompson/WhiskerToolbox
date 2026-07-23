/**
 * @file AnalogDifference.test.cpp
 * @brief Catch2 tests for the AnalogDifference container transform
 *
 * Tests difference methods, window lag, edge policies, irregular time indices,
 * edge cases, JSON parameter round-trip, registry integration, and pipeline loading.
 */

#include "TransformsV2/algorithms/AnalogDifference/AnalogDifference.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

ComputeContext makeCtx() {
    return ComputeContext{};
}

std::shared_ptr<AnalogTimeSeries> makeATS(std::vector<float> data) {
    auto const n = data.size();
    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

std::shared_ptr<AnalogTimeSeries> makeATSWithTimes(
        std::vector<float> data,
        std::vector<TimeFrameIndex> times) {
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

AnalogDifferenceParams makeParams(
        DifferenceMethod method,
        int window,
        DifferenceEdgePolicy edge_policy) {
    AnalogDifferenceParams params;
    params.method = method;
    params.window = window;
    params.edge_policy = edge_policy;
    return params;
}

bool isNaN(float value) {
    return std::isnan(value);
}

}// anonymous namespace

// ============================================================================
// Math Tests
// ============================================================================

TEST_CASE("AnalogDifference backward difference window=1",
          "[transforms][v2][analog_difference][math]") {
    auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 5);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(out[4], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("AnalogDifference forward difference window=1",
          "[transforms][v2][analog_difference][math]") {
    auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
    auto const params = makeParams(DifferenceMethod::Forward, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 5);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(out[4], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("AnalogDifference central difference window=1",
          "[transforms][v2][analog_difference][math]") {
    auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
    auto const params = makeParams(DifferenceMethod::Central, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 4);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(out[3], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("AnalogDifference backward difference window=2",
          "[transforms][v2][analog_difference][math]") {
    auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 2, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 4);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(2.0f, 1e-5f));
    REQUIRE_THAT(out[3], Catch::Matchers::WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("AnalogDifference central difference window=2",
          "[transforms][v2][analog_difference][math]") {
    auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
    auto const params = makeParams(DifferenceMethod::Central, 2, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 2);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(1.0f, 1e-5f));
}

// ============================================================================
// Edge Policy Tests
// ============================================================================

TEST_CASE("AnalogDifference Skip edge policy omits boundary samples",
          "[transforms][v2][analog_difference][edge]") {
    auto const input = makeATS({10.0f, 20.0f, 30.0f});
    auto const params = makeParams(DifferenceMethod::Central, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 1);
    REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("AnalogDifference NaN edge policy fills incomplete windows",
          "[transforms][v2][analog_difference][edge]") {
    auto const input = makeATS({10.0f, 20.0f, 30.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::NaN);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 3);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE(isNaN(out[0]));
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(out[2], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("AnalogDifference Zero edge policy fills incomplete windows",
          "[transforms][v2][analog_difference][edge]") {
    auto const input = makeATS({10.0f, 20.0f, 30.0f});
    auto const params = makeParams(DifferenceMethod::Forward, 1, DifferenceEdgePolicy::Zero);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 3);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(out[2], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("AnalogDifference Clamp edge policy uses boundary samples",
          "[transforms][v2][analog_difference][edge]") {
    auto const input = makeATS({10.0f, 20.0f, 30.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::Clamp);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 3);

    auto const out = result->getAnalogTimeSeries();
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(out[1], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(out[2], Catch::Matchers::WithinAbs(10.0f, 1e-5f));
}

// ============================================================================
// Irregular Time Indices
// ============================================================================

TEST_CASE("AnalogDifference preserves irregular TimeFrameIndex values",
          "[transforms][v2][analog_difference][time]") {
    std::vector<float> data = {0.0f, 5.0f, 10.0f, 15.0f};
    std::vector<TimeFrameIndex> times = {
            TimeFrameIndex(0),
            TimeFrameIndex(10),
            TimeFrameIndex(25),
            TimeFrameIndex(100)};

    auto const input = makeATSWithTimes(std::move(data), std::move(times));
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 3);

    auto const out_times = result->getTimeSeries();
    REQUIRE(out_times[0].getValue() == 10);
    REQUIRE(out_times[1].getValue() == 25);
    REQUIRE(out_times[2].getValue() == 100);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("AnalogDifference empty input returns nullptr",
          "[transforms][v2][analog_difference][edge_case]") {
    auto const input = makeATS({});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    REQUIRE(analogDifference(*input, params, ctx) == nullptr);
}

TEST_CASE("AnalogDifference Skip with insufficient samples returns nullptr",
          "[transforms][v2][analog_difference][edge_case]") {
    auto const input = makeATS({42.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::Skip);
    auto const ctx = makeCtx();

    REQUIRE(analogDifference(*input, params, ctx) == nullptr);
}

TEST_CASE("AnalogDifference NaN with single sample returns one output",
          "[transforms][v2][analog_difference][edge_case]") {
    auto const input = makeATS({42.0f});
    auto const params = makeParams(DifferenceMethod::Backward, 1, DifferenceEdgePolicy::NaN);
    auto const ctx = makeCtx();

    auto const result = analogDifference(*input, params, ctx);
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 1);
    REQUIRE(isNaN(result->getAnalogTimeSeries()[0]));
}

// ============================================================================
// JSON Parameter Tests
// ============================================================================

TEST_CASE("AnalogDifference JSON parameter round-trip",
          "[transforms][v2][analog_difference][json]") {
    SECTION("Serialize and deserialize") {
        AnalogDifferenceParams original;
        original.method = DifferenceMethod::Central;
        original.window = 2;
        original.edge_policy = DifferenceEdgePolicy::Clamp;

        std::string const json = saveParametersToJson(original);
        auto result = loadParametersFromJson<AnalogDifferenceParams>(json);
        REQUIRE(result);

        auto const recovered = result.value();
        REQUIRE(recovered.method == DifferenceMethod::Central);
        REQUIRE(recovered.window.value() == 2);
        REQUIRE(recovered.edge_policy == DifferenceEdgePolicy::Clamp);
    }

    SECTION("Registry-based loadParametersForTransform works") {
        std::string const json = R"({"method": "Forward", "window": 3, "edge_policy": "NaN"})";
        auto params_any = loadParametersForTransform("AnalogDifference", json);
        REQUIRE(params_any.has_value());

        auto const params = std::any_cast<AnalogDifferenceParams>(params_any);
        REQUIRE(params.method == DifferenceMethod::Forward);
        REQUIRE(params.window.value() == 3);
        REQUIRE(params.edge_policy == DifferenceEdgePolicy::NaN);
    }

    SECTION("Empty JSON uses defaults") {
        auto params_any = loadParametersForTransform("AnalogDifference", "{}");
        REQUIRE(params_any.has_value());

        auto const params = std::any_cast<AnalogDifferenceParams>(params_any);
        REQUIRE(params.method == DifferenceMethod::Backward);
        REQUIRE(params.window.value() == 1);
        REQUIRE(params.edge_policy == DifferenceEdgePolicy::Skip);
    }
}

// ============================================================================
// Registry Integration Tests
// ============================================================================

TEST_CASE("AnalogDifference registry integration",
          "[transforms][v2][analog_difference][registry][container]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogDifference"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogDifference"));
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("AnalogDifference");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogDifference");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->is_deterministic == true);
        REQUIRE(metadata->supports_cancellation == true);
    }

    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogDifference") != transforms.end());
    }

    SECTION("Parameter schema contains expected fields") {
        auto const * schema = registry.getParameterSchema("AnalogDifference");
        REQUIRE(schema != nullptr);
        REQUIRE(schema->field("method") != nullptr);
        REQUIRE(schema->field("window") != nullptr);
        REQUIRE(schema->field("edge_policy") != nullptr);
    }

    SECTION("Can execute via registry") {
        auto const input = makeATS({0.0f, 1.0f, 2.0f, 3.0f});
        AnalogDifferenceParams params;
        ComputeContext const ctx;

        auto result = registry.executeContainerTransform<
                AnalogTimeSeries, AnalogTimeSeries, AnalogDifferenceParams>(
                "AnalogDifference", *input, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 3);
    }
}

// ============================================================================
// Pipeline Loader Tests
// ============================================================================

TEST_CASE("AnalogDifference loadPipelineFromJson",
          "[transforms][v2][analog_difference][pipeline_loader]") {
    std::string const json = R"({
        "steps": [{
            "step_id": "step_1",
            "transform_name": "AnalogDifference",
            "parameters": {
                "method": "Central",
                "window": 1,
                "edge_policy": "Skip"
            }
        }]
    })";

    auto pipeline_result = loadPipelineFromJson(json);
    INFO("Error: " << (pipeline_result ? "" : pipeline_result.error()->what()));
    REQUIRE(pipeline_result);
}
