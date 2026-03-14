/**
 * @file RandomPointsGenerator.test.cpp
 * @brief Unit tests for the RandomPoints generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<PointData> runRandomPoints(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("RandomPoints", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<PointData>>(*result);
}

static std::vector<Point2D<float>> getPointsAtTime(PointData const & pd, int frame) {
    auto range = pd.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("RandomPoints produces correct number of frames", "[RandomPoints]") {
    auto pd = runRandomPoints(R"({"num_points": 5, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 15})");
    REQUIRE(pd->getTimeCount() == 15);
}

TEST_CASE("RandomPoints produces correct number of points per frame", "[RandomPoints]") {
    auto pd = runRandomPoints(R"({"num_points": 30, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1})");
    auto points = getPointsAtTime(*pd, 0);
    REQUIRE(points.size() == 30);
}

TEST_CASE("RandomPoints are within bounds", "[RandomPoints]") {
    auto pd = runRandomPoints(R"({"num_frames": 1, "num_points": 100, "min_x": 10, "max_x": 50, "min_y": 20, "max_y": 80, "seed": 123})");
    auto points = getPointsAtTime(*pd, 0);
    REQUIRE(points.size() == 100);

    for (auto const & pt: points) {
        REQUIRE(pt.x >= 10.0f);
        REQUIRE(pt.x <= 50.0f);
        REQUIRE(pt.y >= 20.0f);
        REQUIRE(pt.y <= 80.0f);
    }
}

TEST_CASE("RandomPoints is deterministic with same seed", "[RandomPoints]") {
    auto pd1 = runRandomPoints(R"({"num_points": 10, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1, "seed": 99})");
    auto pd2 = runRandomPoints(R"({"num_points": 10, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1, "seed": 99})");

    auto points1 = getPointsAtTime(*pd1, 0);
    auto points2 = getPointsAtTime(*pd2, 0);
    REQUIRE(points1.size() == points2.size());

    for (size_t i = 0; i < points1.size(); ++i) {
        REQUIRE(points1[i].x == points2[i].x);
        REQUIRE(points1[i].y == points2[i].y);
    }
}

TEST_CASE("RandomPoints different seeds produce different results", "[RandomPoints]") {
    auto pd1 = runRandomPoints(R"({"num_points": 10, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1, "seed": 1})");
    auto pd2 = runRandomPoints(R"({"num_points": 10, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1, "seed": 2})");

    auto points1 = getPointsAtTime(*pd1, 0);
    auto points2 = getPointsAtTime(*pd2, 0);

    bool all_same = true;
    for (size_t i = 0; i < points1.size(); ++i) {
        if (points1[i].x != points2[i].x || points1[i].y != points2[i].y) {
            all_same = false;
            break;
        }
    }
    REQUIRE_FALSE(all_same);
}

TEST_CASE("RandomPoints rejects invalid parameters", "[RandomPoints]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("RandomPoints", R"({"num_points": 5, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("RandomPoints", R"({"num_points": 0, "min_x": 0, "max_x": 640, "min_y": 0, "max_y": 480, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("RandomPoints", R"({"num_points": 5, "min_x": 50, "max_x": 10, "min_y": 0, "max_y": 480, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("RandomPoints", R"({"num_points": 5, "min_x": 0, "max_x": 640, "min_y": 50, "max_y": 10, "num_frames": 1})").has_value());
}
