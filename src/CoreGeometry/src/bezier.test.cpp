#include "catch2/catch_test_macros.hpp"
#include "CoreGeometry/bezier.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include <vector>

TEST_CASE("Bézier Curve Fitting", "[bezier]") {
    SECTION("getBezierParameters") {
        std::vector<Point2D<float>> points;
        points.push_back({0, 0});
        points.push_back({1, 2});
        points.push_back({2, -1});
        points.push_back({3, 1});
        points.push_back({4, 0});

        // Test with degree 3
        auto control_points = CoreGeometry::getBezierParameters(points, 3);
        REQUIRE(control_points.size() == 4);

        // The first and last control points should be the same as the first and last points of the input
        REQUIRE(control_points.front().x() == points.front().x());
        REQUIRE(control_points.front().y() == points.front().y());
        REQUIRE(control_points.back().x() == points.back().x());
        REQUIRE(control_points.back().y() == points.back().y());

        // Test with degree 4
        auto control_points_4 = CoreGeometry::getBezierParameters(points, 4);
        REQUIRE(control_points_4.size() == 5);
        REQUIRE(control_points_4.front().x() == points.front().x());
        REQUIRE(control_points_4.front().y() == points.front().y());
        REQUIRE(control_points_4.back().x() == points.back().x());
        REQUIRE(control_points_4.back().y() == points.back().y());
    }

    SECTION("bezierCurve") {
        std::vector<Point2D<float>> control_points;
        control_points.push_back({0, 0});
        control_points.push_back({1, 2});
        control_points.push_back({3, 3});
        control_points.push_back({4, 0});

        int n_points = 10;
        Line2D curve = CoreGeometry::bezierCurve(control_points, n_points);
        REQUIRE(curve.size() == n_points);

        // The first and last points of the curve should be the same as the first and last control points
        REQUIRE(curve.front().x() == control_points.front().x());
        REQUIRE(curve.front().y() == control_points.front().y());
        REQUIRE(curve.back().x() == control_points.back().x());
        REQUIRE(curve.back().y() == control_points.back().y());
    }

    SECTION("maskToLine") {
        Mask2D mask;
        mask.push_back({0, 0});
        mask.push_back({1, 2});
        mask.push_back({2, -1});
        mask.push_back({3, 1});
        mask.push_back({4, 0});

        int degree = 3;
        int n_points = 20;
        Line2D line = CoreGeometry::maskToLine(mask, degree, n_points);
        REQUIRE(line.size() == n_points);

        // The first and last points of the line should be close to the first and last points of the mask
        REQUIRE(std::abs(line.front().x() - mask.front().x()) < 1e-6);
        REQUIRE(std::abs(line.front().y() - mask.front().y()) < 1e-6);
        REQUIRE(std::abs(line.back().x() - mask.back().x()) < 1e-6);
        REQUIRE(std::abs(line.back().y() - mask.back().y()) < 1e-6);
    }
}
