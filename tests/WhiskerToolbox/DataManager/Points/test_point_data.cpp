#include "Points/Point_Data.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("PointData - Core functionality", "[points][data][core]") {
    PointData point_data;

    // Setup some test data
    Point2D<float> p1{1.0f, 2.0f};
    Point2D<float> p2{3.0f, 4.0f};
    Point2D<float> p3{5.0f, 6.0f};
    std::vector<Point2D<float>> points = {p1, p2};
    std::vector<Point2D<float>> more_points = {p3};

    SECTION("Adding and retrieving points at time") {
        // Add a single point at time 10
        point_data.addPointAtTime(10, p1);

        auto points_at_10 = point_data.getPointsAtTime(10);
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));

        // Add another point at the same time
        point_data.addPointAtTime(10, p2);
        points_at_10 = point_data.getPointsAtTime(10);
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[1].x == Catch::Approx(3.0f));
        REQUIRE(points_at_10[1].y == Catch::Approx(4.0f));

        // Add points at a different time
        point_data.addPointsAtTime(20, more_points);
        auto points_at_20 = point_data.getPointsAtTime(20);
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_20[0].y == Catch::Approx(6.0f));
    }

    SECTION("Overwriting points at time") {
        // Add initial points
        point_data.addPointsAtTime(10, points);

        // Overwrite with a single point
        point_data.overwritePointAtTime(10, p3);
        auto points_at_10 = point_data.getPointsAtTime(10);
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(6.0f));

        // Overwrite with multiple points
        point_data.overwritePointsAtTime(10, points);
        points_at_10 = point_data.getPointsAtTime(10);
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));
    }

    SECTION("Clearing points at time") {
        point_data.addPointsAtTime(10, points);
        point_data.addPointsAtTime(20, more_points);

        point_data.clearPointsAtTime(10);

        auto points_at_10 = point_data.getPointsAtTime(10);
        auto points_at_20 = point_data.getPointsAtTime(20);

        REQUIRE(points_at_10.empty());
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("GetAllPointsAsRange functionality") {
        point_data.addPointsAtTime(10, points);
        point_data.addPointsAtTime(20, more_points);

        size_t count = 0;
        size_t first_time = 0;
        size_t second_time = 0;

        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (count == 0) {
                first_time = pair.time;
                REQUIRE(pair.points.size() == 2);
            } else if (count == 1) {
                second_time = pair.time;
                REQUIRE(pair.points.size() == 1);
            }
            count++;
        }

        REQUIRE(count == 2);
        REQUIRE(first_time == 10);
        REQUIRE(second_time == 20);
    }

    SECTION("Setting and getting image size") {
        ImageSize size{640, 480};
        point_data.setImageSize(size);

        auto retrieved_size = point_data.getImageSize();
        REQUIRE(retrieved_size.width == 640);
        REQUIRE(retrieved_size.height == 480);
    }

    SECTION("Overwriting points at multiple times") {
        std::vector<size_t> times = {10, 20};
        std::vector<std::vector<Point2D<float>>> points_vec = {points, more_points};

        point_data.overwritePointsAtTimes(times, points_vec);

        auto points_at_10 = point_data.getPointsAtTime(10);
        auto points_at_20 = point_data.getPointsAtTime(20);

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("Getting times with points") {
        point_data.addPointsAtTime(10, points);
        point_data.addPointsAtTime(20, more_points);

        auto times = point_data.getTimesWithPoints();

        REQUIRE(times.size() == 2);
        REQUIRE(times[0] == 10);
        REQUIRE(times[1] == 20);
    }

    SECTION("Getting max points") {
        point_data.addPointsAtTime(10, points);      // 2 points
        point_data.addPointsAtTime(20, more_points); // 1 point

        auto max_points = point_data.getMaxPoints();
        REQUIRE(max_points == 2);
    }
}

TEST_CASE("PointData - Edge cases and error handling", "[points][data][error]") {
    PointData point_data;

    SECTION("Getting points at non-existent time") {
        auto points = point_data.getPointsAtTime(999);
        REQUIRE(points.empty());
    }

    SECTION("Clearing points at non-existent time") {
        point_data.clearPointsAtTime(42);
        auto points = point_data.getPointsAtTime(42);
        REQUIRE(points.empty());

        // Verify the time was actually created
        bool found = false;
        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (pair.time == 42) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Empty range with no data") {
        auto range = point_data.GetAllPointsAsRange();
        size_t count = 0;

        for (auto const& pair : range) {
            count++;
        }

        REQUIRE(count == 0);
    }

    SECTION("Adding empty points vector") {
        std::vector<Point2D<float>> empty_points;
        point_data.addPointsAtTime(10, empty_points);

        auto points = point_data.getPointsAtTime(10);
        REQUIRE(points.empty());

        // Verify the time was created
        bool found = false;
        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (pair.time == 10) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Overwriting points at times with mismatched vectors") {
        std::vector<size_t> times = {10, 20, 30};
        std::vector<std::vector<Point2D<float>>> points = {
                {{1.0f, 2.0f}},
                {{3.0f, 4.0f}}
        }; // Only 2 elements

        // This should not modify the data
        point_data.overwritePointsAtTimes(times, points);

        auto range = point_data.GetAllPointsAsRange();
        size_t count = 0;
        for (auto const& pair : range) {
            count++;
        }

        // No points should be added since we had a size mismatch
        REQUIRE(count == 0);
    }

    SECTION("Multiple operations sequence") {
        Point2D<float> p1{1.0f, 2.0f};

        // Add, clear, add again to test internal state consistency
        point_data.addPointAtTime(5, p1);
        point_data.clearPointsAtTime(5);
        point_data.addPointAtTime(5, p1);

        auto points = point_data.getPointsAtTime(5);
        REQUIRE(points.size() == 1);
        REQUIRE(points[0].x == Catch::Approx(1.0f));
    }

    SECTION("Construction from map") {
        std::map<int, std::vector<Point2D<float>>> map_data = {
                {10, {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                {20, {{5.0f, 6.0f}}}
        };

        PointData point_data_from_map(map_data);

        auto points_at_10 = point_data_from_map.getPointsAtTime(10);
        auto points_at_20 = point_data_from_map.getPointsAtTime(20);

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
    }
}
