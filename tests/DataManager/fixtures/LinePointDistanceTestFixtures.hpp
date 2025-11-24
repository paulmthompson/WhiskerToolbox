#ifndef WHISKERTOOLBOX_LINE_POINT_DISTANCE_TEST_FIXTURES_HPP
#define WHISKERTOOLBOX_LINE_POINT_DISTANCE_TEST_FIXTURES_HPP

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include <memory>
#include <vector>

namespace WhiskerToolbox::Testing {

/**
 * @brief Base fixture for line-point distance testing
 * 
 * Provides shared setup with LineData, PointData, and TimeFrame.
 * Derived fixtures create specific test scenarios.
 */
struct LinePointDistanceFixture {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<PointData> point_data;
    std::shared_ptr<TimeFrame> time_frame;
    
    LinePointDistanceFixture() {
        time_frame = std::make_shared<TimeFrame>();
        line_data = std::make_shared<LineData>();
        point_data = std::make_shared<PointData>();
        
        line_data->setTimeFrame(time_frame);
        point_data->setTimeFrame(time_frame);
    }
};

/**
 * @brief Horizontal line with point above it
 * Line: (0,0) to (10,0), Point: (5,5) at t=10
 * Expected distance: 5.0
 */
struct HorizontalLineWithPointAbove : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{10};
    static constexpr float expected_distance = 5.0f;
    
    HorizontalLineWithPointAbove() {
        // Horizontal line at y=0
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->emplaceAtTime(timestamp, line_x, line_y);
        
        // Point above the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(timestamp, points, NotifyObservers::No);
    }
};

/**
 * @brief Vertical line with multiple points at different distances
 * Line: (5,0) to (5,10), Points: (0,5), (8,5), (5,15), (6,8) at t=20
 * Expected minimum distance: 1.0 (from point at (6,8))
 */
struct VerticalLineWithMultiplePoints : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{20};
    static constexpr float expected_distance = 1.0f;
    
    VerticalLineWithMultiplePoints() {
        // Vertical line at x=5
        std::vector<float> line_x = {5.0f, 5.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        line_data->emplaceAtTime(timestamp, line_x, line_y);
        
        // Multiple points at different distances
        std::vector<Point2D<float>> points = {
            Point2D<float>{0.0f, 5.0f},    // 5 units away
            Point2D<float>{8.0f, 5.0f},    // 3 units away
            Point2D<float>{5.0f, 15.0f},   // 5 units away
            Point2D<float>{6.0f, 8.0f}     // 1 unit away (minimum)
        };
        point_data->addAtTime(timestamp, points, NotifyObservers::No);
    }
};

/**
 * @brief Multiple timesteps with different line-point pairs
 * t=30: Horizontal line (0,0)-(10,0) with point (5,2), distance = 2.0
 * t=40: Vertical line (0,0)-(0,10) with point (3,5), distance = 3.0
 * t=50: Point only (no line) - should be skipped
 */
struct MultipleTimesteps : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp1{30};
    static constexpr TimeFrameIndex timestamp2{40};
    static constexpr TimeFrameIndex timestamp3{50};
    static constexpr float expected_distance1 = 2.0f;
    static constexpr float expected_distance2 = 3.0f;
    static constexpr size_t expected_num_results = 2;
    
    MultipleTimesteps() {
        // Timestamp 30: horizontal line
        line_data->emplaceAtTime(timestamp1, 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        point_data->addAtTime(timestamp1, 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 2.0f}}, 
            NotifyObservers::No);
        
        // Timestamp 40: vertical line
        line_data->emplaceAtTime(timestamp2, 
            std::vector<float>{0.0f, 0.0f}, 
            std::vector<float>{0.0f, 10.0f});
        point_data->addAtTime(timestamp2, 
            std::vector<Point2D<float>>{Point2D<float>{3.0f, 5.0f}}, 
            NotifyObservers::No);
        
        // Timestamp 50: point only (no line)
        point_data->addAtTime(timestamp3, 
            std::vector<Point2D<float>>{Point2D<float>{1.0f, 1.0f}}, 
            NotifyObservers::No);
    }
};

/**
 * @brief Data with coordinate scaling between different image sizes
 * Line image size: 100x100, Point image size: 50x50
 * Line: (0,0) to (100,0) at t=60
 * Point: (25,10) in 50x50 space -> (50,20) in 100x100 space
 * Expected distance: 20.0
 */
struct CoordinateScaling : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{60};
    static constexpr float expected_distance = 20.0f;
    
    CoordinateScaling() {
        // Different image sizes for scaling
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50});
        
        // Line in 100x100 space
        line_data->emplaceAtTime(timestamp, 
            std::vector<float>{0.0f, 100.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        // Point in 50x50 space (will be scaled to 100x100)
        point_data->addAtTime(timestamp, 
            std::vector<Point2D<float>>{Point2D<float>{25.0f, 10.0f}}, 
            NotifyObservers::No);
    }
};

/**
 * @brief Point exactly on diagonal line
 * Line: (0,0) to (10,10), Point: (5,5) at t=70
 * Expected distance: 0.0
 */
struct PointOnLine : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{70};
    static constexpr float expected_distance = 0.0f;
    
    PointOnLine() {
        // Diagonal line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        line_data->emplaceAtTime(timestamp, line_x, line_y);
        
        // Point exactly on the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(timestamp, points, NotifyObservers::No);
    }
};

/**
 * @brief Empty line data (edge case)
 * Only point data, no line data
 */
struct EmptyLineData : LinePointDistanceFixture {
    static constexpr size_t expected_num_results = 0;
    
    EmptyLineData() {
        // Only point data, no line data
        point_data->addAtTime(TimeFrameIndex(10), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
    }
};

/**
 * @brief Empty point data (edge case)
 * Only line data, no point data
 */
struct EmptyPointData : LinePointDistanceFixture {
    static constexpr size_t expected_num_results = 0;
    
    EmptyPointData() {
        // Only line data, no point data
        line_data->emplaceAtTime(TimeFrameIndex(10), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
    }
};

/**
 * @brief No matching timestamps (edge case)
 * Line at t=20, point at t=30
 */
struct NoMatchingTimestamps : LinePointDistanceFixture {
    static constexpr size_t expected_num_results = 0;
    
    NoMatchingTimestamps() {
        // Line at t=20
        line_data->emplaceAtTime(TimeFrameIndex(20), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        // Point at t=30 (different timestamp)
        point_data->addAtTime(TimeFrameIndex(30), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
    }
};

/**
 * @brief Line with only one point (invalid)
 * Line needs at least 2 points to form segments
 */
struct InvalidLineOnePoint : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{40};
    static constexpr size_t expected_num_results = 0;
    
    InvalidLineOnePoint() {
        // "Line" with only one point
        std::vector<float> line_x = {5.0f};
        std::vector<float> line_y = {5.0f};
        line_data->emplaceAtTime(timestamp, line_x, line_y);
        
        // Point
        std::vector<Point2D<float>> points = {Point2D<float>{10.0f, 10.0f}};
        point_data->addAtTime(timestamp, points, NotifyObservers::No);
    }
};

/**
 * @brief Invalid image sizes
 * Point data has invalid image size, should fall back to no scaling
 */
struct InvalidImageSizes : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{50};
    static constexpr float expected_distance = 5.0f; // No scaling applied
    
    InvalidImageSizes() {
        // Set invalid image sizes
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{-1, -1}); // Invalid
        
        // Line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->emplaceAtTime(timestamp, line_x, line_y);
        
        // Point - should use original coordinates without scaling
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(timestamp, points, NotifyObservers::No);
    }
};

/**
 * @brief JSON pipeline test fixture - Two timesteps
 * t=100: Horizontal line (0,0)-(10,0) with point (5,5), distance = 5.0
 * t=200: Vertical line (5,0)-(5,10) with point (8,5), distance = 3.0
 */
struct JsonPipelineTwoTimesteps : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp1{100};
    static constexpr TimeFrameIndex timestamp2{200};
    static constexpr float expected_distance1 = 5.0f;
    static constexpr float expected_distance2 = 3.0f;
    static constexpr size_t expected_num_results = 2;
    
    JsonPipelineTwoTimesteps() {
        // Timestamp 100: horizontal line
        line_data->emplaceAtTime(timestamp1,
            std::vector<float>{0.0f, 10.0f},
            std::vector<float>{0.0f, 0.0f});
        point_data->addAtTime(timestamp1,
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}},
            NotifyObservers::No);
        
        // Timestamp 200: vertical line
        line_data->emplaceAtTime(timestamp2,
            std::vector<float>{5.0f, 5.0f},
            std::vector<float>{0.0f, 10.0f});
        point_data->addAtTime(timestamp2,
            std::vector<Point2D<float>>{Point2D<float>{8.0f, 5.0f}},
            NotifyObservers::No);
    }
};

/**
 * @brief JSON pipeline test with scaling
 * Line: 100x100 image, (0,0) to (100,0) at t=300
 * Point: 50x50 image, (25,10) -> scales to (50,20) in line space
 * Expected distance: 20.0
 */
struct JsonPipelineScaling : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{300};
    static constexpr float expected_distance = 20.0f;
    
    JsonPipelineScaling() {
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50});
        
        line_data->emplaceAtTime(timestamp,
            std::vector<float>{0.0f, 100.0f},
            std::vector<float>{0.0f, 0.0f});
        point_data->addAtTime(timestamp,
            std::vector<Point2D<float>>{Point2D<float>{25.0f, 10.0f}},
            NotifyObservers::No);
    }
};

/**
 * @brief JSON pipeline test - point on line
 * Diagonal line (0,0) to (10,10) with point (5,5) at t=400
 * Expected distance: 0.0
 */
struct JsonPipelinePointOnLine : LinePointDistanceFixture {
    static constexpr TimeFrameIndex timestamp{400};
    static constexpr float expected_distance = 0.0f;
    
    JsonPipelinePointOnLine() {
        line_data->emplaceAtTime(timestamp,
            std::vector<float>{0.0f, 10.0f},
            std::vector<float>{0.0f, 10.0f});
        point_data->addAtTime(timestamp,
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}},
            NotifyObservers::No);
    }
};

} // namespace WhiskerToolbox::Testing

#endif // WHISKERTOOLBOX_LINE_POINT_DISTANCE_TEST_FIXTURES_HPP
