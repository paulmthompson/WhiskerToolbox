/**
 * @file GridPointsGenerator.test.cpp
 * @brief Unit tests for the GridPoints generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<PointData> runGridPoints(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("GridPoints", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<PointData>>(*result);
}

static std::vector<Point2D<float>> getPointsAtTime(PointData const & pd, int frame) {
    auto range = pd.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("GridPoints produces correct number of frames", "[GridPoints]") {
    auto pd = runGridPoints(R"({"rows": 3, "cols": 3, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 12})");
    REQUIRE(pd->getTimeCount() == 12);
}

TEST_CASE("GridPoints produces correct number of points per frame", "[GridPoints]") {
    auto pd = runGridPoints(R"({"rows": 4, "cols": 5, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 1})");
    auto points = getPointsAtTime(*pd, 0);
    REQUIRE(points.size() == 20);
}

TEST_CASE("GridPoints origin offsets all points", "[GridPoints]") {
    auto pd = runGridPoints(R"({"num_frames": 1, "rows": 2, "cols": 2, "spacing": 10, "origin_x": 50, "origin_y": 100})");
    auto points = getPointsAtTime(*pd, 0);
    REQUIRE(points.size() == 4);

    // First point should be at origin
    REQUIRE(points[0].x == Catch::Approx(50.0f));
    REQUIRE(points[0].y == Catch::Approx(100.0f));

    // Last point (row=1, col=1) should be at (50+10, 100+10)
    REQUIRE(points[3].x == Catch::Approx(60.0f));
    REQUIRE(points[3].y == Catch::Approx(110.0f));
}

TEST_CASE("GridPoints spacing is correct", "[GridPoints]") {
    float const spacing = 25.0f;
    auto pd = runGridPoints(R"({"rows": 1, "cols": 3, "spacing": 25, "origin_x": 0, "origin_y": 0, "num_frames": 1})");
    auto points = getPointsAtTime(*pd, 0);
    REQUIRE(points.size() == 3);

    REQUIRE(points[0].x == Catch::Approx(0.0f));
    REQUIRE(points[1].x == Catch::Approx(spacing));
    REQUIRE(points[2].x == Catch::Approx(spacing * 2));
}

TEST_CASE("GridPoints all frames have identical points", "[GridPoints]") {
    auto pd = runGridPoints(R"({"rows": 2, "cols": 3, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 5})");
    auto points_0 = getPointsAtTime(*pd, 0);
    auto points_4 = getPointsAtTime(*pd, 4);
    REQUIRE(points_0.size() == points_4.size());
    REQUIRE(points_0.size() == 6);
}

TEST_CASE("GridPoints rejects invalid parameters", "[GridPoints]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("GridPoints", R"({"rows": 3, "cols": 3, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("GridPoints", R"({"rows": 0, "cols": 3, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("GridPoints", R"({"rows": 3, "cols": 0, "spacing": 10, "origin_x": 0, "origin_y": 0, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("GridPoints", R"({"rows": 3, "cols": 3, "spacing": -1, "origin_x": 0, "origin_y": 0, "num_frames": 1})").has_value());
}
