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
        point_data.addPointAtTime(TimeFrameIndex(10), p1);

        auto points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));

        // Add another point at the same time
        point_data.addPointAtTime(TimeFrameIndex(10), p2);
        points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[1].x == Catch::Approx(3.0f));
        REQUIRE(points_at_10[1].y == Catch::Approx(4.0f));

        // Add points at a different time
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);
        auto points_at_20 = point_data.getPointsAtTime(TimeFrameIndex(20));
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_20[0].y == Catch::Approx(6.0f));
    }

    SECTION("Overwriting points at time") {
        // Add initial points
        point_data.addPointsAtTime(TimeFrameIndex(10), points);

        // Overwrite with a single point
        point_data.overwritePointAtTime(TimeFrameIndex(10), p3);
        auto points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(6.0f));

        // Overwrite with multiple points
        point_data.overwritePointsAtTime(TimeFrameIndex(10), points);
        points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));
    }

    SECTION("Clearing points at time") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);

        point_data.clearAtTime(TimeFrameIndex(10));

        auto points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getPointsAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.empty());
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("GetAllPointsAsRange functionality") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);

        size_t count = 0;
        int64_t first_time = 0;
        int64_t second_time = 0;

        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (count == 0) {
                first_time = pair.time.getValue();
                REQUIRE(pair.points.size() == 2);
            } else if (count == 1) {
                second_time = pair.time.getValue();
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
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
        std::vector<std::vector<Point2D<float>>> points_vec = {points, more_points};

        point_data.overwritePointsAtTimes(times, points_vec);

        auto points_at_10 = point_data.getPointsAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getPointsAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("Getting times with points") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);

        auto times = point_data.getTimesWithData();

        REQUIRE(times.size() == 2);
        REQUIRE(times[0].getValue() == 10);
        REQUIRE(times[1].getValue() == 20);
    }

    SECTION("Getting max points") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);      // 2 points
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points); // 1 point

        auto max_points = point_data.getMaxPoints();
        REQUIRE(max_points == 2);
    }
}

TEST_CASE("PointData - Edge cases and error handling", "[points][data][error]") {
    PointData point_data;

    SECTION("Getting points at non-existent time") {
        auto points = point_data.getPointsAtTime(TimeFrameIndex(999));
        REQUIRE(points.empty());
    }

    SECTION("Clearing points at non-existent time") {
        point_data.clearAtTime(TimeFrameIndex(42));
        auto points = point_data.getPointsAtTime(TimeFrameIndex(42));
        REQUIRE(points.empty());

        // Verify the time was NOT created
        bool found = false;
        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (pair.time.getValue() == 42) {
                found = true;
                break;
            }
        }
        REQUIRE(found == false);
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
        point_data.addPointsAtTime(TimeFrameIndex(10), empty_points);

        auto points = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(points.empty());

        // Verify the time was created
        bool found = false;
        for (const auto& pair : point_data.GetAllPointsAsRange()) {
            if (pair.time.getValue() == 10) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Overwriting points at times with mismatched vectors") {
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
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
        point_data.addPointAtTime(TimeFrameIndex(5), p1);
        point_data.clearAtTime(TimeFrameIndex(5));
        point_data.addPointAtTime(TimeFrameIndex(5), p1);

        auto points = point_data.getPointsAtTime(TimeFrameIndex(5));
        REQUIRE(points.size() == 1);
        REQUIRE(points[0].x == Catch::Approx(1.0f));
    }

    SECTION("Construction from map") {
        std::map<TimeFrameIndex, std::vector<Point2D<float>>> map_data = {
                {TimeFrameIndex(10), {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                {TimeFrameIndex(20), {{5.0f, 6.0f}}}
        };

        PointData point_data_from_map(map_data);

        auto points_at_10 = point_data_from_map.getPointsAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data_from_map.getPointsAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
    }
}

TEST_CASE("PointData - Copy and Move operations", "[points][data][copy][move]") {
    PointData source_data;
    PointData target_data;

    // Setup test data
    std::vector<Point2D<float>> points_10 = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    std::vector<Point2D<float>> points_15 = {{5.0f, 6.0f}};
    std::vector<Point2D<float>> points_20 = {{7.0f, 8.0f}, {9.0f, 10.0f}, {11.0f, 12.0f}};
    std::vector<Point2D<float>> points_25 = {{13.0f, 14.0f}};

    source_data.addPointsAtTime(TimeFrameIndex(10), points_10);
    source_data.addPointsAtTime(TimeFrameIndex(15), points_15);
    source_data.addPointsAtTime(TimeFrameIndex(20), points_20);
    source_data.addPointsAtTime(TimeFrameIndex(25), points_25);

    SECTION("copyTo - time range operations") {
        SECTION("Copy entire range") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(25));
            
            REQUIRE(copied == 7); // 2 + 1 + 3 + 1 = 7 points total
            
            // Verify all points were copied
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Verify source data is unchanged
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
        }

        SECTION("Copy partial range") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(15), TimeFrameIndex(20));
            
            REQUIRE(copied == 4); // 1 + 3 = 4 points
            
            // Verify only points in range were copied
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).empty());
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).empty());
        }

        SECTION("Copy single time point") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(20), TimeFrameIndex(20));
            
            REQUIRE(copied == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).empty());
        }

        SECTION("Copy non-existent range") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(100), TimeFrameIndex(200));
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Copy with invalid range (start > end)") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(25), TimeFrameIndex(10));
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }
    }

    SECTION("copyTo - specific times vector operations") {
        SECTION("Copy multiple specific times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 5); // 2 + 3 = 5 points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).empty());
        }

        SECTION("Copy times in non-sequential order") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(25), TimeFrameIndex(10), TimeFrameIndex(15)}; // Non-sequential
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 4); // 1 + 2 + 1 = 4 points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
        }

        SECTION("Copy with duplicate times in vector") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(10), TimeFrameIndex(20)}; // Duplicate time
            std::size_t copied = source_data.copyTo(target_data, times);
            
            // Should copy 10 twice and 20 once
            REQUIRE(copied == 7); // 2 + 2 + 3 = 7 points (10 copied twice)
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 4); // 2 + 2 = 4 (added twice)
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
        }

        SECTION("Copy non-existent times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300)};
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Copy mixed existent and non-existent times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(100), TimeFrameIndex(20), TimeFrameIndex(200)};
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 5); // Only times 10 and 20 exist
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
        }
    }

    SECTION("moveTo - time range operations") {
        SECTION("Move entire range") {
            std::size_t moved = source_data.moveTo(target_data, TimeFrameIndex(10), TimeFrameIndex(25));
            
            REQUIRE(moved == 7); // 2 + 1 + 3 + 1 = 7 points total
            
            // Verify all points were moved to target
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Verify source data is now empty
            REQUIRE(source_data.getTimesWithData().empty());
        }

        SECTION("Move partial range") {
            std::size_t moved = source_data.moveTo(target_data, TimeFrameIndex(15), TimeFrameIndex(20));
            
            REQUIRE(moved == 4); // 1 + 3 = 4 points
            
            // Verify only points in range were moved
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify source still has data outside the range
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(20)).empty());
        }

        SECTION("Move single time point") {
            std::size_t moved = source_data.moveTo(target_data, TimeFrameIndex(20), TimeFrameIndex(20));
            
            REQUIRE(moved == 3);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify only time 20 was removed from source
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(20)).empty());
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
        }
    }

    SECTION("moveTo - specific times vector operations") {
        SECTION("Move multiple specific times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 5); // 2 + 3 = 5 points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify moved times are cleared from source
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(10)).empty());
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(20)).empty());
            // But other times remain
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
        }

        SECTION("Move times in non-sequential order") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(25), TimeFrameIndex(10), TimeFrameIndex(15)}; // Non-sequential
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 4); // 1 + 2 + 1 = 4 points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Only time 20 should remain in source
            REQUIRE(source_data.getTimesWithData().size() == 1);
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(20)).size() == 3);
        }
    }

    SECTION("Copy/Move to target with existing data") {
        // Add some existing data to target
        std::vector<Point2D<float>> existing_points = {{100.0f, 200.0f}};
        target_data.addPointsAtTime(TimeFrameIndex(10), existing_points);

        SECTION("Copy to existing time adds points") {
            std::size_t copied = source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(10));
            
            REQUIRE(copied == 2);
            // Should have existing point plus copied points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 copied
        }

        SECTION("Move to existing time adds points") {
            std::size_t moved = source_data.moveTo(target_data, TimeFrameIndex(10), TimeFrameIndex(10));
            
            REQUIRE(moved == 2);
            // Should have existing point plus moved points
            REQUIRE(target_data.getPointsAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 moved
            
            // Source should no longer have points at time 10
            REQUIRE(source_data.getPointsAtTime(TimeFrameIndex(10)).empty());
        }
    }
}

TEST_CASE("PointData - Image scaling", "[points][data][scaling]") {
    PointData point_data;
    
    // Setup test data with known coordinates
    std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    point_data.addPointsAtTime(TimeFrameIndex(10), points);
    
    SECTION("Scaling from known size") {
        // Set initial image size
        ImageSize initial_size{640, 480};
        point_data.setImageSize(initial_size);
        
        // Scale to larger size
        ImageSize new_size{1280, 960};
        point_data.changeImageSize(new_size);
        
        auto scaled_points = point_data.getPointsAtTime(TimeFrameIndex(10));
        
        // Points should be scaled by factor of 2
        REQUIRE(scaled_points[0].x == Catch::Approx(200.0f));
        REQUIRE(scaled_points[0].y == Catch::Approx(400.0f));
        REQUIRE(scaled_points[1].x == Catch::Approx(600.0f));
        REQUIRE(scaled_points[1].y == Catch::Approx(800.0f));
        
        // Image size should be updated
        auto current_size = point_data.getImageSize();
        REQUIRE(current_size.width == 1280);
        REQUIRE(current_size.height == 960);
    }
    
    SECTION("Scaling with no initial size set") {
        // Try to scale without setting initial size
        ImageSize new_size{1280, 960};
        point_data.changeImageSize(new_size);
        
        // Points should remain unchanged since no initial size was set
        auto unchanged_points = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(unchanged_points[0].x == Catch::Approx(100.0f));
        REQUIRE(unchanged_points[0].y == Catch::Approx(200.0f));
        
        // But image size should now be set
        auto current_size = point_data.getImageSize();
        REQUIRE(current_size.width == 1280);
        REQUIRE(current_size.height == 960);
    }
    
    SECTION("Scaling to same size does nothing") {
        ImageSize size{640, 480};
        point_data.setImageSize(size);
        
        // Scale to same size
        point_data.changeImageSize(size);
        
        // Points should remain unchanged
        auto unchanged_points = point_data.getPointsAtTime(TimeFrameIndex(10));
        REQUIRE(unchanged_points[0].x == Catch::Approx(100.0f));
        REQUIRE(unchanged_points[0].y == Catch::Approx(200.0f));
    }
}
