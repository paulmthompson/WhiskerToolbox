/**
 * @file GatherResult_ValueStore.test.cpp
 * @brief Tests for GatherResult V2 pattern with PipelineValueStore
 *
 * These tests verify the Value Store integration:
 * - buildGatherRowStore() - produces correct PipelineValueStore for each trial
 * - projectGatherRows() - applies value projection factory with store bindings
 * - bindValueProjectionV2() - creates factories from pipelines with bindings
 * - NormalizeTimeParamsV2 - binding-based normalization parameters
 *
 */

#include "GatherResult/GatherResult.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TransformsV2/PipelineValueStore/PipelineValueStore.hpp"
#include "TransformsV2/algorithms/EventToInterval/EventToInterval.hpp"
#include "TransformsV2/algorithms/Temporal/NormalizeTime.hpp"
#include "TransformsV2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/extension/ParameterBinding.hpp"
#include "TransformsV2/extension/ValueProjectionTypes.hpp"
#include "TransformsV2/extension/gatherResult/GatherResultRowContext.hpp"

#include "fixtures/GatherAlignmentFixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Neuralyzer::Gather;
using namespace Neuralyzer::Transforms::V2;
using Catch::Matchers::WithinAbs;
using Neuralyzer::Test::GatherFixtures::createAlignmentEventsForIntervals;
using Neuralyzer::Test::GatherFixtures::createWindowsAroundEvents;
using Neuralyzer::Test::GatherFixtures::TestIntervalAlignmentPoint;
using Neuralyzer::Transforms::V2::Examples::EventToIntervalParams;

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/**
 * @brief Create an identity TimeFrame large enough for the supplied maximum time.
 */
std::shared_ptr<TimeFrame> createIdentityTimeFrameForMax(int64_t max_time) {
    auto const size = static_cast<int>(std::max<int64_t>(max_time + 10'000, 10'000));
    std::vector<int> times(static_cast<std::size_t>(size));
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Ensure transforms are registered
 */
struct V2TestFixture {
    V2TestFixture() {
        Temporal::registerTemporalTransforms();
        ElementRegistry::instance().registerContainerTransform<DigitalEventSeries, DigitalIntervalSeries, EventToIntervalParams>(
                "EventToInterval",
                Examples::eventToInterval);
    }
};

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    auto const max_time = times.empty() ? int64_t{0} : *std::max_element(times.begin(), times.end());
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    for (auto t: times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Create a DigitalIntervalSeries with specified intervals
 */
std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<TimeFrameInterval> interval_vec;
    interval_vec.reserve(intervals.size());
    int64_t max_time = 0;
    for (auto const & [start, end]: intervals) {
        interval_vec.push_back(TimeFrameInterval{TimeFrameIndex(start), TimeFrameIndex(end)});
        max_time = std::max(max_time, end);
    }
    auto series = std::make_shared<DigitalIntervalSeries>(interval_vec);
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    return series;
}

/**
 * @brief Get a required integer value from a PipelineValueStore in tests.
 *
 * @pre @p key must name an integer value in @p store.
 * @post Returns the stored integer value.
 */
int64_t requireStoreInt(PipelineValueStore const & store, std::string const & key) {
    auto value = store.getInt(key);
    if (!value) {
        throw std::runtime_error("Missing integer value in PipelineValueStore: " + key);
    }
    return *value;
}

/**
 * @brief Get a required JSON value from a PipelineValueStore in tests.
 *
 * @pre @p key must name a JSON-serializable value in @p store.
 * @post Returns the stored JSON string.
 */
std::string requireStoreJson(PipelineValueStore const & store, std::string const & key) {
    auto value = store.getJson(key);
    if (!value) {
        throw std::runtime_error("Missing JSON value in PipelineValueStore: " + key);
    }
    return *value;
}

/**
 * @brief Build a pipeline that normalizes ClockTicksWithId event times via store bindings.
 */
TransformPipeline makeNormalizeClockTicksPipeline() {
    TransformPipeline pipeline;
    auto step = PipelineStep("NormalizeClockTicksValueV2", NormalizeTimeParamsV2{});
    step.param_bindings = {{"alignment_time", "alignment_time"}};
    pipeline.addStep(step);
    return pipeline;
}

}// anonymous namespace

// =============================================================================
// buildGatherRowStore Tests
// =============================================================================

TEST_CASE("GatherResult extension - buildGatherRowStore", "[GatherResult][ValueStore][Phase5_5]") {
    // Create events spread across time
    auto events = createEventSeries({5, 15, 25, 35, 45, 55, 65, 75});

    // Create 3 trials with different intervals
    auto intervals = createIntervalSeries({
            {0, 20}, // Trial 0: events 5, 15
            {30, 50},// Trial 1: events 35, 45
            {60, 80} // Trial 2: events 65, 75
    });

    auto result = gather(events, intervals);
    REQUIRE(result.size() == 3);

    SECTION("Trial 0 store values") {
        auto store = buildGatherRowStore(result, 0);

        REQUIRE(store.contains("alignment_time"));
        REQUIRE(store.contains("trial_index"));
        REQUIRE(store.contains("trial_duration"));
        REQUIRE(store.contains("end_time"));

        CHECK(requireStoreInt(store, "alignment_time") == 0);
        CHECK(requireStoreInt(store, "trial_index") == 0);
        CHECK(requireStoreInt(store, "trial_duration") == 20);
        CHECK(requireStoreInt(store, "end_time") == 20);
    }

    SECTION("Trial 1 store values") {
        auto store = buildGatherRowStore(result, 1);

        CHECK(requireStoreInt(store, "alignment_time") == 30);
        CHECK(requireStoreInt(store, "trial_index") == 1);
        CHECK(requireStoreInt(store, "trial_duration") == 20);
        CHECK(requireStoreInt(store, "end_time") == 50);
    }

    SECTION("Trial 2 store values") {
        auto store = buildGatherRowStore(result, 2);

        CHECK(requireStoreInt(store, "alignment_time") == 60);
        CHECK(requireStoreInt(store, "trial_index") == 2);
        CHECK(requireStoreInt(store, "trial_duration") == 20);
        CHECK(requireStoreInt(store, "end_time") == 80);
    }

    SECTION("Out of range throws") {
        CHECK_THROWS_AS(buildGatherRowStore(result, 3), std::out_of_range);
        CHECK_THROWS_AS(buildGatherRowStore(result, 100), std::out_of_range);
    }

    SECTION("Store values are correct type for JSON binding") {
        auto store = buildGatherRowStore(result, 0);

        // Verify JSON representation is correct for binding
        CHECK(requireStoreJson(store, "alignment_time") == "0");
    }
}

// =============================================================================
// buildGatherRowStore with Reordering Tests
// =============================================================================

TEST_CASE("GatherResult extension - buildGatherRowStore with reordering",
          "[GatherResult][ValueStore][Phase5_5]") {
    // Create events
    auto events = createEventSeries({5, 15, 35, 45, 65});
    auto intervals = createIntervalSeries({
            {0, 20}, // Trial 0: events 5, 15
            {30, 50},// Trial 1: events 35, 45
            {60, 80} // Trial 2: event 65
    });

    auto result = gather(events, intervals);

    // Reorder: [2, 0, 1] (trial 2 first, then 0, then 1)
    auto reordered = result.reorder({2, 0, 1});

    SECTION("Reordered position 0 (original trial 2)") {
        auto store = buildGatherRowStore(reordered, 0);

        // Should have trial 2's values
        CHECK(requireStoreInt(store, "alignment_time") == 60);
        CHECK(requireStoreInt(store, "trial_index") == 2);// Original index
    }

    SECTION("Reordered position 1 (original trial 0)") {
        auto store = buildGatherRowStore(reordered, 1);

        // Should have trial 0's values
        CHECK(requireStoreInt(store, "alignment_time") == 0);
        CHECK(requireStoreInt(store, "trial_index") == 0);// Original index
    }

    SECTION("Reordered position 2 (original trial 1)") {
        auto store = buildGatherRowStore(reordered, 2);

        // Should have trial 1's values
        CHECK(requireStoreInt(store, "alignment_time") == 30);
        CHECK(requireStoreInt(store, "trial_index") == 1);// Original index
    }
}

// =============================================================================
// NormalizeTimeParamsV2 Tests
// =============================================================================

TEST_CASE("NormalizeTimeParamsV2 - basic functionality", "[NormalizeTimeParamsV2][ValueStore][Phase3]") {
    SECTION("Default initialization") {
        NormalizeTimeParamsV2 const params{};
        CHECK(params.alignment_time == 0);
    }

    SECTION("Designated initialization") {
        NormalizeTimeParamsV2 const params{.alignment_time = 100};
        CHECK(params.alignment_time == 100);
    }

    SECTION("Transform with params") {
        NormalizeTimeParamsV2 const params{.alignment_time = 100};

        TimeFrameIndex const event_time{125};
        float norm_time = normalizeTimeValueV2(event_time, params);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Clock-tick transform with params") {
        NormalizeTimeParamsV2 const params{.alignment_time = 50};

        ClockTicks const time{75};
        float norm_time = normalizeClockTicksValueV2(time, params);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Clock-tick event transform with params") {
        NormalizeTimeParamsV2 const params{.alignment_time = 50};

        ClockTicksWithId const event{ClockTicks{75}, EntityId{1}};
        float norm_time = normalizeClockTicksWithIdValueV2(event, params);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }
}

// =============================================================================
// Parameter Binding Tests
// =============================================================================

TEST_CASE("NormalizeTimeParamsV2 - parameter binding", "[NormalizeTimeParamsV2][ValueStore][Phase3]") {
    V2TestFixture const fixture;

    SECTION("Apply bindings from store") {
        PipelineValueStore store;
        store.set("alignment_time", int64_t{100});

        NormalizeTimeParamsV2 const base_params{.alignment_time = 0};
        std::map<std::string, std::string> const bindings = {
                {"alignment_time", "alignment_time"}};

        auto bound_params = applyBindings(base_params, bindings, store);
        CHECK(bound_params.alignment_time == 100);
    }

    SECTION("Bindings override default values") {
        PipelineValueStore store;
        store.set("trial_alignment", int64_t{500});

        NormalizeTimeParamsV2 const base_params{.alignment_time = 100};
        std::map<std::string, std::string> const bindings = {
                {"alignment_time", "trial_alignment"}// Different store key
        };

        auto bound_params = applyBindings(base_params, bindings, store);
        CHECK(bound_params.alignment_time == 500);
    }

    SECTION("Missing store key throws") {
        PipelineValueStore const store;
        // Don't set any values

        NormalizeTimeParamsV2 const base_params{};
        std::map<std::string, std::string> const bindings = {
                {"alignment_time", "nonexistent_key"}};

        CHECK_THROWS_AS(
                applyBindings(base_params, bindings, store),
                std::runtime_error);
    }
}

// =============================================================================
// bindValueProjectionV2 Tests
// =============================================================================

TEST_CASE("bindValueProjectionV2 - basic usage", "[TransformPipeline][ValueStore][Phase3]") {
    V2TestFixture const fixture;

    SECTION("Create factory from pipeline (legacy EventWithId path)") {
        // Build pipeline with param bindings
        TransformPipeline pipeline;

        // Add step with param bindings set in the step
        // Use NormalizeTimeValueV2 since bindValueProjectionV2 extracts .time() from EventWithId
        auto step = PipelineStep("NormalizeTimeValueV2", NormalizeTimeParamsV2{});
        step.param_bindings = {{"alignment_time", "alignment_time"}};
        pipeline.addStep(step);

        auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);

        // Create store with alignment time
        PipelineValueStore store;
        store.set("alignment_time", int64_t{100});

        // Get projection from factory
        auto projection = factory(store);

        // Test projection
        EventWithId const event{TimeFrameIndex{125}, EntityId{1}};
        float norm_time = projection(event);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Create factory from pipeline (ClockTicksWithId gather path)") {
        auto pipeline = makeNormalizeClockTicksPipeline();
        auto factory = bindValueProjectionV2<ClockTicksWithId, float>(pipeline);

        PipelineValueStore store;
        store.set("alignment_time", int64_t{100});

        auto projection = factory(store);

        ClockTicksWithId const event{ClockTicks{125}, EntityId{1}};
        float norm_time = projection(event);
        CHECK_THAT(norm_time, WithinAbs(25.0f, 0.001f));
    }

    SECTION("Factory produces different projections for different stores") {
        TransformPipeline pipeline;
        // Use NormalizeTimeValueV2 since bindValueProjectionV2 extracts .time() from EventWithId
        auto step = PipelineStep("NormalizeTimeValueV2", NormalizeTimeParamsV2{});
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
        EventWithId const event{TimeFrameIndex{150}, EntityId{1}};
        CHECK_THAT(proj1(event), WithinAbs(150.0f, 0.001f));// 150 - 0
        CHECK_THAT(proj2(event), WithinAbs(50.0f, 0.001f)); // 150 - 100
    }

    SECTION("Empty pipeline throws") {
        TransformPipeline const pipeline;// Empty
        CHECK_THROWS_AS(
                (bindValueProjectionV2<TimeFrameIndex, float>(pipeline)),
                std::runtime_error);
    }
}

// =============================================================================
// project Tests
// =============================================================================

TEST_CASE("GatherResult - project", "[GatherResult][ValueStore][Phase3]") {
    V2TestFixture const fixture;

    // Create events
    auto events = createEventSeries({5, 15, 35, 45, 65, 75});
    auto intervals = createIntervalSeries({
            {0, 20}, // Trial 0: events 5, 15
            {30, 50},// Trial 1: events 35, 45
            {60, 80} // Trial 2: events 65, 75
    });

    auto result = gather(events, intervals);

    SECTION("project creates per-trial projections") {
        // Create pipeline with bindings for gathered clock-tick events
        auto pipeline = makeNormalizeClockTicksPipeline();

        auto factory = bindValueProjectionV2<ClockTicksWithId, float>(pipeline);
        auto projections = projectGatherRows(result, factory);

        REQUIRE(projections.size() == 3);

        // Test trial 0 projection (alignment = 0)
        std::vector<float> trial0_values;
        for (auto const & event: result[0]->view()) {
            trial0_values.push_back(projections[0](event));
        }
        REQUIRE(trial0_values.size() == 2);
        CHECK_THAT(trial0_values[0], WithinAbs(5.0f, 0.001f)); // 5 - 0
        CHECK_THAT(trial0_values[1], WithinAbs(15.0f, 0.001f));// 15 - 0

        // Test trial 1 projection (alignment = 30)
        std::vector<float> trial1_values;
        for (auto const & event: result[1]->view()) {
            trial1_values.push_back(projections[1](event));
        }
        REQUIRE(trial1_values.size() == 2);
        CHECK_THAT(trial1_values[0], WithinAbs(5.0f, 0.001f)); // 35 - 30
        CHECK_THAT(trial1_values[1], WithinAbs(15.0f, 0.001f));// 45 - 30

        // Test trial 2 projection (alignment = 60)
        std::vector<float> trial2_values;
        for (auto const & event: result[2]->view()) {
            trial2_values.push_back(projections[2](event));
        }
        REQUIRE(trial2_values.size() == 2);
        CHECK_THAT(trial2_values[0], WithinAbs(5.0f, 0.001f)); // 65 - 60
        CHECK_THAT(trial2_values[1], WithinAbs(15.0f, 0.001f));// 75 - 60
    }
}

TEST_CASE("GatherResult extension - reduce and sort rows",
          "[GatherResult][ValueStore][Phase5_5]") {
    auto events = createEventSeries({5, 15, 35, 45, 65});
    auto intervals = createIntervalSeries({{0, 20},
                                           {30, 50},
                                           {70, 80}});

    auto result = gather(events, intervals);

    ReducerFactoryV2<ClockTicksWithId, float> const first_latency_factory =
            [](PipelineValueStore const & store) -> ReducerFn<ClockTicksWithId, float> {
        auto const alignment = store.getInt("alignment_time").value();
        return [alignment](std::span<ClockTicksWithId const> events) -> float {
            if (events.empty()) {
                return NAN;
            }
            return static_cast<float>(events.front().time().getValue() - alignment);
        };
    };

    auto const latencies = reduceGatherRows(result, first_latency_factory);
    REQUIRE(latencies.size() == 3);
    CHECK_THAT(latencies[0], WithinAbs(5.0f, 0.001f));
    CHECK_THAT(latencies[1], WithinAbs(5.0f, 0.001f));
    CHECK(std::isnan(latencies[2]));

    auto const sort_order = sortGatherRowsBy(result, first_latency_factory);
    REQUIRE(sort_order.size() == 3);
    CHECK(sort_order[0] == 0);
    CHECK(sort_order[1] == 1);
    CHECK(sort_order[2] == 2);
}

TEST_CASE("GatherResult extension - transform rows preserves metadata",
          "[GatherResult][ValueStore][Phase5_5]") {
    V2TestFixture const fixture;

    auto spikes = createEventSeries({5, 15, 105, 115});
    auto alignment_events = createEventSeries({10, 110});
    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto raster = gather(spikes, windows, alignment_events);

    TransformPipeline pipeline;
    pipeline.addStep(
            "EventToInterval",
            EventToIntervalParams{
                    .pre_expansion = TimeFrameIndex{0},
                    .post_expansion = TimeFrameIndex{0}});

    auto transformed = transformGatherRows<DigitalEventSeries, DigitalIntervalSeries>(
            raster,
            pipeline);

    REQUIRE(transformed.size() == raster.size());
    CHECK(transformed.source() == nullptr);
    CHECK(transformed.windows() == raster.windows());
    CHECK(transformed.alignmentPoints() == raster.alignmentPoints());
    CHECK(transformed.alignmentTimeAt(0) == raster.alignmentTimeAt(0));
    CHECK(transformed.alignmentTimeAt(1) == raster.alignmentTimeAt(1));
    CHECK(transformed[0]->size() == raster[0]->size());
    CHECK(transformed[1]->size() == raster[1]->size());
}

// =============================================================================
// Integration: Full Raster Plot Workflow with V2 Pattern
// =============================================================================

TEST_CASE("GatherResult - V2 raster plot workflow", "[GatherResult][ValueStore][Integration][Phase3]") {
    V2TestFixture const fixture;

    // Setup: Create spike events at various times
    auto spikes = createEventSeries({
            10, 25, 40,  // Trial 0 events
            110, 130,    // Trial 1 events
            215, 220, 230// Trial 2 events
    });

    // Create trial intervals
    auto trials = createIntervalSeries({
            {0, 50},   // Trial 0
            {100, 150},// Trial 1
            {200, 250} // Trial 2
    });

    auto raster = gather(spikes, trials);
    REQUIRE(raster.size() == 3);

    // Build pipeline for time normalization using V2 pattern (clock-tick gathered events)
    auto pipeline = makeNormalizeClockTicksPipeline();

    auto factory = bindValueProjectionV2<ClockTicksWithId, float>(pipeline);
    auto projections = projectGatherRows(raster, factory);

    SECTION("Verify normalized times for raster plot") {
        // Trial 0: alignment = 0
        std::vector<float> trial0_times;
        for (auto const & event: raster[0]->view()) {
            trial0_times.push_back(projections[0](event));
        }
        REQUIRE(trial0_times.size() == 3);
        CHECK_THAT(trial0_times[0], WithinAbs(10.0f, 0.001f));// 10 - 0
        CHECK_THAT(trial0_times[1], WithinAbs(25.0f, 0.001f));// 25 - 0
        CHECK_THAT(trial0_times[2], WithinAbs(40.0f, 0.001f));// 40 - 0

        // Trial 1: alignment = 100
        std::vector<float> trial1_times;
        for (auto const & event: raster[1]->view()) {
            trial1_times.push_back(projections[1](event));
        }
        REQUIRE(trial1_times.size() == 2);
        CHECK_THAT(trial1_times[0], WithinAbs(10.0f, 0.001f));// 110 - 100
        CHECK_THAT(trial1_times[1], WithinAbs(30.0f, 0.001f));// 130 - 100

        // Trial 2: alignment = 200
        std::vector<float> trial2_times;
        for (auto const & event: raster[2]->view()) {
            trial2_times.push_back(projections[2](event));
        }
        REQUIRE(trial2_times.size() == 3);
        CHECK_THAT(trial2_times[0], WithinAbs(15.0f, 0.001f));// 215 - 200
        CHECK_THAT(trial2_times[1], WithinAbs(20.0f, 0.001f));// 220 - 200
        CHECK_THAT(trial2_times[2], WithinAbs(30.0f, 0.001f));// 230 - 200
    }

    SECTION("Verify EntityId access preserved") {
        // The key benefit: we can still access EntityId from source elements
        for (size_t trial = 0; trial < raster.size(); ++trial) {
            for (auto const & event: raster[trial]->view()) {
                float const norm_time = projections[trial](event);
                EntityId const id = event.id();// Still accessible!

                // Both values should be valid
                CHECK(norm_time >= 0.0f);
                // EntityId is valid (even if uninitialized in test data)
            }
        }
    }
}

// =============================================================================
// Prepared Window Metadata Tests
// =============================================================================

TEST_CASE("Prepared event windows - basic functionality", "[GatherResult][Phase6]") {
    // Create alignment events (e.g., stimulus times)
    auto alignment_events = createEventSeries({100, 200, 300});

    // Create source data (e.g., spikes)
    auto spikes = createEventSeries({90, 110, 120, 195, 210, 290, 310, 320});

    SECTION("Expand events to symmetric windows") {
        // Each alignment event becomes a ±50 window
        auto windows = createWindowsAroundEvents(alignment_events, 50, 50);
        auto result = gather(spikes, windows, alignment_events);

        REQUIRE(result.size() == 3);

        // Trial 0: alignment at 100, window [50, 150]
        // Spikes in this window: 90, 110, 120
        REQUIRE(result[0]->size() == 3);

        // Trial 1: alignment at 200, window [150, 250]
        // Spikes in this window: 195, 210
        REQUIRE(result[1]->size() == 2);

        // Trial 2: alignment at 300, window [250, 350]
        // Spikes in this window: 290, 310, 320
        REQUIRE(result[2]->size() == 3);
    }

    SECTION("Alignment time is event time, not interval start") {
        auto windows = createWindowsAroundEvents(alignment_events, 50, 50);
        auto result = gather(spikes, windows, alignment_events);

        // Check that buildGatherRowStore uses the event time (alignment) not interval start
        auto store0 = buildGatherRowStore(result, 0);
        auto store1 = buildGatherRowStore(result, 1);
        auto store2 = buildGatherRowStore(result, 2);

        // Alignment time should be the event time, not the window start
        CHECK(requireStoreInt(store0, "alignment_time") == 100);// Event time, not 50
        CHECK(requireStoreInt(store1, "alignment_time") == 200);// Event time, not 150
        CHECK(requireStoreInt(store2, "alignment_time") == 300);// Event time, not 250
    }

    SECTION("Asymmetric windows") {
        // 25 before, 75 after
        auto windows = createWindowsAroundEvents(alignment_events, 25, 75);
        auto result = gather(spikes, windows, alignment_events);

        REQUIRE(result.size() == 3);

        // Trial 0: window [75, 175]
        // Spikes: 90, 110, 120
        REQUIRE(result[0]->size() == 3);
    }
}

TEST_CASE("Prepared interval alignment events - alignment options", "[GatherResult][Phase6]") {
    auto intervals = createIntervalSeries({
            {0, 100},  // Duration 100
            {150, 250},// Duration 100
            {300, 500} // Duration 200
    });

    auto spikes = createEventSeries({10, 50, 90, 160, 200, 240, 350, 400, 450});

    SECTION("Default alignment is start") {
        auto alignment_events = createAlignmentEventsForIntervals(
                intervals,
                TestIntervalAlignmentPoint::Start);
        auto result = gather(spikes, intervals, alignment_events);

        auto store0 = buildGatherRowStore(result, 0);
        auto store1 = buildGatherRowStore(result, 1);
        auto store2 = buildGatherRowStore(result, 2);

        CHECK(requireStoreInt(store0, "alignment_time") == 0);
        CHECK(requireStoreInt(store1, "alignment_time") == 150);
        CHECK(requireStoreInt(store2, "alignment_time") == 300);
    }

    SECTION("End alignment") {
        auto alignment_events = createAlignmentEventsForIntervals(
                intervals,
                TestIntervalAlignmentPoint::End);
        auto result = gather(spikes, intervals, alignment_events);

        auto store0 = buildGatherRowStore(result, 0);
        auto store1 = buildGatherRowStore(result, 1);
        auto store2 = buildGatherRowStore(result, 2);

        CHECK(requireStoreInt(store0, "alignment_time") == 100);
        CHECK(requireStoreInt(store1, "alignment_time") == 250);
        CHECK(requireStoreInt(store2, "alignment_time") == 500);
    }

    SECTION("Center alignment") {
        auto alignment_events = createAlignmentEventsForIntervals(
                intervals,
                TestIntervalAlignmentPoint::Center);
        auto result = gather(spikes, intervals, alignment_events);

        auto store0 = buildGatherRowStore(result, 0);
        auto store1 = buildGatherRowStore(result, 1);
        auto store2 = buildGatherRowStore(result, 2);

        CHECK(requireStoreInt(store0, "alignment_time") == 50); // (0 + 100) / 2
        CHECK(requireStoreInt(store1, "alignment_time") == 200);// (150 + 250) / 2
        CHECK(requireStoreInt(store2, "alignment_time") == 400);// (300 + 500) / 2
    }
}

TEST_CASE("Prepared event windows with time normalization", "[GatherResult][ValueStore][Phase6]") {
    V2TestFixture const fixture;

    // Create alignment events at 100, 200, 300
    auto alignment_events = createEventSeries({100, 200, 300});

    // Create spikes relative to each event
    auto spikes = createEventSeries({
            80, 100, 120, // Around event at 100
            180, 200, 220,// Around event at 200
            280, 300, 320 // Around event at 300
    });

    // Create prepared ±50 windows with companion alignment events
    auto windows = createWindowsAroundEvents(alignment_events, 50, 50);
    auto raster = gather(spikes, windows, alignment_events);

    REQUIRE(raster.size() == 3);

    // Build pipeline for normalization (clock-tick gathered events)
    auto pipeline = makeNormalizeClockTicksPipeline();

    auto factory = bindValueProjectionV2<ClockTicksWithId, float>(pipeline);
    auto projections = projectGatherRows(raster, factory);

    SECTION("Each trial uses correct alignment time") {
        // Trial 0: alignment = 100
        std::vector<float> trial0_times;
        for (auto const & event: raster[0]->view()) {
            trial0_times.push_back(projections[0](event));
        }
        REQUIRE(trial0_times.size() == 3);
        CHECK_THAT(trial0_times[0], WithinAbs(-20.0f, 0.001f));// 80 - 100
        CHECK_THAT(trial0_times[1], WithinAbs(0.0f, 0.001f));  // 100 - 100
        CHECK_THAT(trial0_times[2], WithinAbs(20.0f, 0.001f)); // 120 - 100

        // Trial 1: alignment = 200
        std::vector<float> trial1_times;
        for (auto const & event: raster[1]->view()) {
            trial1_times.push_back(projections[1](event));
        }
        REQUIRE(trial1_times.size() == 3);
        CHECK_THAT(trial1_times[0], WithinAbs(-20.0f, 0.001f));// 180 - 200
        CHECK_THAT(trial1_times[1], WithinAbs(0.0f, 0.001f));  // 200 - 200
        CHECK_THAT(trial1_times[2], WithinAbs(20.0f, 0.001f)); // 220 - 200

        // Trial 2: alignment = 300
        std::vector<float> trial2_times;
        for (auto const & event: raster[2]->view()) {
            trial2_times.push_back(projections[2](event));
        }
        REQUIRE(trial2_times.size() == 3);
        CHECK_THAT(trial2_times[0], WithinAbs(-20.0f, 0.001f));// 280 - 300
        CHECK_THAT(trial2_times[1], WithinAbs(0.0f, 0.001f));  // 300 - 300
        CHECK_THAT(trial2_times[2], WithinAbs(20.0f, 0.001f)); // 320 - 300
    }
}
