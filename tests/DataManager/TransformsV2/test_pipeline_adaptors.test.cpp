/**
 * @file test_pipeline_adaptors.test.cpp
 * @brief Tests for TransformPipeline view adaptor and reducer binding
 *
 * These tests verify:
 * 1. bindToView() - produces view adaptors from pipelines
 * 2. bindReducer() - produces reducers from pipelines with range reductions
 * 3. ViewAdaptorTypes - type definitions and factories
 *
 * The transform pipeline works with TimeFrameIndex (which is in ElementVariant).
 * Callers extract .time() from their element types (EventWithId, TimeValuePoint, etc.)
 * before passing through the pipeline.
 *
 * @note Context injection via ContextInjectorRegistry has been removed.
 * Use the V2 pattern with PipelineValueStore and parameter bindings instead.
 * See PIPELINE_VALUE_STORE_ROADMAP.md for details.
 *
 * @see GATHER_PIPELINE_DESIGN.md for the complete design document
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"
#include "transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/extension/ViewAdaptorTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <span>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Fixtures
// ============================================================================

namespace {

/**
 * @brief Ensure temporal transforms are registered
 */
struct TemporalTransformFixture {
    TemporalTransformFixture() {
        Temporal::registerTemporalTransforms();
        RangeReductions::registerAllRangeReductions();
    }
};

/**
 * @brief Create a simple set of test times
 */
std::vector<TimeFrameIndex> makeTestTimes() {
    return {
        TimeFrameIndex{100},
        TimeFrameIndex{150},
        TimeFrameIndex{200},
        TimeFrameIndex{250}
    };
}

/**
 * @brief Create test times around an alignment point
 */
std::vector<TimeFrameIndex> makeTrialTimes(TimeFrameIndex alignment) {
    int64_t base = alignment.getValue();
    return {
        TimeFrameIndex{base - 10},  // Before alignment
        TimeFrameIndex{base + 20},  // After alignment
        TimeFrameIndex{base + 50},  // After alignment
        TimeFrameIndex{base + 80}   // After alignment
    };
}

}  // anonymous namespace

// ============================================================================
// ViewAdaptorTypes Tests
// ============================================================================

TEST_CASE("ViewAdaptorTypes - type definitions", "[PipelineAdaptors][Types]") {
    SECTION("ViewAdaptorFn signature with TimeFrameIndex") {
        // Verify the type alias compiles and has expected signature
        ViewAdaptorFn<TimeFrameIndex, float> adaptor = 
            [](std::span<TimeFrameIndex const>) -> std::vector<float> {
                return {};
            };
        REQUIRE(adaptor);  // Should be callable
    }

    SECTION("ViewAdaptorFactory signature") {
        ViewAdaptorFactory<TimeFrameIndex, float> factory =
            [](TrialContext const&) -> ViewAdaptorFn<TimeFrameIndex, float> {
                return [](std::span<TimeFrameIndex const>) { return std::vector<float>{}; };
            };
        REQUIRE(factory);
    }

    SECTION("ReducerFn signature") {
        ReducerFn<TimeFrameIndex, int> reducer =
            [](std::span<TimeFrameIndex const> times) -> int {
                return static_cast<int>(times.size());
            };
        REQUIRE(reducer);
    }

    SECTION("ReducerFactory signature") {
        ReducerFactory<TimeFrameIndex, int> factory =
            [](TrialContext const&) -> ReducerFn<TimeFrameIndex, int> {
                return [](std::span<TimeFrameIndex const> times) { 
                    return static_cast<int>(times.size()); 
                };
            };
        REQUIRE(factory);
    }
}

TEST_CASE("RangeReductionStep - construction", "[PipelineAdaptors][RangeReductionStep]") {
    SECTION("Default construction") {
        RangeReductionStep step;
        CHECK(step.reduction_name.empty());
        CHECK(step.input_type == typeid(void));
        CHECK(step.output_type == typeid(void));
    }

    SECTION("Construction with parameters") {
        struct TestParams { float threshold{0.5f}; };
        RangeReductionStep step{"TestReduction", TestParams{.threshold = 0.75f}};
        
        CHECK(step.reduction_name == "TestReduction");
        CHECK(step.params_type == typeid(TestParams));
        CHECK(std::any_cast<TestParams>(step.params).threshold == 0.75f);
    }
}

// ============================================================================
// TransformPipeline Range Reduction Tests
// ============================================================================

TEST_CASE("TransformPipeline - setRangeReduction", "[PipelineAdaptors][Pipeline]") {
    SECTION("Pipeline starts without range reduction") {
        TransformPipeline pipeline;
        CHECK_FALSE(pipeline.hasRangeReduction());
        CHECK_FALSE(pipeline.getRangeReduction().has_value());
    }

    SECTION("setRangeReduction sets the reduction") {
        TransformPipeline pipeline;
        
        // Use the stateless version with template arguments
        pipeline.setRangeReduction<TimeFrameIndex, int>("Count");
        
        CHECK(pipeline.hasRangeReduction());
        auto opt_step = pipeline.getRangeReduction();
        REQUIRE(opt_step.has_value());
        CHECK(opt_step->reduction_name == "Count");
    }
}

// ============================================================================
// bindToView Tests
// ============================================================================

TEST_CASE("bindToView - basic functionality", "[PipelineAdaptors][bindToView]") {
    TemporalTransformFixture fixture;

    SECTION("Empty pipeline throws") {
        TransformPipeline pipeline;
        CHECK_THROWS_AS(
            (bindToView<TimeFrameIndex, TimeFrameIndex>(pipeline)),
            std::runtime_error);
    }

    SECTION("Pipeline with NormalizeTimeValue produces adaptor") {
        NormalizeTimeParams params;
        params.setAlignmentTime(TimeFrameIndex{100});
        
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", params);
        
        // This should compile and create an adaptor
        auto adaptor = bindToView<TimeFrameIndex, float>(pipeline);
        REQUIRE(adaptor);
        
        // Test the adaptor - using TimeFrameIndex directly
        std::vector<TimeFrameIndex> times = {
            TimeFrameIndex{150},
            TimeFrameIndex{200}
        };
        
        auto result = adaptor(std::span<TimeFrameIndex const>{times});
        REQUIRE(result.size() == 2);
        
        // First time: 150 - 100 = 50
        CHECK_THAT(result[0], WithinAbs(50.0f, 0.001f));
        
        // Second time: 200 - 100 = 100
        CHECK_THAT(result[1], WithinAbs(100.0f, 0.001f));
    }
}

// ============================================================================
// bindReducer Tests
// ============================================================================

TEST_CASE("bindReducer - basic functionality", "[PipelineAdaptors][bindReducer]") {
    TemporalTransformFixture fixture;

    SECTION("Pipeline without range reduction throws") {
        NormalizeTimeParams params;
        params.setAlignmentTime(TimeFrameIndex{100});
        
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", params);
        
        CHECK_THROWS_AS(
            (bindReducer<TimeFrameIndex, int>(pipeline)),
            std::runtime_error);
    }

    // Note: Full bindReducer tests require the range reductions to be 
    // compatible with the intermediate element types, which may need
    // additional registrations
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Integration - Raster plot workflow with pre-set alignment", "[PipelineAdaptors][Integration]") {
    TemporalTransformFixture fixture;

    SECTION("Normalize times with pre-set alignment") {
        // In V2 pattern, alignment time is set via parameter bindings from PipelineValueStore
        // This test shows the simpler pre-set alignment approach
        NormalizeTimeParams params;
        params.setAlignmentTime(TimeFrameIndex{100});
        
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", params);
        
        auto adaptor = bindToView<TimeFrameIndex, float>(pipeline);
        
        std::vector<TimeFrameIndex> times = {
            TimeFrameIndex{80},   // Before alignment
            TimeFrameIndex{100},  // At alignment
            TimeFrameIndex{150},  // After alignment
            TimeFrameIndex{200}   // After alignment
        };
        
        auto result = adaptor(std::span<TimeFrameIndex const>{times});
        
        REQUIRE(result.size() == 4);
        CHECK_THAT(result[0], WithinAbs(-20.0f, 0.001f));  // 80-100
        CHECK_THAT(result[1], WithinAbs(0.0f, 0.001f));    // 100-100
        CHECK_THAT(result[2], WithinAbs(50.0f, 0.001f));   // 150-100
        CHECK_THAT(result[3], WithinAbs(100.0f, 0.001f));  // 200-100
    }
}

TEST_CASE("Integration - Multiple trials with different alignments", "[PipelineAdaptors][Integration]") {
    TemporalTransformFixture fixture;

    SECTION("Process trials by creating new pipelines with pre-set alignment") {
        // For per-trial alignment, create a new pipeline or params for each trial
        // This is the approach when not using V2 parameter bindings
        struct Trial {
            TimeFrameIndex alignment;
            std::vector<TimeFrameIndex> times;
        };
        
        std::vector<Trial> trials = {
            {TimeFrameIndex{100}, {
                TimeFrameIndex{110},
                TimeFrameIndex{150}
            }},
            {TimeFrameIndex{300}, {
                TimeFrameIndex{280},  // Before alignment
                TimeFrameIndex{350}
            }},
            {TimeFrameIndex{500}, {
                TimeFrameIndex{525},
                TimeFrameIndex{575},
                TimeFrameIndex{600}
            }}
        };
        
        std::vector<std::vector<float>> normalized_trials;
        
        for (auto const & trial : trials) {
            // Create params with this trial's alignment
            NormalizeTimeParams params;
            params.setAlignmentTime(trial.alignment);
            
            TransformPipeline pipeline;
            pipeline.addStep("NormalizeTimeValue", params);
            
            auto adaptor = bindToView<TimeFrameIndex, float>(pipeline);
            auto normalized = adaptor(std::span<TimeFrameIndex const>{trial.times});
            normalized_trials.push_back(std::move(normalized));
        }
        
        // Verify trial 0: alignment at 100
        REQUIRE(normalized_trials[0].size() == 2);
        CHECK_THAT(normalized_trials[0][0], WithinAbs(10.0f, 0.001f));   // 110-100
        CHECK_THAT(normalized_trials[0][1], WithinAbs(50.0f, 0.001f));   // 150-100
        
        // Verify trial 1: alignment at 300 (includes pre-alignment time)
        REQUIRE(normalized_trials[1].size() == 2);
        CHECK_THAT(normalized_trials[1][0], WithinAbs(-20.0f, 0.001f));  // 280-300
        CHECK_THAT(normalized_trials[1][1], WithinAbs(50.0f, 0.001f));   // 350-300
        
        // Verify trial 2: alignment at 500
        REQUIRE(normalized_trials[2].size() == 3);
        CHECK_THAT(normalized_trials[2][0], WithinAbs(25.0f, 0.001f));   // 525-500
        CHECK_THAT(normalized_trials[2][1], WithinAbs(75.0f, 0.001f));   // 575-500
        CHECK_THAT(normalized_trials[2][2], WithinAbs(100.0f, 0.001f));  // 600-500
    }
}
