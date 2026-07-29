/**
 * @file GatherResult_Characterization.test.cpp
 * @brief Phase 0 characterization tests locking in pre-migration GatherResult behavior
 */

#include "GatherResult/GatherResult.hpp"

#include "fixtures/GatherAlignmentFixtures.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

using Neuralyzer::Test::GatherFixtures::createEventSeries;
using Neuralyzer::Test::GatherFixtures::createWindowsAroundEvents;

namespace {

using Neuralyzer::Test::GatherFixtures::createIdentityTimeFrame;
using Neuralyzer::Test::GatherFixtures::createIntervalSeries;

}// namespace

TEST_CASE("GatherResult - overlapping event windows preserve one row per event",
          "[GatherResult][migration][phase0]") {
    auto spikes = createEventSeries({5, 10, 15, 20, 25, 30});
    auto alignment_events = createEventSeries({10, 15, 20});

    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 3);

    SECTION("Each event produces a distinct overlapping window") {
        CHECK(result.intervalAt(0).start == TimeFrameIndex(0));
        CHECK(result.intervalAt(0).end == TimeFrameIndex(20));
        CHECK(result.intervalAt(1).start == TimeFrameIndex(5));
        CHECK(result.intervalAt(1).end == TimeFrameIndex(25));
        CHECK(result.intervalAt(2).start == TimeFrameIndex(10));
        CHECK(result.intervalAt(2).end == TimeFrameIndex(30));

        CHECK(result.intervalAt(0).end > result.intervalAt(1).start);
        CHECK(result.intervalAt(1).end > result.intervalAt(2).start);
    }

    SECTION("Alignment times match original event indices") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(10));
        CHECK(result.alignmentTimeAt(1) == ClockTicks(15));
        CHECK(result.alignmentTimeAt(2) == ClockTicks(20));
    }

    SECTION("Each trial gathers the expected spike subset") {
        REQUIRE(result[0]->size() == 4);// spikes 5, 10, 15, 20 in [0, 20]
        REQUIRE(result[1]->size() == 5);// spikes 5, 10, 15, 20, 25 in [5, 25]
        REQUIRE(result[2]->size() == 5);// spikes 10, 15, 20, 25, 30 in [10, 30]

        std::vector<int64_t> trial_0_times;
        for (auto const & event: result[0]->view()) {
            trial_0_times.push_back(event.time().getValue());
        }
        CHECK(trial_0_times == std::vector<int64_t>{5, 10, 15, 20});

        std::vector<int64_t> trial_1_times;
        for (auto const & event: result[1]->view()) {
            trial_1_times.push_back(event.time().getValue());
        }
        CHECK(trial_1_times == std::vector<int64_t>{5, 10, 15, 20, 25});

        std::vector<int64_t> trial_2_times;
        for (auto const & event: result[2]->view()) {
            trial_2_times.push_back(event.time().getValue());
        }
        CHECK(trial_2_times == std::vector<int64_t>{10, 15, 20, 25, 30});
    }
}

TEST_CASE("GatherResult - reorder preserves originalIndex and intervalAtReordered",
          "[GatherResult][migration][phase0]") {
    auto events = createEventSeries({10, 20, 30});
    auto tf = createIdentityTimeFrame(40);
    events->setTimeFrame(tf);

    auto windows = createWindowsAroundEvents(events, 5, 5);
    auto gathered = gather(events, windows, events);

    REQUIRE(gathered.size() == 3);
    CHECK_FALSE(gathered.isReordered());

    ClockTicks const align_0 = gathered.alignmentTimeAt(0);
    ClockTicks const align_1 = gathered.alignmentTimeAt(1);
    ClockTicks const align_2 = gathered.alignmentTimeAt(2);

    auto const interval_0 = gathered.intervalAt(0);
    auto const interval_1 = gathered.intervalAt(1);
    auto const interval_2 = gathered.intervalAt(2);

    auto reordered = gathered.reorder({2, 0, 1});
    REQUIRE(reordered.isReordered());

    CHECK(reordered.originalIndex(0) == 2);
    CHECK(reordered.originalIndex(1) == 0);
    CHECK(reordered.originalIndex(2) == 1);

    CHECK(reordered.intervalAtReordered(0) == interval_2);
    CHECK(reordered.intervalAtReordered(1) == interval_0);
    CHECK(reordered.intervalAtReordered(2) == interval_1);

    CHECK(reordered.alignmentTimeAt(0) == align_2);
    CHECK(reordered.alignmentTimeAt(1) == align_0);
    CHECK(reordered.alignmentTimeAt(2) == align_1);

    CHECK(reordered.intervalAtReordered(0) == reordered.intervalAt(reordered.originalIndex(0)));
    CHECK(reordered.intervalAtReordered(1) == reordered.intervalAt(reordered.originalIndex(1)));
    CHECK(reordered.intervalAtReordered(2) == reordered.intervalAt(reordered.originalIndex(2)));
}

TEST_CASE("GatherResult - materialize preserves intervals and alignment times",
          "[GatherResult][migration][phase0]") {
    auto spikes = createEventSeries({10, 20, 30, 40});
    auto alignment_events = createEventSeries({20});

    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto gathered = gather(spikes, windows, alignment_events);

    REQUIRE(gathered.size() == 1);
    auto const materialized = gathered.materialize();

    REQUIRE(materialized.size() == gathered.size());
    CHECK(materialized.intervalAt(0).start == gathered.intervalAt(0).start);
    CHECK(materialized.intervalAt(0).end == gathered.intervalAt(0).end);
    CHECK(materialized.alignmentTimeAt(0) == gathered.alignmentTimeAt(0));

    SECTION("Materialize preserves reorder metadata") {
        auto reordered = gathered.reorder({0});
        auto const materialized_reordered = reordered.materialize();
        CHECK(materialized_reordered.isReordered());
        CHECK(materialized_reordered.originalIndex(0) == 0);
        CHECK(materialized_reordered.alignmentTimeAt(0) == gathered.alignmentTimeAt(0));
    }

    SECTION("Materialized views are owning copies") {
        REQUIRE(gathered[0] != nullptr);
        REQUIRE(materialized[0] != nullptr);
        CHECK(gathered[0].get() != materialized[0].get());
        CHECK_FALSE(materialized[0]->isView());
    }
}

TEST_CASE("GatherResult - centered event window alignment differs from window start",
          "[GatherResult][migration][phase0]") {
    auto spikes = createEventSeries({80, 100, 120});
    auto alignment_events = createEventSeries({100});

    auto windows = createWindowsAroundEvents(alignment_events, 50, 50);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 1);
    CHECK(result.intervalAt(0).start == TimeFrameIndex(50));
    CHECK(result.intervalAt(0).end == TimeFrameIndex(150));
    CHECK(result.alignmentTimeAt(0) == ClockTicks(100));
    CHECK(result.alignmentTimeAt(0).getValue() != result.intervalAt(0).start.getValue());
}

TEST_CASE("GatherResult - prepared windows use companion alignment events",
          "[GatherResult][migration][phase3]") {
    auto spikes = createEventSeries({5, 10, 15, 20, 25, 30});
    auto windows = createIntervalSeries({{0, 20}, {5, 25}, {10, 30}});
    auto alignment_events = createEventSeries({10, 15, 20});

    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 3);
    REQUIRE(result.windows() == windows);
    REQUIRE(result.alignmentPoints() == alignment_events);

    CHECK(result.intervalAt(0) == TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(20)});
    CHECK(result.intervalAt(1) == TimeFrameInterval{TimeFrameIndex(5), TimeFrameIndex(25)});
    CHECK(result.intervalAt(2) == TimeFrameInterval{TimeFrameIndex(10), TimeFrameIndex(30)});

    CHECK(result.alignmentTimeAt(0) == ClockTicks(10));
    CHECK(result.alignmentTimeAt(1) == ClockTicks(15));
    CHECK(result.alignmentTimeAt(2) == ClockTicks(20));
    CHECK(result.alignmentTimeAt(0).getValue() != result.intervalAt(0).start.getValue());
}

TEST_CASE("GatherResult - prepared metadata survives reorder and materialize",
          "[GatherResult][migration][phase3]") {
    auto spikes = createEventSeries({5, 10, 15, 20, 25, 30});
    auto windows = createIntervalSeries({{0, 20}, {5, 25}, {10, 30}});
    auto alignment_events = createEventSeries({10, 15, 20});

    auto gathered = gather(spikes, windows, alignment_events);
    auto reordered = gathered.reorder({2, 0, 1});

    REQUIRE(reordered.size() == gathered.size());
    CHECK(reordered.windows() == windows);
    CHECK(reordered.alignmentPoints() == alignment_events);
    CHECK(reordered.originalIndex(0) == 2);
    CHECK(reordered.originalIndex(1) == 0);
    CHECK(reordered.originalIndex(2) == 1);
    CHECK(reordered.intervalAtReordered(0) == gathered.intervalAt(2));
    CHECK(reordered.intervalAtReordered(1) == gathered.intervalAt(0));
    CHECK(reordered.intervalAtReordered(2) == gathered.intervalAt(1));
    CHECK(reordered.alignmentTimeAt(0) == ClockTicks(20));
    CHECK(reordered.alignmentTimeAt(1) == ClockTicks(10));
    CHECK(reordered.alignmentTimeAt(2) == ClockTicks(15));

    auto materialized = reordered.materialize();
    REQUIRE(materialized.size() == reordered.size());
    CHECK(materialized.windows() == windows);
    CHECK(materialized.alignmentPoints() == alignment_events);
    CHECK(materialized.originalIndex(0) == 2);
    CHECK(materialized.intervalAtReordered(0) == gathered.intervalAt(2));
    CHECK(materialized.alignmentTimeAt(0) == ClockTicks(20));
}

TEST_CASE("GatherResult - prepared alignment point count must match windows",
          "[GatherResult][migration][phase3]") {
    auto spikes = createEventSeries({5, 10, 15, 20});
    auto windows = createIntervalSeries({{0, 20}, {5, 25}});
    auto alignment_events = createEventSeries({10});

    CHECK_THROWS_AS(gather(spikes, windows, alignment_events), std::invalid_argument);
}
