/**
 * @file LineEditOperations.test.cpp
 * @brief Unit tests for Media Viewer line edit helpers
 */

#include "Core/LineEditOperations.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

Line2D makeZigZagLine() {
    return Line2D({Point2D<float>{0.0f, 0.0f},
                   Point2D<float>{10.0f, 5.0f},
                   Point2D<float>{20.0f, 0.0f},
                   Point2D<float>{30.0f, 5.0f},
                   Point2D<float>{40.0f, 0.0f}});
}

}// namespace

TEST_CASE("smoothPolylineLocally returns false for empty or missed brush", "[LineEditOperations]") {
    LineSmoothBrushParams params;
    params.center = {10.0f, 0.0f};
    params.radius_px = 5.0f;

    Line2D empty_line;
    REQUIRE_FALSE(smoothPolylineLocally(empty_line, params));

    Line2D single_point({Point2D<float>{0.0f, 0.0f}});
    REQUIRE_FALSE(smoothPolylineLocally(single_point, params));

    Line2D line = makeZigZagLine();
    params.center = {100.0f, 100.0f};
    REQUIRE_FALSE(smoothPolylineLocally(line, params));
}

TEST_CASE("smoothPolylineLocally preserves endpoints outside brush", "[LineEditOperations]") {
    Line2D line = makeZigZagLine();
    Line2D const original = line;

    LineSmoothBrushParams params;
    params.center = {20.0f, 2.5f};
    params.radius_px = 12.0f;
    params.algorithm = LineSmoothAlgorithm::MovingAverage;
    params.strength = 1;

    REQUIRE(smoothPolylineLocally(line, params));
    REQUIRE(line.front().x == Catch::Approx(original.front().x));
    REQUIRE(line.front().y == Catch::Approx(original.front().y));
    REQUIRE(line.back().x == Catch::Approx(original.back().x));
    REQUIRE(line.back().y == Catch::Approx(original.back().y));
}

TEST_CASE("smoothPolylineLocally moving average changes middle vertices", "[LineEditOperations]") {
    Line2D line = makeZigZagLine();

    LineSmoothBrushParams params;
    params.center = {20.0f, 2.5f};
    params.radius_px = 12.0f;
    params.algorithm = LineSmoothAlgorithm::MovingAverage;
    params.strength = 2;

    REQUIRE(smoothPolylineLocally(line, params));
    REQUIRE(line[2].y != Catch::Approx(0.0f));
}

TEST_CASE("applyPolynomialFitToLine resamples a jagged line", "[LineEditOperations]") {
    Line2D line = makeZigZagLine();
    std::size_t const original_size = line.size();

    applyPolynomialFitToLine(line, 3);

    REQUIRE(line.size() > original_size);
    REQUIRE(line.front().x == Catch::Approx(0.0f).margin(1.0f));
    REQUIRE(line.back().x == Catch::Approx(40.0f).margin(1.0f));
}

TEST_CASE("smoothPolylineLocally polynomial fit can change vertex count locally", "[LineEditOperations]") {
    Line2D line = makeZigZagLine();
    std::size_t const original_size = line.size();

    LineSmoothBrushParams params;
    params.center = {20.0f, 2.5f};
    params.radius_px = 25.0f;
    params.algorithm = LineSmoothAlgorithm::PolynomialFit;
    params.polynomial_order = 3;

    REQUIRE(smoothPolylineLocally(line, params));
    REQUIRE(line.size() != original_size);
}
