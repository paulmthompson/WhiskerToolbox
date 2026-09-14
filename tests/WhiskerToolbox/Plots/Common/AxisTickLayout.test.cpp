#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Plots/Common/AxisTickLayout.hpp"

#include <cmath>

using Catch::Matchers::WithinRel;

using namespace Neuralyzer::Plots;

TEST_CASE("computeNiceTickInterval produces expected intervals", "[AxisTickLayout]") {
    CHECK_THAT(computeNiceTickInterval(10.0, 7), WithinRel(1.0, 0.01));
    CHECK_THAT(computeNiceTickInterval(100.0, 7), WithinRel(10.0, 0.01));
    CHECK_THAT(computeNiceTickInterval(1000.0, 7), WithinRel(100.0, 0.01));
    CHECK_THAT(computeNiceTickInterval(0.5, 7), WithinRel(0.05, 0.01));
}

TEST_CASE("computeTickPositions auto mode spans visible range", "[AxisTickLayout]") {
    AxisTickConfig config;
    config.mode = AxisTickMode::Auto;
    config.target_tick_count = 7;

    auto const positions = computeTickPositions(0.0, 100.0, config);
    REQUIRE(!positions.empty());
    CHECK(positions.front() >= 0.0);
    CHECK(positions.back() <= 100.0);
}

TEST_CASE("computeTickPositions fixed mode uses configured interval", "[AxisTickLayout]") {
    AxisTickConfig config;
    config.mode = AxisTickMode::Fixed;
    config.fixed_interval = 50.0;

    auto const positions = computeTickPositions(0.0, 200.0, config);
    REQUIRE(positions.size() >= 3);
    CHECK_THAT(positions[1] - positions[0], WithinRel(50.0, 0.01));
}

TEST_CASE("computeTickPositions includes negative ticks", "[AxisTickLayout]") {
    AxisTickConfig config;
    config.mode = AxisTickMode::Fixed;
    config.fixed_interval = 10.0;

    auto const positions = computeTickPositions(-25.0, 25.0, config);
    bool has_negative = false;
    for (double const v: positions) {
        if (v < 0.0) {
            has_negative = true;
            break;
        }
    }
    CHECK(has_negative);
}

TEST_CASE("isMajorTick identifies zero and fifth multiples", "[AxisTickLayout]") {
    double const interval = 10.0;
    CHECK(isMajorTick(0.0, interval));
    CHECK(isMajorTick(50.0, interval));
    CHECK_FALSE(isMajorTick(30.0, interval));
}
