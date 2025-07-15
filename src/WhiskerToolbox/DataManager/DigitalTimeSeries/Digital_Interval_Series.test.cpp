
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
