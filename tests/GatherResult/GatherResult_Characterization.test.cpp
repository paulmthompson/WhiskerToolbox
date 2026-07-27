/**
 * @file GatherResult_Characterization.test.cpp
 * @brief Phase 0 characterization tests locking in pre-migration GatherResult behavior
 */

#include "GatherResult/GatherResult.hpp"
#include "GatherResult/IntervalAdapters.hpp"

#include "fixtures/GatherAlignmentFixtures.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using Neuralyzer::Gather::expandEvents;
using Neuralyzer::Test::GatherFixtures::createEventSeries;

namespace {

using Neuralyzer::Test::GatherFixtures::createIdentityTimeFrame;

}// namespace

TEST_CASE("GatherResult - overlapping event windows preserve one row per event",
          "[GatherResult][migration][phase0]") {
    auto spikes = createEventSeries({5, 10, 15, 20, 25, 30});
    auto alignment_events = createEventSeries({10, 15, 20});

    auto adapter = expandEvents(alignment_events, 10, 10);
    auto result = GatherResult<DigitalEventSeries>::create(spikes, adapter);

    REQUIRE(result.size() == 3);

    SECTION("Each event produces a distinct overlapping window") {
        CHECK(result.intervalAt(0).start == 0);
        CHECK(result.intervalAt(0).end == 20);
        CHECK(result.intervalAt(1).start == 5);
        CHECK(result.intervalAt(1).end == 25);
        CHECK(result.intervalAt(2).start == 10);
        CHECK(result.intervalAt(2).end == 30);

        CHECK(result.intervalAt(0).end > result.intervalAt(1).start);
        CHECK(result.intervalAt(1).end > result.intervalAt(2).start);
    }

    SECTION("Alignment times match original event indices") {
        CHECK(result.alignmentTimeAt(0) == 10);
        CHECK(result.alignmentTimeAt(1) == 15);
        CHECK(result.alignmentTimeAt(2) == 20);
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

    auto adapter = expandEvents(events, 5, 5);
    auto gathered = GatherResult<DigitalEventSeries>::create(events, adapter);

    REQUIRE(gathered.size() == 3);
    CHECK_FALSE(gathered.isReordered());

    int64_t const align_0 = gathered.alignmentTimeAt(0);
    int64_t const align_1 = gathered.alignmentTimeAt(1);
    int64_t const align_2 = gathered.alignmentTimeAt(2);

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

    auto adapter = expandEvents(alignment_events, 10, 10);
    auto gathered = GatherResult<DigitalEventSeries>::create(spikes, adapter);

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

    auto adapter = expandEvents(alignment_events, 50, 50);
    auto result = GatherResult<DigitalEventSeries>::create(spikes, adapter);

    REQUIRE(result.size() == 1);
    CHECK(result.intervalAt(0).start == 50);
    CHECK(result.intervalAt(0).end == 150);
    CHECK(result.alignmentTimeAt(0) == 100);
    CHECK(result.alignmentTimeAt(0) != static_cast<int64_t>(result.intervalAt(0).start));
}
