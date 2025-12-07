#ifndef LINE_POINT_DISTANCE_TEST_FIXTURE_HPP
#define LINE_POINT_DISTANCE_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <map>
#include <string>
#include <vector>

/**
 * @brief Test fixture for LineMinPointDist transform tests
 * 
 * This fixture provides reusable test data for both V1 and V2 tests.
 * Each test scenario is stored with a descriptive key that describes the
 * data pattern, not the expected result.
 * 
 * The fixture creates pairs of LineData and PointData for testing distance calculations.
 * Expected results are documented in comments but not encoded in the fixture.
 */
class LinePointDistanceTestFixture {
protected:
    LinePointDistanceTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~LinePointDistanceTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    // Map of named test line data - primary inputs
    std::map<std::string, std::shared_ptr<LineData>> m_line_data;
    
    // Map of named test point data - secondary inputs
    std::map<std::string, std::shared_ptr<PointData>> m_point_data;

private:
    void populateTestData() {
        // ========================================================================
        // Core Functionality Test Data
        // ========================================================================

        // Scenario: Horizontal line with point above
        // Line: (0,0) to (10,0) at t=10
        // Point: (5,5) at t=10
        // Expected distance: 5.0
        createLinePair("horizontal_line_point_above",
            /*timestamp=*/10,
            /*line_x=*/{0.0f, 10.0f},
            /*line_y=*/{0.0f, 0.0f},
            /*points=*/{{5.0f, 5.0f}});

        // Scenario: Vertical line with multiple points at different distances
        // Line: (5,0) to (5,10) at t=20
        // Points: (0,5), (8,5), (5,15), (6,8) at t=20
        // Expected minimum distance: 1.0 (from point at (6,8))
        createLinePair("vertical_line_multiple_points",
            /*timestamp=*/20,
            /*line_x=*/{5.0f, 5.0f},
            /*line_y=*/{0.0f, 10.0f},
            /*points=*/{{0.0f, 5.0f}, {8.0f, 5.0f}, {5.0f, 15.0f}, {6.0f, 8.0f}});

        // Scenario: Diagonal line with point exactly on it
        // Line: (0,0) to (10,10) at t=70
        // Point: (5,5) at t=70
        // Expected distance: 0.0
        createLinePair("point_on_line",
            /*timestamp=*/70,
            /*line_x=*/{0.0f, 10.0f},
            /*line_y=*/{0.0f, 10.0f},
            /*points=*/{{5.0f, 5.0f}});

        // Scenario: Multiple timesteps with different line-point pairs
        // t=30: Horizontal line (0,0)-(10,0) with point (5,2), distance = 2.0
        // t=40: Vertical line (0,0)-(0,10) with point (3,5), distance = 3.0
        // t=50: Point only (no line) - should be skipped
        createMultiTimestepData("multiple_timesteps");

        // Scenario: Coordinate scaling between different image sizes
        // Line image size: 100x100, Point image size: 50x50
        // Line: (0,0) to (100,0) at t=60
        // Point: (25,10) in 50x50 space -> (50,20) in 100x100 space
        // Expected distance: 20.0
        createScalingData("coordinate_scaling");

        // ========================================================================
        // Edge Cases Test Data
        // ========================================================================

        // Scenario: Empty line data (only points)
        createEmptyLineData("empty_line_data");

        // Scenario: Empty point data (only lines)
        createEmptyPointData("empty_point_data");

        // Scenario: No matching timestamps between line and point data
        // Line at t=20, point at t=30
        createNoMatchingTimestamps("no_matching_timestamps");

        // Scenario: Line with only one point (invalid)
        // Line needs at least 2 points to form segments
        createInvalidLineOnePoint("invalid_line_one_point");

        // Scenario: Invalid image sizes (should fall back to no scaling)
        createInvalidImageSizes("invalid_image_sizes");

        // ========================================================================
        // JSON Pipeline Test Data
        // ========================================================================

        // Scenario: Two timesteps for JSON pipeline test
        // t=100: Horizontal line (0,0)-(10,0) with point (5,5), distance = 5.0
        // t=200: Vertical line (5,0)-(5,10) with point (8,5), distance = 3.0
        createJsonPipelineTwoTimesteps("json_pipeline_two_timesteps");

        // Scenario: Scaling for JSON pipeline test
        // Line: 100x100 image, (0,0) to (100,0) at t=300
        // Point: 50x50 image, (25,10) -> scales to (50,20) in line space
        // Expected distance: 20.0
        createJsonPipelineScaling("json_pipeline_scaling");

        // Scenario: Point on line for JSON pipeline test
        // Diagonal line (0,0) to (10,10) with point (5,5) at t=400
        // Expected distance: 0.0
        createJsonPipelinePointOnLine("json_pipeline_point_on_line");
    }

    void createLinePair(const std::string& key, 
                        int timestamp,
                        const std::vector<float>& line_x,
                        const std::vector<float>& line_y,
                        const std::vector<Point2D<float>>& points) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        TimeFrameIndex time_idx(timestamp);
        
        // Add line at timestamp
        line_data->emplaceAtTime(time_idx, 
            std::vector<float>(line_x), 
            std::vector<float>(line_y));
        
        // Add points at timestamp
        point_data->addAtTime(time_idx, 
            std::vector<Point2D<float>>(points), 
            NotifyObservers::No);
        
        // Store in maps for direct access
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        // Store in DataManager with distinct keys
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createMultiTimestepData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Timestamp 30: horizontal line with point
        line_data->emplaceAtTime(TimeFrameIndex(30), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        point_data->addAtTime(TimeFrameIndex(30), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 2.0f}}, 
            NotifyObservers::No);
        
        // Timestamp 40: vertical line with point
        line_data->emplaceAtTime(TimeFrameIndex(40), 
            std::vector<float>{0.0f, 0.0f}, 
            std::vector<float>{0.0f, 10.0f});
        point_data->addAtTime(TimeFrameIndex(40), 
            std::vector<Point2D<float>>{Point2D<float>{3.0f, 5.0f}}, 
            NotifyObservers::No);
        
        // Timestamp 50: point only (no line) - will be skipped in processing
        point_data->addAtTime(TimeFrameIndex(50), 
            std::vector<Point2D<float>>{Point2D<float>{1.0f, 1.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createScalingData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Different image sizes for scaling
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50});
        
        // Line in 100x100 space
        line_data->emplaceAtTime(TimeFrameIndex(60), 
            std::vector<float>{0.0f, 100.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        // Point in 50x50 space (will be scaled to 100x100)
        point_data->addAtTime(TimeFrameIndex(60), 
            std::vector<Point2D<float>>{Point2D<float>{25.0f, 10.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createEmptyLineData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Only point data, no line data
        point_data->addAtTime(TimeFrameIndex(10), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createEmptyPointData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Only line data, no point data
        line_data->emplaceAtTime(TimeFrameIndex(10), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createNoMatchingTimestamps(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Line at t=20
        line_data->emplaceAtTime(TimeFrameIndex(20), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        // Point at t=30 (different timestamp)
        point_data->addAtTime(TimeFrameIndex(30), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createInvalidLineOnePoint(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // "Line" with only one point (invalid)
        line_data->emplaceAtTime(TimeFrameIndex(40), 
            std::vector<float>{5.0f}, 
            std::vector<float>{5.0f});
        
        // Point
        point_data->addAtTime(TimeFrameIndex(40), 
            std::vector<Point2D<float>>{Point2D<float>{10.0f, 10.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createInvalidImageSizes(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Set invalid image sizes
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{-1, -1}); // Invalid
        
        // Line
        line_data->emplaceAtTime(TimeFrameIndex(50), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        // Point - should use original coordinates without scaling
        point_data->addAtTime(TimeFrameIndex(50), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createJsonPipelineTwoTimesteps(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Timestamp 100: horizontal line with point above
        line_data->emplaceAtTime(TimeFrameIndex(100),
            std::vector<float>{0.0f, 10.0f},
            std::vector<float>{0.0f, 0.0f});
        point_data->addAtTime(TimeFrameIndex(100),
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}},
            NotifyObservers::No);
        
        // Timestamp 200: vertical line with point
        line_data->emplaceAtTime(TimeFrameIndex(200),
            std::vector<float>{5.0f, 5.0f},
            std::vector<float>{0.0f, 10.0f});
        point_data->addAtTime(TimeFrameIndex(200),
            std::vector<Point2D<float>>{Point2D<float>{8.0f, 5.0f}},
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createJsonPipelineScaling(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50});
        
        // Line at t=300
        line_data->emplaceAtTime(TimeFrameIndex(300),
            std::vector<float>{0.0f, 100.0f},
            std::vector<float>{0.0f, 0.0f});
        
        // Point in 50x50 space
        point_data->addAtTime(TimeFrameIndex(300),
            std::vector<Point2D<float>>{Point2D<float>{25.0f, 10.0f}},
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    void createJsonPipelinePointOnLine(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        auto point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(m_time_frame);
        point_data->setTimeFrame(m_time_frame);
        
        // Diagonal line at t=400
        line_data->emplaceAtTime(TimeFrameIndex(400),
            std::vector<float>{0.0f, 10.0f},
            std::vector<float>{0.0f, 10.0f});
        
        // Point exactly on the line
        point_data->addAtTime(TimeFrameIndex(400),
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}},
            NotifyObservers::No);
        
        m_line_data[key] = line_data;
        m_point_data[key] = point_data;
        
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
        m_data_manager->setData(key + "_point", point_data, TimeKey("default"));
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif // LINE_POINT_DISTANCE_TEST_FIXTURE_HPP
