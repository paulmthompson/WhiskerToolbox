
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
