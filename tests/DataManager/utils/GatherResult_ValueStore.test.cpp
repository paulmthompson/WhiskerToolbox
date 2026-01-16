/**
 * @file GatherResult_ValueStore.test.cpp
 * @brief Tests for GatherResult V2 pattern with PipelineValueStore
 *
 * These tests verify the Phase 3 Value Store integration:
 * - buildTrialStore() - produces correct PipelineValueStore for each trial
 * - projectV2() - applies value projection factory with store bindings
 * - bindValueProjectionV2() - creates factories from pipelines with bindings
 * - NormalizeTimeParamsV2 - binding-based normalization parameters
 *
 * @see PIPELINE_VALUE_STORE_ROADMAP.md Phase 3 for design details
 */

#include "utils/GatherResult.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"
#include "transforms/v2/core/PipelineValueStore.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/extension/ParameterBinding.hpp"
#include "transforms/v2/extension/ValueProjectionTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/**
 * @brief Ensure transforms are registered
 */
struct V2TestFixture {
    V2TestFixture() {
        Temporal::registerTemporalTransforms();
    }
};

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const& times) {
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto t : times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Create a DigitalIntervalSeries with specified intervals
 */
std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const& intervals) {
    std::vector<Interval> interval_vec;
    interval_vec.reserve(intervals.size());
    for (auto const& [start, end] : intervals) {
        interval_vec.push_back(Interval{start, end});
    }
    return std::make_shared<DigitalIntervalSeries>(interval_vec);
}

}  // anonymous namespace

// =============================================================================
// buildTrialStore Tests
// =============================================================================

TEST_CASE("GatherResult - buildTrialStore", "[GatherResult][ValueStore][Phase3]") {
    // Create events spread across time
    auto events = createEventSeries({5, 15, 25, 35, 45, 55, 65, 75});
    
    // Create 3 trials with different intervals
    auto intervals = createIntervalSeries({
        {0, 20},    // Trial 0: events 5, 15
        {30, 50},   // Trial 1: events 35, 45
        {60, 80}    // Trial 2: events 65, 75
    });

    auto result = gather(events, intervals);
    REQUIRE(result.size() == 3);

    SECTION("Trial 0 store values") {
        auto store = result.buildTrialStore(0);
        
        REQUIRE(store.contains("alignment_time"));
        REQUIRE(store.contains("trial_index"));
        REQUIRE(store.contains("trial_duration"));
        REQUIRE(store.contains("end_time"));
        
        CHECK(store.getInt("alignment_time").value() == 0);
        CHECK(store.getInt("trial_index").value() == 0);
        CHECK(store.getInt("trial_duration").value() == 20);
        CHECK(store.getInt("end_time").value() == 20);
    }

    SECTION("Trial 1 store values") {
        auto store = result.buildTrialStore(1);
        
        CHECK(store.getInt("alignment_time").value() == 30);
        CHECK(store.getInt("trial_index").value() == 1);
        CHECK(store.getInt("trial_duration").value() == 20);
        CHECK(store.getInt("end_time").value() == 50);
    }

    SECTION("Trial 2 store values") {
        auto store = result.buildTrialStore(2);
        
        CHECK(store.getInt("alignment_time").value() == 60);
        CHECK(store.getInt("trial_index").value() == 2);
        CHECK(store.getInt("trial_duration").value() == 20);
        CHECK(store.getInt("end_time").value() == 80);
    }

    SECTION("Out of range throws") {
        CHECK_THROWS_AS(result.buildTrialStore(3), std::out_of_range);
        CHECK_THROWS_AS(result.buildTrialStore(100), std::out_of_range);
    }

    SECTION("Store values are correct type for JSON binding") {
        auto store = result.buildTrialStore(0);
        
        // Verify JSON representation is correct for binding
        auto json = store.getJson("alignment_time");
        REQUIRE(json.has_value());
        CHECK(json.value() == "0");
    }
}

// =============================================================================
// buildTrialStore with Reordering Tests
// =============================================================================

TEST_CASE("GatherResult - buildTrialStore with reordering", "[GatherResult][ValueStore][Phase3]") {
    // Create events
    auto events = createEventSeries({5, 15, 35, 45, 65});
    auto intervals = createIntervalSeries({
        {0, 20},    // Trial 0: events 5, 15
        {30, 50},   // Trial 1: events 35, 45
        {60, 80}    // Trial 2: event 65
    });

    auto result = gather(events, intervals);
    
    // Reorder: [2, 0, 1] (trial 2 first, then 0, then 1)
    auto reordered = result.reorder({2, 0, 1});

    SECTION("Reordered position 0 (original trial 2)") {
        auto store = reordered.buildTrialStore(0);
        
        // Should have trial 2's values
        CHECK(store.getInt("alignment_time").value() == 60);
        CHECK(store.getInt("trial_index").value() == 2);  // Original index
    }

    SECTION("Reordered position 1 (original trial 0)") {
        auto store = reordered.buildTrialStore(1);
        
        // Should have trial 0's values
        CHECK(store.getInt("alignment_time").value() == 0);
        CHECK(store.getInt("trial_index").value() == 0);  // Original index
    }

    SECTION("Reordered position 2 (original trial 1)") {
        auto store = reordered.buildTrialStore(2);
        
        // Should have trial 1's values
        CHECK(store.getInt("alignment_time").value() == 30);
        CHECK(store.getInt("trial_index").value() == 1);  // Original index
    }
}

// =============================================================================
// NormalizeTimeParamsV2 Tests
// =============================================================================

TEST_CASE("NormalizeTimeParamsV2 - basic functionality", "[NormalizeTimeParamsV2][ValueStore][Phase3]") {
    SECTION("Default initialization") {
        NormalizeTimeParamsV2 params{};
        CHECK(params.alignment_time == 0);
    }

    SECTION("Designated initialization") {
        NormalizeTimeParamsV2 params{.alignment_time = 100};
        CHECK(params.alignment_time == 100);
    }

    SECTION("Transform with params") {
        NormalizeTimeParamsV2 params{.alignment_time = 100};
        
        TimeFrameIndex event_time{125};
        float norm_time = normalizeTimeValueV2(event_time, params);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Event transform with params") {
        NormalizeTimeParamsV2 params{.alignment_time = 50};
        
        EventWithId event{TimeFrameIndex{75}, EntityId{1}};
        float norm_time = normalizeEventTimeValueV2(event, params);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }
}

// =============================================================================
// Parameter Binding Tests
// =============================================================================

TEST_CASE("NormalizeTimeParamsV2 - parameter binding", "[NormalizeTimeParamsV2][ValueStore][Phase3]") {
    V2TestFixture fixture;

    SECTION("Apply bindings from store") {
        PipelineValueStore store;
        store.set("alignment_time", int64_t{100});

        NormalizeTimeParamsV2 base_params{.alignment_time = 0};
        std::map<std::string, std::string> bindings = {
            {"alignment_time", "alignment_time"}
        };

        auto bound_params = applyBindings(base_params, bindings, store);
        CHECK(bound_params.alignment_time == 100);
    }

    SECTION("Bindings override default values") {
        PipelineValueStore store;
        store.set("trial_alignment", int64_t{500});

        NormalizeTimeParamsV2 base_params{.alignment_time = 100};
        std::map<std::string, std::string> bindings = {
            {"alignment_time", "trial_alignment"}  // Different store key
        };

        auto bound_params = applyBindings(base_params, bindings, store);
        CHECK(bound_params.alignment_time == 500);
    }

    SECTION("Missing store key throws") {
        PipelineValueStore store;
        // Don't set any values

        NormalizeTimeParamsV2 base_params{};
        std::map<std::string, std::string> bindings = {
            {"alignment_time", "nonexistent_key"}
        };

        CHECK_THROWS_AS(
            applyBindings(base_params, bindings, store),
            std::runtime_error);
    }
}

// =============================================================================
// bindValueProjectionV2 Tests
// =============================================================================

TEST_CASE("bindValueProjectionV2 - basic usage", "[TransformPipeline][ValueStore][Phase3]") {
    V2TestFixture fixture;

    SECTION("Create factory from pipeline") {
        // Build pipeline with param bindings
        TransformPipeline pipeline;
        
        // Add step with param bindings set in the step
        auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);

        auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);

        // Create store with alignment time
        PipelineValueStore store;
        store.set("alignment_time", int64_t{100});

        // Get projection from factory
        auto projection = factory(store);

        // Test projection
        EventWithId event{TimeFrameIndex{125}, EntityId{1}};
        float norm_time = projection(event);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Factory produces different projections for different stores") {
        TransformPipeline pipeline;
        auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);

        auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);

        // Create projections with different alignment times
        PipelineValueStore store1;
        store1.set("alignment_time", int64_t{0});
        auto proj1 = factory(store1);

        PipelineValueStore store2;
        store2.set("alignment_time", int64_t{100});
        auto proj2 = factory(store2);

        // Same event, different projections
        EventWithId event{TimeFrameIndex{150}, EntityId{1}};
        CHECK_THAT(proj1(event), WithinAbs(150.0f, 0.001f));  // 150 - 0
        CHECK_THAT(proj2(event), WithinAbs(50.0f, 0.001f));   // 150 - 100
    }

    SECTION("Empty pipeline throws") {
        TransformPipeline pipeline;  // Empty
        CHECK_THROWS_AS(
            (bindValueProjectionV2<EventWithId, float>(pipeline)),
            std::runtime_error);
    }
}

// =============================================================================
// projectV2 Tests
// =============================================================================

TEST_CASE("GatherResult - projectV2", "[GatherResult][ValueStore][Phase3]") {
    V2TestFixture fixture;

    // Create events
    auto events = createEventSeries({5, 15, 35, 45, 65, 75});
    auto intervals = createIntervalSeries({
        {0, 20},    // Trial 0: events 5, 15
        {30, 50},   // Trial 1: events 35, 45
        {60, 80}    // Trial 2: events 65, 75
    });

    auto result = gather(events, intervals);

    SECTION("projectV2 creates per-trial projections") {
        // Create pipeline with bindings
        TransformPipeline pipeline;
        auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);

        auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);
        auto projections = result.projectV2(factory);

        REQUIRE(projections.size() == 3);

        // Test trial 0 projection (alignment = 0)
        std::vector<float> trial0_values;
        for (auto const& event : result[0]->view()) {
            trial0_values.push_back(projections[0](event));
        }
        REQUIRE(trial0_values.size() == 2);
        CHECK_THAT(trial0_values[0], WithinAbs(5.0f, 0.001f));   // 5 - 0
        CHECK_THAT(trial0_values[1], WithinAbs(15.0f, 0.001f));  // 15 - 0

        // Test trial 1 projection (alignment = 30)
        std::vector<float> trial1_values;
        for (auto const& event : result[1]->view()) {
            trial1_values.push_back(projections[1](event));
        }
        REQUIRE(trial1_values.size() == 2);
        CHECK_THAT(trial1_values[0], WithinAbs(5.0f, 0.001f));   // 35 - 30
        CHECK_THAT(trial1_values[1], WithinAbs(15.0f, 0.001f));  // 45 - 30

        // Test trial 2 projection (alignment = 60)
        std::vector<float> trial2_values;
        for (auto const& event : result[2]->view()) {
            trial2_values.push_back(projections[2](event));
        }
        REQUIRE(trial2_values.size() == 2);
        CHECK_THAT(trial2_values[0], WithinAbs(5.0f, 0.001f));   // 65 - 60
        CHECK_THAT(trial2_values[1], WithinAbs(15.0f, 0.001f));  // 75 - 60
    }
}

// =============================================================================
// Integration: Full Raster Plot Workflow with V2 Pattern
// =============================================================================

TEST_CASE("GatherResult - V2 raster plot workflow", "[GatherResult][ValueStore][Integration][Phase3]") {
    V2TestFixture fixture;

    // Setup: Create spike events at various times
    auto spikes = createEventSeries({
        10, 25, 40,     // Trial 0 events
        110, 130,       // Trial 1 events  
        215, 220, 230   // Trial 2 events
    });

    // Create trial intervals
    auto trials = createIntervalSeries({
        {0, 50},      // Trial 0
        {100, 150},   // Trial 1
        {200, 250}    // Trial 2
    });

    auto raster = gather(spikes, trials);
    REQUIRE(raster.size() == 3);

    // Build pipeline for time normalization using V2 pattern
    TransformPipeline pipeline;
    auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
    step.param_bindings = {{"alignment_time", "alignment_time"}};
    pipeline.addStep(step);

    auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);
    auto projections = raster.projectV2(factory);

    SECTION("Verify normalized times for raster plot") {
        // Trial 0: alignment = 0
        std::vector<float> trial0_times;
        for (auto const& event : raster[0]->view()) {
            trial0_times.push_back(projections[0](event));
        }
        REQUIRE(trial0_times.size() == 3);
        CHECK_THAT(trial0_times[0], WithinAbs(10.0f, 0.001f));  // 10 - 0
        CHECK_THAT(trial0_times[1], WithinAbs(25.0f, 0.001f));  // 25 - 0
        CHECK_THAT(trial0_times[2], WithinAbs(40.0f, 0.001f));  // 40 - 0

        // Trial 1: alignment = 100
        std::vector<float> trial1_times;
        for (auto const& event : raster[1]->view()) {
            trial1_times.push_back(projections[1](event));
        }
        REQUIRE(trial1_times.size() == 2);
        CHECK_THAT(trial1_times[0], WithinAbs(10.0f, 0.001f));  // 110 - 100
        CHECK_THAT(trial1_times[1], WithinAbs(30.0f, 0.001f));  // 130 - 100

        // Trial 2: alignment = 200
        std::vector<float> trial2_times;
        for (auto const& event : raster[2]->view()) {
            trial2_times.push_back(projections[2](event));
        }
        REQUIRE(trial2_times.size() == 3);
        CHECK_THAT(trial2_times[0], WithinAbs(15.0f, 0.001f));  // 215 - 200
        CHECK_THAT(trial2_times[1], WithinAbs(20.0f, 0.001f));  // 220 - 200
        CHECK_THAT(trial2_times[2], WithinAbs(30.0f, 0.001f));  // 230 - 200
    }

    SECTION("Verify EntityId access preserved") {
        // The key benefit: we can still access EntityId from source elements
        for (size_t trial = 0; trial < raster.size(); ++trial) {
            for (auto const& event : raster[trial]->view()) {
                float norm_time = projections[trial](event);
                EntityId id = event.id();  // Still accessible!
                
                // Both values should be valid
                CHECK(norm_time >= 0.0f);
                // EntityId is valid (even if uninitialized in test data)
            }
        }
    }
}

// =============================================================================
// Comparison: V1 vs V2 Pattern
// =============================================================================

TEST_CASE("GatherResult - V1 vs V2 pattern comparison", "[GatherResult][ValueStore][Phase3]") {
    V2TestFixture fixture;

    // Same test data for both approaches
    auto events = createEventSeries({25, 75, 125});
    auto intervals = createIntervalSeries({
        {0, 50},     // Trial 0
        {100, 150}   // Trial 1
    });

    auto result = gather(events, intervals);

    SECTION("V1 pattern uses buildContext and ValueProjectionFactory") {
        // V1: Use context-aware transforms
        ValueProjectionFactory<EventWithId, float> factory_v1 = 
            [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
                NormalizeTimeParams params;
                params.setContext(ctx);
                return [params](EventWithId const& event) -> float {
                    return normalizeTimeValue(event.time(), params);
                };
            };

        auto projections_v1 = result.project(factory_v1);
        
        // Test trial 0
        EventWithId test_event{TimeFrameIndex{25}, EntityId{1}};
        CHECK_THAT(projections_v1[0](test_event), WithinAbs(25.0f, 0.001f));
    }

    SECTION("V2 pattern uses buildTrialStore and ValueProjectionFactoryV2") {
        // V2: Use pipeline with param bindings
        TransformPipeline pipeline;
        auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);

        auto factory_v2 = bindValueProjectionV2<EventWithId, float>(pipeline);
        auto projections_v2 = result.projectV2(factory_v2);
        
        // Test trial 0 - should produce same result as V1
        EventWithId test_event{TimeFrameIndex{25}, EntityId{1}};
        CHECK_THAT(projections_v2[0](test_event), WithinAbs(25.0f, 0.001f));
    }

    SECTION("Both patterns produce equivalent results") {
        // V1 setup
        ValueProjectionFactory<EventWithId, float> factory_v1 = 
            [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
                NormalizeTimeParams params;
                params.setContext(ctx);
                return [params](EventWithId const& event) -> float {
                    return normalizeTimeValue(event.time(), params);
                };
            };
        auto projections_v1 = result.project(factory_v1);

        // V2 setup
        TransformPipeline pipeline;
        auto step = PipelineStep("NormalizeEventTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);
        auto factory_v2 = bindValueProjectionV2<EventWithId, float>(pipeline);
        auto projections_v2 = result.projectV2(factory_v2);

        // Compare all trials
        for (size_t trial = 0; trial < result.size(); ++trial) {
            for (auto const& event : result[trial]->view()) {
                float v1_result = projections_v1[trial](event);
                float v2_result = projections_v2[trial](event);
                CHECK_THAT(v1_result, WithinAbs(v2_result, 0.001f));
            }
        }
    }
}
