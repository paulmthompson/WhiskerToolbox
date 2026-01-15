/**
 * @file GatherResult_Pipeline.test.cpp
 * @brief Tests for GatherResult pipeline integration (Step 5.4-5.8)
 *
 * These tests verify the GatherResult methods for pipeline integration:
 * - buildContext() - produces correct TrialContext
 * - project() - applies value projection factory to all trials
 * - reduce() - applies reducer factory to all trials
 * - sortIndicesBy() - sorts trials by reduction result
 * - reorder() - creates reordered GatherResult
 *
 * @see GATHER_PIPELINE_DESIGN.md Phase 5 for design details
 */

#include "utils/GatherResult.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"
#include "transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <numeric>
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
struct PipelineFixture {
    PipelineFixture() {
        Temporal::registerTemporalTransforms();
        RangeReductions::registerAllRangeReductions();
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
// element_type Tests
// =============================================================================

TEST_CASE("GatherResult - element_type alias", "[GatherResult][Pipeline]") {
    SECTION("DigitalEventSeries element_type is EventWithId") {
        using ElementType = GatherResult<DigitalEventSeries>::element_type;
        static_assert(std::is_same_v<ElementType, EventWithId>);
    }

    SECTION("AnalogTimeSeries element_type is TimeValuePoint") {
        using ElementType = GatherResult<AnalogTimeSeries>::element_type;
        static_assert(std::is_same_v<ElementType, AnalogTimeSeries::TimeValuePoint>);
    }

    SECTION("DigitalIntervalSeries element_type is IntervalWithId") {
        using ElementType = GatherResult<DigitalIntervalSeries>::element_type;
        static_assert(std::is_same_v<ElementType, IntervalWithId>);
    }
}

// =============================================================================
// buildContext Tests
// =============================================================================

TEST_CASE("GatherResult - buildContext", "[GatherResult][Pipeline]") {
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

    SECTION("Trial 0 context") {
        auto ctx = result.buildContext(0);
        CHECK(ctx.alignment_time == TimeFrameIndex{0});
        CHECK(ctx.trial_index.has_value());
        CHECK(ctx.trial_index.value() == 0);
        CHECK(ctx.trial_duration.has_value());
        CHECK(ctx.trial_duration.value() == 20);
        CHECK(ctx.end_time.has_value());
        CHECK(ctx.end_time.value() == TimeFrameIndex{20});
    }

    SECTION("Trial 1 context") {
        auto ctx = result.buildContext(1);
        CHECK(ctx.alignment_time == TimeFrameIndex{30});
        CHECK(ctx.trial_index.value() == 1);
        CHECK(ctx.trial_duration.value() == 20);
        CHECK(ctx.end_time.value() == TimeFrameIndex{50});
    }

    SECTION("Trial 2 context") {
        auto ctx = result.buildContext(2);
        CHECK(ctx.alignment_time == TimeFrameIndex{60});
        CHECK(ctx.trial_index.value() == 2);
        CHECK(ctx.trial_duration.value() == 20);
        CHECK(ctx.end_time.value() == TimeFrameIndex{80});
    }

    SECTION("Out of range throws") {
        CHECK_THROWS_AS(result.buildContext(3), std::out_of_range);
        CHECK_THROWS_AS(result.buildContext(100), std::out_of_range);
    }
}

// =============================================================================
// project Tests
// =============================================================================

TEST_CASE("GatherResult - project", "[GatherResult][Pipeline]") {
    PipelineFixture fixture;

    // Create events
    auto events = createEventSeries({5, 15, 35, 45, 65, 75});
    auto intervals = createIntervalSeries({
        {0, 20},    // Trial 0
        {30, 50},   // Trial 1
        {60, 80}    // Trial 2
    });

    auto result = gather(events, intervals);

    SECTION("project() creates per-trial projections") {
        // Create a simple projection factory that normalizes time
        ValueProjectionFactory<EventWithId, float> factory = 
            [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
                TimeFrameIndex alignment = ctx.alignment_time;
                return [alignment](EventWithId const& event) -> float {
                    return static_cast<float>(event.time().getValue() - alignment.getValue());
                };
            };

        auto projections = result.project(factory);
        REQUIRE(projections.size() == 3);

        // Each projection should normalize to its trial's alignment
        // Trial 0: alignment=0, events at 5,15 → normalized 5,15
        // Trial 1: alignment=30, events at 35,45 → normalized 5,15
        // Trial 2: alignment=60, events at 65,75 → normalized 5,15

        // Test trial 0 projection
        std::vector<float> trial0_values;
        for (auto const& event : result[0]->view()) {
            trial0_values.push_back(projections[0](event));
        }
        REQUIRE(trial0_values.size() == 2);
        CHECK_THAT(trial0_values[0], WithinAbs(5.0f, 0.001f));
        CHECK_THAT(trial0_values[1], WithinAbs(15.0f, 0.001f));

        // Test trial 1 projection
        std::vector<float> trial1_values;
        for (auto const& event : result[1]->view()) {
            trial1_values.push_back(projections[1](event));
        }
        REQUIRE(trial1_values.size() == 2);
        CHECK_THAT(trial1_values[0], WithinAbs(5.0f, 0.001f));   // 35 - 30 = 5
        CHECK_THAT(trial1_values[1], WithinAbs(15.0f, 0.001f));  // 45 - 30 = 15
    }
}

// =============================================================================
// reduce Tests
// =============================================================================

TEST_CASE("GatherResult - reduce", "[GatherResult][Pipeline]") {
    PipelineFixture fixture;

    // Create events with different counts per trial
    auto events = createEventSeries({5, 15, 25, 35, 45, 65});
    auto intervals = createIntervalSeries({
        {0, 30},    // Trial 0: 3 events (5, 15, 25)
        {30, 50},   // Trial 1: 2 events (35, 45)
        {60, 80}    // Trial 2: 1 event (65)
    });

    auto result = gather(events, intervals);

    SECTION("reduce() with event count") {
        // Simple reducer that counts events
        ReducerFactory<EventWithId, int> count_factory =
            [](TrialContext const&) -> ReducerFn<EventWithId, int> {
                return [](std::span<EventWithId const> events) -> int {
                    return static_cast<int>(events.size());
                };
            };

        auto counts = result.reduce(count_factory);
        REQUIRE(counts.size() == 3);
        CHECK(counts[0] == 3);
        CHECK(counts[1] == 2);
        CHECK(counts[2] == 1);
    }

    SECTION("reduce() with first event latency") {
        // Reducer that returns first event time relative to alignment
        ReducerFactory<EventWithId, float> latency_factory =
            [](TrialContext const& ctx) -> ReducerFn<EventWithId, float> {
                TimeFrameIndex alignment = ctx.alignment_time;
                return [alignment](std::span<EventWithId const> events) -> float {
                    if (events.empty()) {
                        return std::numeric_limits<float>::quiet_NaN();
                    }
                    return static_cast<float>(events[0].time().getValue() - alignment.getValue());
                };
            };

        auto latencies = result.reduce(latency_factory);
        REQUIRE(latencies.size() == 3);
        CHECK_THAT(latencies[0], WithinAbs(5.0f, 0.001f));   // First event at 5, alignment 0
        CHECK_THAT(latencies[1], WithinAbs(5.0f, 0.001f));   // First event at 35, alignment 30
        CHECK_THAT(latencies[2], WithinAbs(5.0f, 0.001f));   // First event at 65, alignment 60
    }

    SECTION("reduce() handles empty trials") {
        auto sparse_events = createEventSeries({15, 45});
        auto sparse_intervals = createIntervalSeries({
            {0, 10},    // Trial 0: no events
            {10, 20},   // Trial 1: 1 event (15)
            {40, 50}    // Trial 2: 1 event (45)
        });

        auto sparse_result = gather(sparse_events, sparse_intervals);

        ReducerFactory<EventWithId, int> count_factory =
            [](TrialContext const&) -> ReducerFn<EventWithId, int> {
                return [](std::span<EventWithId const> events) -> int {
                    return static_cast<int>(events.size());
                };
            };

        auto counts = sparse_result.reduce(count_factory);
        REQUIRE(counts.size() == 3);
        CHECK(counts[0] == 0);  // Empty trial
        CHECK(counts[1] == 1);
        CHECK(counts[2] == 1);
    }
}

// =============================================================================
// sortIndicesBy Tests
// =============================================================================

TEST_CASE("GatherResult - sortIndicesBy", "[GatherResult][Pipeline]") {
    // Create events with different counts per trial
    auto events = createEventSeries({5, 15, 25, 35, 45, 65});
    auto intervals = createIntervalSeries({
        {0, 30},    // Trial 0: 3 events
        {30, 50},   // Trial 1: 2 events
        {60, 80}    // Trial 2: 1 event
    });

    auto result = gather(events, intervals);

    ReducerFactory<EventWithId, int> count_factory =
        [](TrialContext const&) -> ReducerFn<EventWithId, int> {
            return [](std::span<EventWithId const> events) -> int {
                return static_cast<int>(events.size());
            };
        };

    SECTION("sortIndicesBy ascending") {
        auto indices = result.sortIndicesBy(count_factory, true);
        REQUIRE(indices.size() == 3);
        // Ascending: 1 event (trial 2) < 2 events (trial 1) < 3 events (trial 0)
        CHECK(indices[0] == 2);  // Trial with 1 event
        CHECK(indices[1] == 1);  // Trial with 2 events
        CHECK(indices[2] == 0);  // Trial with 3 events
    }

    SECTION("sortIndicesBy descending") {
        auto indices = result.sortIndicesBy(count_factory, false);
        REQUIRE(indices.size() == 3);
        // Descending: 3 events (trial 0) > 2 events (trial 1) > 1 event (trial 2)
        CHECK(indices[0] == 0);  // Trial with 3 events
        CHECK(indices[1] == 1);  // Trial with 2 events
        CHECK(indices[2] == 2);  // Trial with 1 event
    }

    SECTION("sortIndicesBy handles NaN values") {
        // Create reducer that returns NaN for empty trials
        auto sparse_events = createEventSeries({15, 45});
        auto sparse_intervals = createIntervalSeries({
            {0, 10},    // Trial 0: no events → NaN
            {10, 20},   // Trial 1: 1 event
            {40, 50}    // Trial 2: 1 event
        });

        auto sparse_result = gather(sparse_events, sparse_intervals);

        ReducerFactory<EventWithId, float> latency_factory =
            [](TrialContext const& ctx) -> ReducerFn<EventWithId, float> {
                TimeFrameIndex alignment = ctx.alignment_time;
                return [alignment](std::span<EventWithId const> events) -> float {
                    if (events.empty()) {
                        return std::numeric_limits<float>::quiet_NaN();
                    }
                    return static_cast<float>(events[0].time().getValue() - alignment.getValue());
                };
            };

        auto indices = sparse_result.sortIndicesBy(latency_factory, true);
        REQUIRE(indices.size() == 3);
        // NaN should sort to end
        CHECK(indices[2] == 0);  // Trial with NaN at the end
    }
}

// =============================================================================
// reorder Tests
// =============================================================================

TEST_CASE("GatherResult - reorder", "[GatherResult][Pipeline]") {
    auto events = createEventSeries({5, 15, 25, 35, 45, 65});
    auto intervals = createIntervalSeries({
        {0, 30},    // Trial 0: 3 events
        {30, 50},   // Trial 1: 2 events
        {60, 80}    // Trial 2: 1 event
    });

    auto result = gather(events, intervals);

    SECTION("reorder() creates correctly ordered result") {
        std::vector<GatherResult<DigitalEventSeries>::size_type> new_order = {2, 0, 1};
        auto reordered = result.reorder(new_order);

        REQUIRE(reordered.size() == 3);
        
        // Check that views are in new order
        CHECK(reordered[0]->size() == 1);  // Was trial 2
        CHECK(reordered[1]->size() == 3);  // Was trial 0
        CHECK(reordered[2]->size() == 2);  // Was trial 1
    }

    SECTION("reorder() tracks original indices") {
        std::vector<GatherResult<DigitalEventSeries>::size_type> new_order = {2, 0, 1};
        auto reordered = result.reorder(new_order);

        CHECK(reordered.isReordered());
        CHECK(reordered.originalIndex(0) == 2);
        CHECK(reordered.originalIndex(1) == 0);
        CHECK(reordered.originalIndex(2) == 1);
    }

    SECTION("non-reordered result returns identity for originalIndex") {
        CHECK_FALSE(result.isReordered());
        CHECK(result.originalIndex(0) == 0);
        CHECK(result.originalIndex(1) == 1);
        CHECK(result.originalIndex(2) == 2);
    }

    SECTION("reorder() with wrong size throws") {
        std::vector<GatherResult<DigitalEventSeries>::size_type> wrong_size = {0, 1};
        CHECK_THROWS_AS(result.reorder(wrong_size), std::invalid_argument);
    }

    SECTION("reorder() with out-of-range index throws") {
        std::vector<GatherResult<DigitalEventSeries>::size_type> bad_indices = {0, 1, 5};
        CHECK_THROWS_AS(result.reorder(bad_indices), std::out_of_range);
    }

    SECTION("intervalAtReordered() returns original interval") {
        std::vector<GatherResult<DigitalEventSeries>::size_type> new_order = {2, 0, 1};
        auto reordered = result.reorder(new_order);

        // Position 0 in reordered is original trial 2 (interval [60, 80])
        auto interval = reordered.intervalAtReordered(0);
        CHECK(interval.start == 60);
        CHECK(interval.end == 80);

        // Position 1 in reordered is original trial 0 (interval [0, 30])
        interval = reordered.intervalAtReordered(1);
        CHECK(interval.start == 0);
        CHECK(interval.end == 30);
    }
}

// =============================================================================
// Full Workflow Integration Tests
// =============================================================================

TEST_CASE("GatherResult - full raster plot workflow", "[GatherResult][Pipeline][Integration]") {
    PipelineFixture fixture;

    // Create events with varying first-spike latencies
    // Trial 0 (interval [0, 100]): first spike at 20 → latency 20
    // Trial 1 (interval [100, 200]): first spike at 105 → latency 5
    // Trial 2 (interval [200, 300]): first spike at 230 → latency 30
    auto events = createEventSeries({20, 50, 105, 150, 230, 280});
    auto intervals = createIntervalSeries({
        {0, 100},    // Trial 0
        {100, 200},  // Trial 1
        {200, 300}   // Trial 2
    });

    auto result = gather(events, intervals);

    // Step 1: Create projection factory for normalized time
    ValueProjectionFactory<EventWithId, float> projection_factory =
        [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](EventWithId const& event) -> float {
                return static_cast<float>(event.time().getValue() - alignment.getValue());
            };
        };

    // Step 2: Create reducer for first-spike latency
    ReducerFactory<EventWithId, float> latency_factory =
        [](TrialContext const& ctx) -> ReducerFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](std::span<EventWithId const> events) -> float {
                if (events.empty()) {
                    return std::numeric_limits<float>::quiet_NaN();
                }
                return static_cast<float>(events[0].time().getValue() - alignment.getValue());
            };
        };

    // Step 3: Sort by latency
    auto sort_order = result.sortIndicesBy(latency_factory, true);

    // Verify sort order: Trial 1 (5) < Trial 0 (20) < Trial 2 (30)
    REQUIRE(sort_order.size() == 3);
    CHECK(sort_order[0] == 1);  // Latency 5
    CHECK(sort_order[1] == 0);  // Latency 20
    CHECK(sort_order[2] == 2);  // Latency 30

    // Step 4: Reorder result
    auto sorted_result = result.reorder(sort_order);

    // Step 5: Get projections for sorted result
    auto projections = sorted_result.project(projection_factory);

    // Step 6: Simulate drawing - collect all normalized times
    std::vector<std::vector<float>> raster_data;
    for (size_t row = 0; row < sorted_result.size(); ++row) {
        std::vector<float> row_times;
        for (auto const& event : sorted_result[row]->view()) {
            row_times.push_back(projections[row](event));
        }
        raster_data.push_back(std::move(row_times));
    }

    // Verify raster data
    // Row 0: Original trial 1, events at 105, 150 → normalized 5, 50
    REQUIRE(raster_data[0].size() == 2);
    CHECK_THAT(raster_data[0][0], WithinAbs(5.0f, 0.001f));
    CHECK_THAT(raster_data[0][1], WithinAbs(50.0f, 0.001f));

    // Row 1: Original trial 0, events at 20, 50 → normalized 20, 50
    REQUIRE(raster_data[1].size() == 2);
    CHECK_THAT(raster_data[1][0], WithinAbs(20.0f, 0.001f));
    CHECK_THAT(raster_data[1][1], WithinAbs(50.0f, 0.001f));

    // Row 2: Original trial 2, events at 230, 280 → normalized 30, 80
    REQUIRE(raster_data[2].size() == 2);
    CHECK_THAT(raster_data[2][0], WithinAbs(30.0f, 0.001f));
    CHECK_THAT(raster_data[2][1], WithinAbs(80.0f, 0.001f));
}

