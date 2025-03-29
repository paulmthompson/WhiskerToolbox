
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

TEST_CASE("Digital Interval Overlap Left", "[DataManager]") {

    DigitalIntervalSeries dis;
    dis.addEvent(0, 10);
    dis.addEvent(5, 15);

    auto data = dis.getDigitalIntervalSeries();

    REQUIRE(data.size() == 1);
    REQUIRE(data[0].start == 0);
    REQUIRE(data[0].end == 15);
}

TEST_CASE("DigitalIntervalSeries - Range-based access", "[DataManager]") {
    DigitalIntervalSeries dis;
    dis.addEvent(0, 10); // Interval from 0 to 10
    dis.addEvent(15, 25);// Interval from 15 to 25
    dis.addEvent(30, 40);// Interval from 30 to 40

    SECTION("CONTAINED mode") {
        // Test contained mode with various ranges
        auto intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::CONTAINED>(5, 30);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 15);
        REQUIRE(intervals[0].end == 25);

        // Range that doesn't fully contain any interval
        intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::CONTAINED>(11, 14);
        REQUIRE(intervals.empty());

        // Range that exactly matches an interval
        intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::CONTAINED>(15, 25);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 15);
        REQUIRE(intervals[0].end == 25);
    }

    SECTION("OVERLAPPING mode") {
        // Test overlapping mode with various ranges
        auto intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(5, 20);
        REQUIRE(intervals.size() == 2);
        REQUIRE(intervals[0].start == 0);
        REQUIRE(intervals[0].end == 10);
        REQUIRE(intervals[1].start == 15);
        REQUIRE(intervals[1].end == 25);

        // Range between intervals
        intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(11, 14);
        REQUIRE(intervals.empty());

        // Range that overlaps with the end of an interval
        intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(8, 12);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 0);
        REQUIRE(intervals[0].end == 10);
    }

    SECTION("CLIP mode") {
        // Test clip mode with various ranges
        auto intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::CLIP>(5, 20);
        REQUIRE(intervals.size() == 2);
        REQUIRE(intervals[0].start == 5);// Clipped from 0
        REQUIRE(intervals[0].end == 10);
        REQUIRE(intervals[1].start == 15);
        REQUIRE(intervals[1].end == 20);// Clipped from 25

        // Range that spans multiple intervals
        intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::CLIP>(5, 35);
        REQUIRE(intervals.size() == 3);
        REQUIRE(intervals[0].start == 5);// Clipped
        REQUIRE(intervals[0].end == 10);
        REQUIRE(intervals[1].start == 15);// Unchanged
        REQUIRE(intervals[1].end == 25);  // Unchanged
        REQUIRE(intervals[2].start == 30);// Unchanged
        REQUIRE(intervals[2].end == 35);  // Clipped
    }

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

TEST_CASE("DigitalIntervalSeries - Empty and edge cases", "[DataManager]") {
    DigitalIntervalSeries dis;

    SECTION("Empty series") {
        auto intervals = dis.getIntervalsAsVector(0, 10);
        REQUIRE(intervals.empty());
    }

    dis.addEvent(10, 20);

    SECTION("Range before all intervals") {
        auto intervals = dis.getIntervalsAsVector(0, 5);
        REQUIRE(intervals.empty());
    }

    SECTION("Range after all intervals") {
        auto intervals = dis.getIntervalsAsVector(25, 30);
        REQUIRE(intervals.empty());
    }

    SECTION("Range exactly matching interval") {
        auto intervals = dis.getIntervalsAsVector(10, 20);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 10);
        REQUIRE(intervals[0].end == 20);
    }

    SECTION("Single-point range at start boundary") {
        auto intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(10, 10);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 10);
        REQUIRE(intervals[0].end == 20);
    }

    SECTION("Single-point range at end boundary") {
        auto intervals = dis.getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(20, 20);
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 10);
        REQUIRE(intervals[0].end == 20);
    }
}
