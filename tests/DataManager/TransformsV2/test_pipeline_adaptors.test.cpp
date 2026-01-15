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
 * @brief Create a simple set of test events
 */
std::vector<EventWithId> makeTestEvents() {
    return {
        EventWithId{TimeFrameIndex{100}, EntityId{1}},
        EventWithId{TimeFrameIndex{150}, EntityId{2}},
        EventWithId{TimeFrameIndex{200}, EntityId{1}},
        EventWithId{TimeFrameIndex{250}, EntityId{3}}
    };
}

/**
 * @brief Create test events around an alignment point
 */
std::vector<EventWithId> makeTrialEvents(TimeFrameIndex alignment) {
    int64_t base = alignment.getValue();
    return {
        EventWithId{TimeFrameIndex{base - 10}, EntityId{1}},  // Before alignment
        EventWithId{TimeFrameIndex{base + 20}, EntityId{2}},  // After alignment
        EventWithId{TimeFrameIndex{base + 50}, EntityId{1}},  // After alignment
        EventWithId{TimeFrameIndex{base + 80}, EntityId{3}}   // After alignment
    };
}

}  // anonymous namespace

// ============================================================================
// ViewAdaptorTypes Tests
// ============================================================================

TEST_CASE("ViewAdaptorTypes - type definitions", "[PipelineAdaptors][Types]") {
    SECTION("ViewAdaptorFn signature") {
        // Verify the type alias compiles and has expected signature
        ViewAdaptorFn<EventWithId, NormalizedEvent> adaptor = 
            [](std::span<EventWithId const>) -> std::vector<NormalizedEvent> {
                return {};
            };
        REQUIRE(adaptor);  // Should be callable
    }

    SECTION("ViewAdaptorFactory signature") {
        ViewAdaptorFactory<EventWithId, NormalizedEvent> factory =
            [](TrialContext const&) -> ViewAdaptorFn<EventWithId, NormalizedEvent> {
                return [](std::span<EventWithId const>) { return std::vector<NormalizedEvent>{}; };
            };
        REQUIRE(factory);
    }

    SECTION("ReducerFn signature") {
        ReducerFn<EventWithId, int> reducer =
            [](std::span<EventWithId const> events) -> int {
                return static_cast<int>(events.size());
            };
        REQUIRE(reducer);
    }

    SECTION("ReducerFactory signature") {
        ReducerFactory<EventWithId, int> factory =
            [](TrialContext const&) -> ReducerFn<EventWithId, int> {
                return [](std::span<EventWithId const> events) { 
                    return static_cast<int>(events.size()); 
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
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
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
            (bindToView<EventWithId, EventWithId>(pipeline)),
            std::runtime_error);
    }

    SECTION("Pipeline with NormalizeEventTime produces adaptor") {
        NormalizeTimeParams params;
        params.setAlignmentTime(TimeFrameIndex{100});
        
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeEventTime", params);
        
        // This should compile and create an adaptor
        auto adaptor = bindToView<EventWithId, NormalizedEvent>(pipeline);
        REQUIRE(adaptor);
        
        // Test the adaptor
        std::vector<EventWithId> events = {
            EventWithId{TimeFrameIndex{150}, EntityId{1}},
            EventWithId{TimeFrameIndex{200}, EntityId{2}}
        };
        
        auto result = adaptor(std::span<EventWithId const>{events});
        REQUIRE(result.size() == 2);
        
        // First event: 150 - 100 = 50
        CHECK_THAT(result[0].normalized_time, WithinAbs(50.0f, 0.001f));
        CHECK(result[0].entity_id == EntityId{1});
        
        // Second event: 200 - 100 = 100
        CHECK_THAT(result[1].normalized_time, WithinAbs(100.0f, 0.001f));
        CHECK(result[1].entity_id == EntityId{2});
    }
}

// ============================================================================
// bindToViewWithContext Tests
// ============================================================================

TEST_CASE("bindToViewWithContext - basic functionality", "[PipelineAdaptors][bindToViewWithContext]") {
    TemporalTransformFixture fixture;

    SECTION("Creates factory that produces context-aware adaptors") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<EventWithId, NormalizedEvent>(pipeline);
        REQUIRE(factory);
        
        // Create adaptor with context for trial 1 (alignment at 100)
        TrialContext ctx1{.alignment_time = TimeFrameIndex{100}};
        auto adaptor1 = factory(ctx1);
        
        std::vector<EventWithId> events1 = {
            EventWithId{TimeFrameIndex{120}, EntityId{1}}
        };
        auto result1 = adaptor1(std::span<EventWithId const>{events1});
        REQUIRE(result1.size() == 1);
        CHECK_THAT(result1[0].normalized_time, WithinAbs(20.0f, 0.001f));  // 120 - 100
        
        // Create adaptor with context for trial 2 (alignment at 300)
        TrialContext ctx2{.alignment_time = TimeFrameIndex{300}};
        auto adaptor2 = factory(ctx2);
        
        std::vector<EventWithId> events2 = {
            EventWithId{TimeFrameIndex{350}, EntityId{2}}
        };
        auto result2 = adaptor2(std::span<EventWithId const>{events2});
        REQUIRE(result2.size() == 1);
        CHECK_THAT(result2[0].normalized_time, WithinAbs(50.0f, 0.001f));  // 350 - 300
    }

    SECTION("Each factory call gets fresh context") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<EventWithId, NormalizedEvent>(pipeline);
        
        // Same events, different alignments
        std::vector<EventWithId> events = {
            EventWithId{TimeFrameIndex{500}, EntityId{1}}
        };
        
        TrialContext ctx_a{.alignment_time = TimeFrameIndex{400}};
        auto result_a = factory(ctx_a)(std::span<EventWithId const>{events});
        CHECK_THAT(result_a[0].normalized_time, WithinAbs(100.0f, 0.001f));  // 500 - 400
        
        TrialContext ctx_b{.alignment_time = TimeFrameIndex{450}};
        auto result_b = factory(ctx_b)(std::span<EventWithId const>{events});
        CHECK_THAT(result_b[0].normalized_time, WithinAbs(50.0f, 0.001f));   // 500 - 450
    }
}

// ============================================================================
// bindReducer Tests
// ============================================================================

TEST_CASE("bindReducer - basic functionality", "[PipelineAdaptors][bindReducer]") {
    TemporalTransformFixture fixture;

    SECTION("Pipeline without range reduction throws") {
        TransformPipeline pipeline;
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
        
        CHECK_THROWS_AS(
            (bindReducer<EventWithId, int>(pipeline)),
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
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
        
        CHECK_THROWS_AS(
            (bindReducerWithContext<EventWithId, int>(pipeline)),
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
        pipeline.addStep("NormalizeEventTime", NormalizeTimeParams{});
        
        auto factory = bindToViewWithContext<EventWithId, NormalizedEvent>(pipeline);
        
        // Simulate three trials with different alignment times
        struct Trial {
            TimeFrameIndex alignment;
            std::vector<EventWithId> events;
        };
        
        std::vector<Trial> trials = {
            {TimeFrameIndex{100}, {
                EventWithId{TimeFrameIndex{110}, EntityId{1}},
                EventWithId{TimeFrameIndex{150}, EntityId{2}}
            }},
            {TimeFrameIndex{300}, {
                EventWithId{TimeFrameIndex{280}, EntityId{1}},  // Before alignment
                EventWithId{TimeFrameIndex{350}, EntityId{2}}
            }},
            {TimeFrameIndex{500}, {
                EventWithId{TimeFrameIndex{525}, EntityId{1}},
                EventWithId{TimeFrameIndex{575}, EntityId{2}},
                EventWithId{TimeFrameIndex{600}, EntityId{3}}
            }}
        };
        
        std::vector<std::vector<NormalizedEvent>> normalized_trials;
        
        for (auto const & trial : trials) {
            TrialContext ctx{.alignment_time = trial.alignment};
            auto adaptor = factory(ctx);
            auto normalized = adaptor(std::span<EventWithId const>{trial.events});
            normalized_trials.push_back(std::move(normalized));
        }
        
        // Verify trial 0: alignment at 100
        REQUIRE(normalized_trials[0].size() == 2);
        CHECK_THAT(normalized_trials[0][0].normalized_time, WithinAbs(10.0f, 0.001f));   // 110-100
        CHECK_THAT(normalized_trials[0][1].normalized_time, WithinAbs(50.0f, 0.001f));   // 150-100
        
        // Verify trial 1: alignment at 300 (includes pre-alignment event)
        REQUIRE(normalized_trials[1].size() == 2);
        CHECK_THAT(normalized_trials[1][0].normalized_time, WithinAbs(-20.0f, 0.001f));  // 280-300
        CHECK_THAT(normalized_trials[1][1].normalized_time, WithinAbs(50.0f, 0.001f));   // 350-300
        
        // Verify trial 2: alignment at 500
        REQUIRE(normalized_trials[2].size() == 3);
        CHECK_THAT(normalized_trials[2][0].normalized_time, WithinAbs(25.0f, 0.001f));   // 525-500
        CHECK_THAT(normalized_trials[2][1].normalized_time, WithinAbs(75.0f, 0.001f));   // 575-500
        CHECK_THAT(normalized_trials[2][2].normalized_time, WithinAbs(100.0f, 0.001f));  // 600-500
    }
}
