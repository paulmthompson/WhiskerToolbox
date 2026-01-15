/**
 * @file test_pipeline_adaptors.test.cpp
 * @brief Tests for TransformPipeline view adaptor and reducer binding
 *
 * These tests verify Phase 4 of the GatherResult + TransformPipeline integration:
 * 1. bindToView() - produces view adaptors from pipelines
 * 2. bindToViewWithContext() - context-aware view adaptor factories
 * 3. bindReducer() - produces reducers from pipelines with range reductions
 * 4. bindReducerWithContext() - context-aware reducer factories
 *
 * The transform pipeline works with TimeFrameIndex (which is in ElementVariant).
 * Callers extract .time() from their element types (EventWithId, TimeValuePoint, etc.)
 * before passing through the pipeline.
 *
 * @see GATHER_PIPELINE_DESIGN.md for the complete design document
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"
#include "transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.hpp"
#include "transforms/v2/core/ContextAwareParams.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/ViewAdaptorTypes.hpp"

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
        pipeline.setRangeReduction<EventWithId, int>("EventCount");
        
        CHECK(pipeline.hasRangeReduction());
        auto const & reduction = pipeline.getRangeReduction();
        REQUIRE(reduction.has_value());
        CHECK(reduction->reduction_name == "EventCount");
        CHECK(reduction->input_type == typeid(EventWithId));
        CHECK(reduction->output_type == typeid(int));
    }

    SECTION("setRangeReduction with parameters") {
        struct WindowParams { float start{0.0f}; float end{100.0f}; };
        
        TransformPipeline pipeline;
        pipeline.setRangeReduction<EventWithId, int, WindowParams>(
            "EventCountInWindow", WindowParams{.start = 10.0f, .end = 50.0f});
        
        CHECK(pipeline.hasRangeReduction());
        auto const & reduction = pipeline.getRangeReduction();
        REQUIRE(reduction.has_value());
        CHECK(reduction->params_type == typeid(WindowParams));
    }

    SECTION("clearRangeReduction removes the reduction") {
        TransformPipeline pipeline;
        pipeline.setRangeReduction<EventWithId, int>("EventCount");
        CHECK(pipeline.hasRangeReduction());
        
        pipeline.clearRangeReduction();
        CHECK_FALSE(pipeline.hasRangeReduction());
    }

    SECTION("Chaining with addStep") {
        TransformPipeline pipeline;
        
        // Should be able to chain addStep and setRangeReduction
        pipeline
            .addStep("SomeTransform")
            .setRangeReduction<EventWithId, float>("SomeReduction");
        
        CHECK(pipeline.size() == 1);
        CHECK(pipeline.hasRangeReduction());
    }
}

TEST_CASE("TransformPipeline - isElementWiseOnly", "[PipelineAdaptors][Pipeline]") {
    TemporalTransformFixture fixture;

    SECTION("Empty pipeline is element-wise only") {
        TransformPipeline pipeline;
        CHECK(pipeline.isElementWiseOnly());
    }

    SECTION("Pipeline with element-wise transform is element-wise only") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        CHECK(pipeline.isElementWiseOnly());
    }
}

// ============================================================================
// ContextInjectorRegistry Tests
// ============================================================================

TEST_CASE("ContextInjectorRegistry - registration and injection", "[PipelineAdaptors][Context]") {
    TemporalTransformFixture fixture;

    SECTION("NormalizeTimeParams injector is registered") {
        auto & registry = ContextInjectorRegistry::instance();
        CHECK(registry.hasInjector(typeid(NormalizeTimeParams)));
    }

    SECTION("Can inject context into NormalizeTimeParams via registry") {
        auto & registry = ContextInjectorRegistry::instance();
        
        std::any params_any = NormalizeTimeParams{};
        TrialContext ctx{.alignment_time = TimeFrameIndex{500}};
        
        bool injected = registry.tryInject(params_any, ctx);
        CHECK(injected);
        
        auto & params = std::any_cast<NormalizeTimeParams &>(params_any);
        CHECK(params.hasContext());
        CHECK(params.getAlignmentTime() == TimeFrameIndex{500});
    }

    SECTION("Returns false for non-registered types") {
        auto & registry = ContextInjectorRegistry::instance();
        
        struct UnregisteredParams { int value{0}; };
        std::any params_any = UnregisteredParams{.value = 42};
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        
        bool injected = registry.tryInject(params_any, ctx);
        CHECK_FALSE(injected);
        
        // Original value unchanged
        CHECK(std::any_cast<UnregisteredParams>(params_any).value == 42);
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
// bindToViewWithContext Tests
// ============================================================================

TEST_CASE("bindToViewWithContext - basic functionality", "[PipelineAdaptors][bindToViewWithContext]") {
    TemporalTransformFixture fixture;

    SECTION("Creates factory that produces context-aware adaptors") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<TimeFrameIndex, float>(pipeline);
        REQUIRE(factory);
        
        // Create adaptor with context for trial 1 (alignment at 100)
        TrialContext ctx1{.alignment_time = TimeFrameIndex{100}};
        auto adaptor1 = factory(ctx1);
        
        std::vector<TimeFrameIndex> times1 = {
            TimeFrameIndex{120}
        };
        auto result1 = adaptor1(std::span<TimeFrameIndex const>{times1});
        REQUIRE(result1.size() == 1);
        CHECK_THAT(result1[0], WithinAbs(20.0f, 0.001f));  // 120 - 100
        
        // Create adaptor with context for trial 2 (alignment at 300)
        TrialContext ctx2{.alignment_time = TimeFrameIndex{300}};
        auto adaptor2 = factory(ctx2);
        
        std::vector<TimeFrameIndex> times2 = {
            TimeFrameIndex{350}
        };
        auto result2 = adaptor2(std::span<TimeFrameIndex const>{times2});
        REQUIRE(result2.size() == 1);
        CHECK_THAT(result2[0], WithinAbs(50.0f, 0.001f));  // 350 - 300
    }

    SECTION("Each factory call gets fresh context") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<TimeFrameIndex, float>(pipeline);
        
        // Same times, different alignments
        std::vector<TimeFrameIndex> times = {
            TimeFrameIndex{500}
        };
        
        TrialContext ctx_a{.alignment_time = TimeFrameIndex{400}};
        auto result_a = factory(ctx_a)(std::span<TimeFrameIndex const>{times});
        CHECK_THAT(result_a[0], WithinAbs(100.0f, 0.001f));  // 500 - 400
        
        TrialContext ctx_b{.alignment_time = TimeFrameIndex{450}};
        auto result_b = factory(ctx_b)(std::span<TimeFrameIndex const>{times});
        CHECK_THAT(result_b[0], WithinAbs(50.0f, 0.001f));   // 500 - 450
    }
}

// ============================================================================
// bindReducer Tests
// ============================================================================

TEST_CASE("bindReducer - basic functionality", "[PipelineAdaptors][bindReducer]") {
    TemporalTransformFixture fixture;

    SECTION("Pipeline without range reduction throws") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        
        CHECK_THROWS_AS(
            (bindReducer<TimeFrameIndex, int>(pipeline)),
            std::runtime_error);
    }

    // Note: Full bindReducer tests require the range reductions to be 
    // compatible with the intermediate element types, which may need
    // additional registrations
}

// ============================================================================
// bindReducerWithContext Tests  
// ============================================================================

TEST_CASE("bindReducerWithContext - basic functionality", "[PipelineAdaptors][bindReducerWithContext]") {
    TemporalTransformFixture fixture;

    SECTION("Pipeline without range reduction throws") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        
        CHECK_THROWS_AS(
            (bindReducerWithContext<TimeFrameIndex, int>(pipeline)),
            std::runtime_error);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Integration - Raster plot workflow", "[PipelineAdaptors][Integration]") {
    TemporalTransformFixture fixture;

    SECTION("Normalize multiple trials with different alignments") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeTimeValue", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<TimeFrameIndex, float>(pipeline);
        
        // Simulate three trials with different alignment times
        // In real usage, times would be extracted from EventWithId via .time()
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
            TrialContext ctx{.alignment_time = trial.alignment};
            auto adaptor = factory(ctx);
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
