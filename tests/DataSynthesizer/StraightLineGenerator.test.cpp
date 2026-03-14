/**
 * @file StraightLineGenerator.test.cpp
 * @brief Unit tests for the StraightLine generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Lines/Line_Data.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<LineData> runStraightLine(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("StraightLine", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<LineData>>(*result);
}

static std::vector<Line2D> getLinesAtTime(LineData const & ld, int frame) {
    auto range = ld.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("StraightLine produces correct number of frames", "[StraightLine]") {
    auto ld = runStraightLine(R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100, "num_points_per_line": 50, "num_frames": 6})");
    REQUIRE(ld->getTimeCount() == 6);
}

TEST_CASE("StraightLine produces lines with correct number of points", "[StraightLine]") {
    auto ld = runStraightLine(R"({"num_frames": 1, "num_points_per_line": 25, "start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100})");
    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 25);
}

TEST_CASE("StraightLine starts and ends at correct positions", "[StraightLine]") {
    auto ld = runStraightLine(R"({"num_frames": 1, "num_points_per_line": 10, "start_x": 10, "start_y": 20, "end_x": 90, "end_y": 80})");
    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);

    auto const & line = lines[0];
    REQUIRE(line[0].x == Catch::Approx(10.0f));
    REQUIRE(line[0].y == Catch::Approx(20.0f));
    REQUIRE(line[line.size() - 1].x == Catch::Approx(90.0f));
    REQUIRE(line[line.size() - 1].y == Catch::Approx(80.0f));
}

TEST_CASE("StraightLine points are evenly spaced", "[StraightLine]") {
    auto ld = runStraightLine(R"({"num_frames": 1, "num_points_per_line": 5, "start_x": 0, "start_y": 0, "end_x": 40, "end_y": 0})");
    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);

    auto const & line = lines[0];
    REQUIRE(line.size() == 5);

    // Expected: x = 0, 10, 20, 30, 40
    REQUIRE(line[0].x == Catch::Approx(0.0f));
    REQUIRE(line[1].x == Catch::Approx(10.0f));
    REQUIRE(line[2].x == Catch::Approx(20.0f));
    REQUIRE(line[3].x == Catch::Approx(30.0f));
    REQUIRE(line[4].x == Catch::Approx(40.0f));

    for (size_t i = 0; i < 5; ++i) {
        REQUIRE(line[i].y == Catch::Approx(0.0f));
    }
}

TEST_CASE("StraightLine all frames have identical lines", "[StraightLine]") {
    auto ld = runStraightLine(R"({"num_frames": 3, "num_points_per_line": 10, "start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100})");
    auto lines_0 = getLinesAtTime(*ld, 0);
    auto lines_2 = getLinesAtTime(*ld, 2);
    REQUIRE(lines_0.size() == 1);
    REQUIRE(lines_2.size() == 1);
    REQUIRE(lines_0[0].size() == lines_2[0].size());
}

TEST_CASE("StraightLine diagonal line has correct length", "[StraightLine]") {
    auto ld = runStraightLine(R"({"num_frames": 1, "num_points_per_line": 100, "start_x": 0, "start_y": 0, "end_x": 30, "end_y": 40})");
    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);

    auto const & line = lines[0];
    // Total Euclidean distance should be 50 (3-4-5 triangle scaled by 10)
    float total_dist = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        float const dx = line[i].x - line[i - 1].x;
        float const dy = line[i].y - line[i - 1].y;
        total_dist += std::sqrt(dx * dx + dy * dy);
    }
    REQUIRE(total_dist == Catch::Approx(50.0f).margin(0.1f));
}

TEST_CASE("StraightLine rejects invalid parameters", "[StraightLine]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("StraightLine", R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100, "num_points_per_line": 50, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("StraightLine", R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100, "num_points_per_line": 1, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("StraightLine", R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 100, "num_points_per_line": 0, "num_frames": 1})").has_value());
}
