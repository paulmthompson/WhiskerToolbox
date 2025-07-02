#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/interval_data.hpp"
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

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));

        // Add another point at the same time
        point_data.addPointAtTime(TimeFrameIndex(10), p2);
        points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[1].x == Catch::Approx(3.0f));
        REQUIRE(points_at_10[1].y == Catch::Approx(4.0f));

        // Add points at a different time
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_20[0].y == Catch::Approx(6.0f));
    }

    SECTION("Overwriting points at time") {
        // Add initial points
        point_data.addPointsAtTime(TimeFrameIndex(10), points);

        // Overwrite with a single point
        point_data.overwritePointAtTime(TimeFrameIndex(10), p3);
        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(6.0f));

        // Overwrite with multiple points
        point_data.overwritePointsAtTime(TimeFrameIndex(10), points);
        points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));
    }

    SECTION("Clearing points at time") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);

        point_data.clearAtTime(TimeFrameIndex(10));

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));

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

    SECTION("GetPointsInRange functionality") {
        // Setup data at multiple time points
        point_data.addPointsAtTime(TimeFrameIndex(5), points);       // 2 points
        point_data.addPointsAtTime(TimeFrameIndex(10), points);      // 2 points  
        point_data.addPointsAtTime(TimeFrameIndex(15), more_points); // 1 point
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points); // 1 point
        point_data.addPointsAtTime(TimeFrameIndex(25), points);      // 2 points

        SECTION("Range includes some data") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.points.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.points.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.points.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("Range includes all data") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                count++;
            }
            REQUIRE(count == 5); // Should include all 5 time points
        }

        SECTION("Range includes no data") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0); // Should be empty
        }

        SECTION("Range with single time point") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                REQUIRE(pair.time.getValue() == 15);
                REQUIRE(pair.points.size() == 1);
                count++;
            }
            REQUIRE(count == 1); // Should include only time 15
        }

        SECTION("Range with start > end") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0); // Should be empty when start > end
        }

        SECTION("Range with timeframe conversion - same timeframes") {
            // Test with same source and target timeframes
            std::vector<int> times = {5, 10, 15, 20, 25};
            auto timeframe = std::make_shared<TimeFrame>(times);
            
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval, timeframe, timeframe)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.points.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.points.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.points.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("Range with timeframe conversion - different timeframes") {
            // Create a separate point data instance for timeframe conversion test
            PointData timeframe_test_data;
            
            // Create source timeframe (video frames)
            std::vector<int> video_times = {0, 10, 20, 30, 40};  
            auto video_timeframe = std::make_shared<TimeFrame>(video_times);
            
            // Create target timeframe (data sampling)
            std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40}; 
            auto data_timeframe = std::make_shared<TimeFrame>(data_times);
            
            // Add data at target timeframe indices
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(2), points);  // At data timeframe index 2 (time=10)
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(3), more_points);  // At data timeframe index 3 (time=15)
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(4), points);  // At data timeframe index 4 (time=20)
            
            // Query video frames 1-2 (times 10-20) which should map to data indices 2-4 (times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            size_t count = 0;
            for (const auto& pair : timeframe_test_data.GetPointsInRange(video_interval, video_timeframe, data_timeframe)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 2);
                    REQUIRE(pair.points.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 3);
                    REQUIRE(pair.points.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 4);
                    REQUIRE(pair.points.size() == 2);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include converted times 2, 3, 4
        }

        SECTION("TimeFrameInterval overloads") {
            // Test the TimeFrameInterval convenience overload
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            
            size_t count = 0;
            for (const auto& pair : point_data.GetPointsInRange(interval)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.points.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.points.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.points.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("TimeFrameInterval with timeframe conversion") {
            // Create a separate point data instance for timeframe conversion test
            PointData timeframe_test_data;
            
            // Create source timeframe (video frames)
            std::vector<int> video_times = {0, 10, 20, 30, 40};  
            auto video_timeframe = std::make_shared<TimeFrame>(video_times);
            
            // Create target timeframe (data sampling)
            std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40}; 
            auto data_timeframe = std::make_shared<TimeFrame>(data_times);
            
            // Add data at target timeframe indices
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(2), points);  // At data timeframe index 2 (time=10)
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(3), more_points);  // At data timeframe index 3 (time=15)
            timeframe_test_data.addPointsAtTime(TimeFrameIndex(4), points);  // At data timeframe index 4 (time=20)
            
            // Create interval in video timeframe (frames 1-2 covering times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            
            size_t count = 0;
            for (const auto& pair : timeframe_test_data.GetPointsInRange(video_interval, video_timeframe, data_timeframe)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 2);
                    REQUIRE(pair.points.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 3);
                    REQUIRE(pair.points.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 4);
                    REQUIRE(pair.points.size() == 2);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include converted times 2, 3, 4
        }
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

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("Getting times with points") {
        point_data.addPointsAtTime(TimeFrameIndex(10), points);
        point_data.addPointsAtTime(TimeFrameIndex(20), more_points);

        auto times = point_data.getTimesWithData();

        REQUIRE(times.size() == 2);
        
        // Convert to vector for easier testing or use iterators
        auto it = times.begin();
        REQUIRE(*it == TimeFrameIndex(10));
        ++it;
        REQUIRE(*it == TimeFrameIndex(20));
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
        auto points = point_data.getAtTime(TimeFrameIndex(999));
        REQUIRE(points.empty());
    }

    SECTION("Clearing points at non-existent time") {
        point_data.clearAtTime(TimeFrameIndex(42));
        auto points = point_data.getAtTime(TimeFrameIndex(42));
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

        auto points = point_data.getAtTime(TimeFrameIndex(10));
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

        auto points = point_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(points.size() == 1);
        REQUIRE(points[0].x == Catch::Approx(1.0f));
    }

    SECTION("Construction from map") {
        std::map<TimeFrameIndex, std::vector<Point2D<float>>> map_data = {
                {TimeFrameIndex(10), {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                {TimeFrameIndex(20), {{5.0f, 6.0f}}}
        };

        PointData point_data_from_map(map_data);

        auto points_at_10 = point_data_from_map.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data_from_map.getAtTime(TimeFrameIndex(20));

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
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(25)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 7); // 2 + 1 + 3 + 1 = 7 points total
            
            // Verify all points were copied
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Verify source data is unchanged
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 3);
        }

        SECTION("Copy partial range") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 4); // 1 + 3 = 4 points
            
            // Verify only points in range were copied
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).empty());
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).empty());
        }

        SECTION("Copy single time point") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).empty());
        }

        SECTION("Copy non-existent range") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Copy with invalid range (start > end)") {
            TimeFrameInterval interval{TimeFrameIndex(25), TimeFrameIndex(10)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }
    }

    SECTION("copyTo - specific times vector operations") {
        SECTION("Copy multiple specific times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 5); // 2 + 3 = 5 points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).empty());
        }

        SECTION("Copy times in non-sequential order") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(25), TimeFrameIndex(10), TimeFrameIndex(15)}; // Non-sequential
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 4); // 1 + 2 + 1 = 4 points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).size() == 1);
        }

        SECTION("Copy with duplicate times in vector") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(10), TimeFrameIndex(20)}; // Duplicate time
            std::size_t copied = source_data.copyTo(target_data, times);
            
            // Should copy 10 twice and 20 once
            REQUIRE(copied == 7); // 2 + 2 + 3 = 7 points (10 copied twice)
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 4); // 2 + 2 = 4 (added twice)
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
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
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
        }
    }

    SECTION("moveTo - time range operations") {
        SECTION("Move entire range") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(25)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 7); // 2 + 1 + 3 + 1 = 7 points total
            
            // Verify all points were moved to target
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Verify source data is now empty
            REQUIRE(source_data.getTimesWithData().empty());
        }

        SECTION("Move partial range") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 4); // 1 + 3 = 4 points
            
            // Verify only points in range were moved
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify source still has data outside the range
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(25)).size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).empty());
        }

        SECTION("Move single time point") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 3);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify only time 20 was removed from source
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).empty());
            REQUIRE(source_data.getAtTime(TimeFrameIndex(25)).size() == 1);
        }
    }

    SECTION("moveTo - specific times vector operations") {
        SECTION("Move multiple specific times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 5); // 2 + 3 = 5 points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 3);
            
            // Verify moved times are cleared from source
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).empty());
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).empty());
            // But other times remain
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(25)).size() == 1);
        }

        SECTION("Move times in non-sequential order") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(25), TimeFrameIndex(10), TimeFrameIndex(15)}; // Non-sequential
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 4); // 1 + 2 + 1 = 4 points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(25)).size() == 1);
            
            // Only time 20 should remain in source
            REQUIRE(source_data.getTimesWithData().size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 3);
        }
    }

    SECTION("Copy/Move to target with existing data") {
        // Add some existing data to target
        std::vector<Point2D<float>> existing_points = {{100.0f, 200.0f}};
        target_data.addPointsAtTime(TimeFrameIndex(10), existing_points);

        SECTION("Copy to existing time adds points") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 2);
            // Should have existing point plus copied points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 copied
        }

        SECTION("Move to existing time adds points") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 2);
            // Should have existing point plus moved points
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 moved
            
            // Source should no longer have points at time 10
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).empty());
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
        
        auto scaled_points = point_data.getAtTime(TimeFrameIndex(10));
        
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
        auto unchanged_points = point_data.getAtTime(TimeFrameIndex(10));
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
        auto unchanged_points = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(unchanged_points[0].x == Catch::Approx(100.0f));
        REQUIRE(unchanged_points[0].y == Catch::Approx(200.0f));
    }
}

TEST_CASE("PointData - Timeframe conversion", "[points][data][timeframe]") {
    PointData point_data;
    
    // Setup test data
    std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    point_data.addPointsAtTime(TimeFrameIndex(10), points);
    point_data.addPointsAtTime(TimeFrameIndex(20), points);
    
    SECTION("Same timeframe returns original data") {
        // Create a single timeframe
        std::vector<int> times = {5, 10, 15, 20, 25};
        auto timeframe = std::make_shared<TimeFrame>(times);
        
        // Query with same source and target timeframe
        auto result = point_data.getAtTime(TimeFrameIndex(10), timeframe.get(), timeframe.get());
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
        REQUIRE(result[0].y == Catch::Approx(200.0f));
    }
    
    SECTION("Different timeframes with conversion") {
        // Create source timeframe (e.g., video frames)
        std::vector<int> video_times = {0, 10, 20, 30, 40};  // Video at 1 Hz
        auto video_timeframe = std::make_shared<TimeFrame>(video_times);
        
        // Create target timeframe (e.g., data sampling at higher rate)
        std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40}; // Data at 2 Hz
        auto data_timeframe = std::make_shared<TimeFrame>(data_times);
        
        // Clear existing data and add data at the correct indices for the target timeframe
        PointData test_point_data;
        
        // Video frame 1 (time=10) should map to data_timeframe index 2 (time=10)
        // Video frame 2 (time=20) should map to data_timeframe index 4 (time=20)
        test_point_data.addPointsAtTime(TimeFrameIndex(2), points);  // At data timeframe index 2 (time=10)
        test_point_data.addPointsAtTime(TimeFrameIndex(4), points);  // At data timeframe index 4 (time=20)
        
        // Query video frame 1 (time=10) which should map to data index 2 (time=10)
        auto result = test_point_data.getAtTime(TimeFrameIndex(1), 
                                                video_timeframe.get(), 
                                                data_timeframe.get());
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
        REQUIRE(result[0].y == Catch::Approx(200.0f));
    }
    
    SECTION("Timeframe conversion with no matching data") {
        // Create timeframes where conversion maps to non-existent data
        std::vector<int> video_times = {0, 5, 10};
        auto video_timeframe = std::make_shared<TimeFrame>(video_times);
        
        std::vector<int> data_times = {0, 3, 7, 15, 25};
        auto data_timeframe = std::make_shared<TimeFrame>(data_times);
        
        // Create a separate point data instance for this test
        PointData test_point_data;
        test_point_data.addPointsAtTime(TimeFrameIndex(3), points);  // At data timeframe index 3 (time=15)
        
        // Query video frame 1 (time=5) which should map to data timeframe index 1 (time=3, closest to 5)
        // Since we only have data at index 3, this should return empty
        auto result = test_point_data.getAtTime(TimeFrameIndex(1), video_timeframe.get(), data_timeframe.get());
        
        // Should return empty since we don't have data at the converted index
        REQUIRE(result.empty());
    }
    
    SECTION("Null timeframe handling") {
        std::vector<int> times = {5, 10, 15, 20, 25};
        auto valid_timeframe = std::make_shared<TimeFrame>(times);
        
        // This should still work since the function should handle null pointers gracefully
        // by falling back to the original behavior
        auto result = point_data.getAtTime(TimeFrameIndex(10), nullptr, valid_timeframe.get());
        
        // The behavior when timeframes are null might depend on getTimeIndexForSeries implementation
        // For now, let's check that it doesn't crash and returns some result
        // The exact behavior would depend on the TimeFrame implementation
    }
}
