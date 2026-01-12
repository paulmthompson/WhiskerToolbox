
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

TEST_CASE("Digital Interval Overlap Left", "[DataManager]") {

    DigitalIntervalSeries dis;
    dis.addEvent(TimeFrameIndex(0), TimeFrameIndex(10));
    dis.addEvent(TimeFrameIndex(5), TimeFrameIndex(15));

    auto data = dis.getDigitalIntervalSeries();

    REQUIRE(data.size() == 1);
    REQUIRE(data[0].start == 0);
    REQUIRE(data[0].end == 15);
}

TEST_CASE("DigitalIntervalSeries - Range-based access", "[DataManager]") {
    DigitalIntervalSeries dis;
    dis.addEvent(TimeFrameIndex(0), TimeFrameIndex(10)); // Interval from 0 to 10
    dis.addEvent(TimeFrameIndex(15), TimeFrameIndex(25));// Interval from 15 to 25
    dis.addEvent(TimeFrameIndex(30), TimeFrameIndex(40));// Interval from 30 to 40

    SECTION("View-based iteration") {
        // Test using the range view directly
        auto range = dis.getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(5, 35);

        std::vector<Interval> collected;
        for (auto const & interval: range) {
            collected.push_back(interval);
        }

        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0].start == 0);
        REQUIRE(collected[0].end == 10);
        REQUIRE(collected[1].start == 15);
        REQUIRE(collected[1].end == 25);
        REQUIRE(collected[2].start == 30);
        REQUIRE(collected[2].end == 40);
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
        std::vector<Interval> collected_intervals;
        std::vector<EntityId> collected_ids;

        for (auto const interval_with_id : dis.view()) {
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
        REQUIRE(collected_intervals[0].start == 0);
        REQUIRE(collected_intervals[0].end == 10);
        REQUIRE(collected_intervals[1].start == 20);
        REQUIRE(collected_intervals[1].end == 30);
        REQUIRE(collected_intervals[2].start == 40);
        REQUIRE(collected_intervals[2].end == 50);
    }

    SECTION("Empty series") {
        DigitalIntervalSeries empty_dis;

        size_t count = 0;
        for ([[maybe_unused]] auto const interval : empty_dis.view()) {
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Direct member access via interval and entity_id") {
        auto v = dis.view();
        auto it = std::ranges::begin(v);
        auto first_element = *it;

        // Test direct member access
        REQUIRE(first_element.interval.start == 0);
        REQUIRE(first_element.interval.end == 10);
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
        std::vector<Interval> collected_values;

        for (auto interval : dis.view()) {
            collected_times.push_back(interval.time());
            collected_ids.push_back(interval.id());
            collected_values.push_back(interval.value());
        }

        REQUIRE(collected_times.size() == 2);
        // time() returns start of interval
        REQUIRE(collected_times[0] == TimeFrameIndex(100));
        REQUIRE(collected_times[1] == TimeFrameIndex(300));

        // value() returns the full interval
        REQUIRE(collected_values[0].start == 100);
        REQUIRE(collected_values[0].end == 200);
        REQUIRE(collected_values[1].start == 300);
        REQUIRE(collected_values[1].end == 400);    }
}