/**
 * @file RegularIntervalsGenerator.test.cpp
 * @brief Unit tests for the RegularIntervals generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<DigitalIntervalSeries> runRegularIntervals(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("RegularIntervals", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalIntervalSeries>>(*result);
}

TEST_CASE("RegularIntervals produces correct number of intervals", "[RegularIntervals]") {
    // period = 50 + 50 = 100. In 1000 samples: 0, 100, 200, ..., 900 => 10 intervals
    auto dis = runRegularIntervals(R"({"num_samples": 1000, "on_duration": 50, "off_duration": 50})");
    REQUIRE(dis->size() == 10);
}

TEST_CASE("RegularIntervals intervals have correct timing", "[RegularIntervals]") {
    auto dis = runRegularIntervals(R"({"num_samples": 300, "on_duration": 30, "off_duration": 70})");
    // period = 100. Intervals: [0,29], [100,129], [200,229] => 3 intervals
    REQUIRE(dis->size() == 3);

    auto v = dis->view();
    REQUIRE(v[0].value().start == 0);
    REQUIRE(v[0].value().end == 29);
    REQUIRE(v[1].value().start == 100);
    REQUIRE(v[1].value().end == 129);
    REQUIRE(v[2].value().start == 200);
    REQUIRE(v[2].value().end == 229);
}

TEST_CASE("RegularIntervals respects start_offset", "[RegularIntervals]") {
    auto dis = runRegularIntervals(
            R"({"num_samples": 200, "on_duration": 20, "off_duration": 30, "start_offset": 10})");
    // period = 50. Intervals start at 10: [10,29], [60,79], [110,129], [160,179] => 4
    REQUIRE(dis->size() == 4);

    auto v = dis->view();
    REQUIRE(v[0].value().start == 10);
    REQUIRE(v[0].value().end == 29);
    REQUIRE(v[1].value().start == 60);
    REQUIRE(v[1].value().end == 79);
}

TEST_CASE("RegularIntervals last interval is clamped to num_samples", "[RegularIntervals]") {
    // on_duration = 60, off_duration = 50, period = 110
    // Start at 0: [0,59], next at 110: [110, min(169,149)] = [110,149]
    auto dis = runRegularIntervals(R"({"num_samples": 150, "on_duration": 60, "off_duration": 50})");
    REQUIRE(dis->size() == 2);

    auto v = dis->view();
    REQUIRE(v[1].value().end == 149);
}

TEST_CASE("RegularIntervals rejects invalid parameters", "[RegularIntervals]") {
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularIntervals",
                              R"({"num_samples": 0, "on_duration": 50, "off_duration": 50})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularIntervals",
                              R"({"num_samples": 100, "on_duration": 0, "off_duration": 50})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularIntervals",
                              R"({"num_samples": 100, "on_duration": 50, "off_duration": 0})")
                    .has_value());
}
