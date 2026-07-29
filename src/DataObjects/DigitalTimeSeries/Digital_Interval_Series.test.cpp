
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityRegistry.hpp"

TEST_CASE("DigitalIntervalSeries - interval entity identity uses start and end",
          "[DataManager][interval][entity]") {
    EntityRegistry registry;
    DigitalIntervalSeries series;
    series.setIdentityContext("test_intervals", &registry);

    std::vector<int> times;
    for (int i: std::views::iota(0, 1000)) {
        times.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(times);
    series.setTimeFrame(timeframe);

    series.addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(200)});
    series.rebuildAllEntityIds();

    auto const view = series.view();
    REQUIRE(series.size() == 1);
    EntityId const original_id = view[0].id();
    REQUIRE(original_id != EntityId{0});

    SECTION("rebuildAllEntityIds is stable for unchanged intervals") {
        series.rebuildAllEntityIds();
        REQUIRE(series.view()[0].id() == original_id);
    }

    SECTION("identity survives unrelated interval removal") {
        series.addEvent(TimeFrameInterval{TimeFrameIndex(500), TimeFrameIndex(600)});
        series.rebuildAllEntityIds();
        series.removeInterval(TimeFrameInterval{TimeFrameIndex(500), TimeFrameIndex(600)});
        series.rebuildAllEntityIds();

        REQUIRE(series.size() == 1);
        REQUIRE(series.view()[0].id() == original_id);
        REQUIRE(series.getIntervalByEntityId(original_id)->start == ClockTicks(100));
        REQUIRE(series.getIntervalByEntityId(original_id)->end == ClockTicks(200));
    }

    SECTION("registry descriptor stores interval end in local_index") {
        auto desc = registry.get(original_id);
        REQUIRE(desc.has_value());
        REQUIRE(desc->kind == EntityKind::IntervalEntity);
        REQUIRE(desc->time_value == 100);
        REQUIRE(desc->local_index == 200);
    }
}

TEST_CASE("DigitalIntervalSeries - extend preserves EntityId",
          "[DataManager][interval][entity]") {
    EntityRegistry registry;
    DigitalIntervalSeries series;
    series.setIdentityContext("test_intervals", &registry);

    std::vector<int> times;
    for (int i: std::views::iota(0, 1000)) {
        times.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(times);
    series.setTimeFrame(timeframe);

    series.addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(200)});
    series.rebuildAllEntityIds();

    EntityId const original_id = series.view()[0].id();

    series.addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(250)});

    REQUIRE(series.size() == 1);
    REQUIRE(series.view()[0].id() == original_id);
    REQUIRE(series.view()[0].value().start == TimeFrameIndex(100));
    REQUIRE(series.view()[0].value().end == TimeFrameIndex(250));

    auto desc = registry.get(original_id);
    REQUIRE(desc.has_value());
    REQUIRE(desc->time_value == 100);
    REQUIRE(desc->local_index == 250);

    auto interval = series.getIntervalByEntityId(original_id);
    REQUIRE(interval.has_value());
    REQUIRE(interval->start == ClockTicks(100));
    REQUIRE(interval->end == ClockTicks(250));
}

TEST_CASE("DigitalIntervalSeries - merge inherits earliest-start EntityId",
          "[DataManager][interval][entity]") {
    EntityRegistry registry;
    DigitalIntervalSeries series;
    series.setIdentityContext("test_intervals", &registry);

    series.addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(150)});
    series.addEvent(TimeFrameInterval{TimeFrameIndex(200), TimeFrameIndex(250)});
    series.rebuildAllEntityIds();

    EntityId const first_id = series.view()[0].id();
    EntityId const second_id = series.view()[1].id();
    REQUIRE(first_id != second_id);

    series.addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(250)});

    REQUIRE(series.size() == 1);
    REQUIRE(series.view()[0].id() == first_id);
    REQUIRE(series.view()[0].value().start == TimeFrameIndex(100));
    REQUIRE(series.view()[0].value().end == TimeFrameIndex(250));
}

TEST_CASE("Digital Interval Overlap Left", "[DataManager]") {

    DigitalIntervalSeries dis;
    dis.addEvent(TimeFrameIndex(0), TimeFrameIndex(10));
    dis.addEvent(TimeFrameIndex(5), TimeFrameIndex(15));

    auto data = dis.view();

    REQUIRE(dis.size() == 1);
    REQUIRE(data[0].value().start == TimeFrameIndex(0));
    REQUIRE(data[0].value().end == TimeFrameIndex(15));
}

TEST_CASE("DigitalIntervalSeries - Range-based access", "[DataManager]") {
    DigitalIntervalSeries dis;
    std::vector<int> times;
    for (int i: std::views::iota(0, 100)) {
        times.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(times);
    dis.setTimeFrame(timeframe);
    dis.addEvent(TimeFrameIndex(0), TimeFrameIndex(10)); // Interval from 0 to 10
    dis.addEvent(TimeFrameIndex(15), TimeFrameIndex(25));// Interval from 15 to 25
    dis.addEvent(TimeFrameIndex(30), TimeFrameIndex(40));// Interval from 30 to 40

    SECTION("View-based iteration") {
        // Test using the range view directly
        auto range = dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(TimeFrameIndex(5),
                                                                                            TimeFrameIndex(35),
                                                                                            *timeframe);

        std::vector<ClockTicksInterval> collected;
        for (auto const & interval: range) {
            collected.push_back(interval);
        }

        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0].start == timeframe->getTimeAtIndex(TimeFrameIndex(0)));
        REQUIRE(collected[0].end == timeframe->getTimeAtIndex(TimeFrameIndex(10)));
        REQUIRE(collected[1].start == timeframe->getTimeAtIndex(TimeFrameIndex(15)));
        REQUIRE(collected[1].end == timeframe->getTimeAtIndex(TimeFrameIndex(25)));
        REQUIRE(collected[2].start == timeframe->getTimeAtIndex(TimeFrameIndex(30)));
        REQUIRE(collected[2].end == timeframe->getTimeAtIndex(TimeFrameIndex(40)));
    }

    SECTION("CONTAINED mode returns only fully contained intervals") {
        auto range = dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::CONTAINED>(
                TimeFrameIndex(12), TimeFrameIndex(28), *timeframe);

        std::vector<ClockTicksInterval> collected(range.begin(), range.end());
        REQUIRE(collected.size() == 1);
        REQUIRE(collected[0].start == timeframe->getTimeAtIndex(TimeFrameIndex(15)));
        REQUIRE(collected[0].end == timeframe->getTimeAtIndex(TimeFrameIndex(25)));
    }

    SECTION("CLIP mode returns clipped clock-tick intervals") {
        auto clipped = dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::CLIP>(
                TimeFrameIndex(5), TimeFrameIndex(32), *timeframe);

        REQUIRE(clipped.size() == 3);
        REQUIRE(clipped[0].start == timeframe->getTimeAtIndex(TimeFrameIndex(5)));
        REQUIRE(clipped[0].end == timeframe->getTimeAtIndex(TimeFrameIndex(10)));
        REQUIRE(clipped[2].start == timeframe->getTimeAtIndex(TimeFrameIndex(30)));
        REQUIRE(clipped[2].end == timeframe->getTimeAtIndex(TimeFrameIndex(32)));
    }

    SECTION("Cross-timeframe query returns absolute clock ticks") {
        std::vector<int> sparse_times{0, 10, 20, 30, 40, 50};
        auto query_timeframe = std::make_shared<TimeFrame>(sparse_times);

        // Query indices 1..2 map to clock ticks [10, 20], excluding [30, 40].
        auto range = dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                TimeFrameIndex(1), TimeFrameIndex(2), *query_timeframe);

        std::vector<ClockTicksInterval> collected(range.begin(), range.end());
        REQUIRE(collected.size() == 2);
        REQUIRE(collected[0].start == ClockTicks(0));
        REQUIRE(collected[0].end == ClockTicks(10));
        REQUIRE(collected[1].start == ClockTicks(15));
        REQUIRE(collected[1].end == ClockTicks(25));
    }

    SECTION("Storage bounds skip intervals outside query range") {
        DigitalIntervalSeries large_dis;
        large_dis.setTimeFrame(timeframe);
        for (int i = 0; i < 200; i += 20) {
            large_dis.addEvent(TimeFrameIndex(i), TimeFrameIndex(i + 5));
        }

        auto range = large_dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                TimeFrameIndex(38), TimeFrameIndex(42), *timeframe);

        std::vector<ClockTicksInterval> collected(range.begin(), range.end());
        REQUIRE(collected.size() == 1);
        REQUIRE(collected[0].start == ClockTicks(40));
        REQUIRE(collected[0].end == ClockTicks(45));
    }
}

// =============================================================================
// Test view() method - primary iteration interface
// =============================================================================

TEST_CASE("DigitalIntervalSeries - view() method", "[DataManager][view]") {
    DigitalIntervalSeries dis;
    dis.addEvent(TimeFrameIndex(0), TimeFrameIndex(10));
    dis.addEvent(TimeFrameIndex(20), TimeFrameIndex(30));
    dis.addEvent(TimeFrameIndex(40), TimeFrameIndex(50));

    SECTION("Basic iteration with IntervalWithId accessors") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<TimeFrameInterval> collected_intervals;
        std::vector<EntityId> collected_ids;

        for (auto const interval_with_id: dis.view()) {
            collected_times.push_back(interval_with_id.time());
            collected_intervals.push_back(interval_with_id.interval);
            collected_ids.push_back(interval_with_id.id());
        }

        REQUIRE(collected_times.size() == 3);
        // time() should be the start of each interval
        REQUIRE(collected_times[0] == TimeFrameIndex(0));
        REQUIRE(collected_times[1] == TimeFrameIndex(20));
        REQUIRE(collected_times[2] == TimeFrameIndex(40));

        // Verify intervals
        REQUIRE(collected_intervals[0].start == TimeFrameIndex(0));
        REQUIRE(collected_intervals[0].end == TimeFrameIndex(10));
        REQUIRE(collected_intervals[1].start == TimeFrameIndex(20));
        REQUIRE(collected_intervals[1].end == TimeFrameIndex(30));
        REQUIRE(collected_intervals[2].start == TimeFrameIndex(40));
        REQUIRE(collected_intervals[2].end == TimeFrameIndex(50));
    }

    SECTION("Empty series") {
        DigitalIntervalSeries empty_dis;

        size_t count = 0;
        for ([[maybe_unused]] auto const interval: empty_dis.view()) {
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Direct member access via interval and entity_id") {
        auto v = dis.view();
        auto it = std::ranges::begin(v);
        auto first_element = *it;

        // Test direct member access
        REQUIRE(first_element.interval.start == TimeFrameIndex(0));
        REQUIRE(first_element.interval.end == TimeFrameIndex(10));
        REQUIRE(first_element.time() == TimeFrameIndex(0));
    }

    SECTION("Works with range algorithms") {
        // Count elements in range
        auto count = std::ranges::distance(dis.view());
        REQUIRE(count == 3);
    }
}

TEST_CASE("DigitalIntervalSeries - view() concept-compliant iteration", "[DataManager][view]") {
    DigitalIntervalSeries dis;
    dis.addEvent(TimeFrameIndex(100), TimeFrameIndex(200));
    dis.addEvent(TimeFrameIndex(300), TimeFrameIndex(400));

    SECTION("IntervalWithId satisfies concept requirements") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<EntityId> collected_ids;
        std::vector<TimeFrameInterval> collected_values;

        for (auto interval: dis.view()) {
            collected_times.push_back(interval.time());
            collected_ids.push_back(interval.id());
            collected_values.push_back(interval.value());
        }

        REQUIRE(collected_times.size() == 2);
        // time() returns start of interval
        REQUIRE(collected_times[0] == TimeFrameIndex(100));
        REQUIRE(collected_times[1] == TimeFrameIndex(300));

        // value() returns the full interval
        REQUIRE(collected_values[0].start == TimeFrameIndex(100));
        REQUIRE(collected_values[0].end == TimeFrameIndex(200));
        REQUIRE(collected_values[1].start == TimeFrameIndex(300));
        REQUIRE(collected_values[1].end == TimeFrameIndex(400));
    }
}

TEST_CASE("DigitalIntervalSeries - IntervalLayout", "[DataManager][interval][layout]") {
    SECTION("Default layout is Disjoint") {
        DigitalIntervalSeries series;
        REQUIRE(series.layout() == IntervalLayout::Disjoint);
    }

    SECTION("Disjoint layout merges overlapping addEvent calls") {
        DigitalIntervalSeries series;
        series.addEvent(TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(10)});
        series.addEvent(TimeFrameInterval{TimeFrameIndex(5), TimeFrameIndex(15)});
        REQUIRE(series.size() == 1);
        REQUIRE(series.view()[0].value().start == TimeFrameIndex(0));
        REQUIRE(series.view()[0].value().end == TimeFrameIndex(15));
    }

    SECTION("Overlapping layout preserves intervals on addEvent") {
        auto series = DigitalIntervalSeries::createOverlapping({});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(10)});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(5), TimeFrameIndex(15)});
        REQUIRE(series->layout() == IntervalLayout::Overlapping);
        REQUIRE(series->size() == 2);
    }

    SECTION("Overlapping layout does not suppress contained intervals") {
        auto series = DigitalIntervalSeries::createOverlapping({});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(200)});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(120), TimeFrameIndex(150)});
        REQUIRE(series->size() == 2);
    }

    SECTION("Overlapping layout rejects exact duplicate addEvent") {
        auto series = DigitalIntervalSeries::createOverlapping({});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(10), TimeFrameIndex(20)});
        series->addEvent(TimeFrameInterval{TimeFrameIndex(10), TimeFrameIndex(20)});
        REQUIRE(series->size() == 1);
    }

    SECTION("layout propagates through materialize and createView") {
        auto overlapping = DigitalIntervalSeries::createOverlapping(
                {TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(10)}, TimeFrameInterval{TimeFrameIndex(20), TimeFrameIndex(30)}});
        auto materialized = overlapping->materialize();
        REQUIRE(materialized->layout() == IntervalLayout::Overlapping);

        auto shared = std::const_pointer_cast<DigitalIntervalSeries const>(overlapping);
        auto view = DigitalIntervalSeries::createView(shared, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(view->layout() == IntervalLayout::Overlapping);
    }

    SECTION("createFromView defaults to Overlapping layout") {
        std::vector<std::pair<TimeFrameInterval, EntityId>> data{
                {TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(10)}, EntityId{1}},
                {TimeFrameInterval{TimeFrameIndex(20), TimeFrameIndex(30)}, EntityId{2}},
        };
        auto range_view = data | std::views::transform([](auto const & p) {
                              return IntervalWithId{p.first, p.second};
                          });
        auto lazy = DigitalIntervalSeries::createFromView(range_view, data.size());
        REQUIRE(lazy->layout() == IntervalLayout::Overlapping);
    }

    SECTION("createView time filter includes all overlapping intervals") {
        auto overlapping = DigitalIntervalSeries::createOverlapping(
                {TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(100)},
                 TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(60)}});
        auto shared = std::const_pointer_cast<DigitalIntervalSeries const>(overlapping);
        auto filtered = DigitalIntervalSeries::createView(shared, TimeFrameIndex(62), TimeFrameIndex(65));
        REQUIRE(filtered->size() == 1);
        REQUIRE(filtered->view()[0].value().start == TimeFrameIndex(0));
        REQUIRE(filtered->view()[0].value().end == TimeFrameIndex(100));
    }
}