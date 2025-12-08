#ifndef DISTANCE_SCENARIOS_HPP
#define DISTANCE_SCENARIOS_HPP

#include "builders/LineDataBuilder.hpp"
#include "builders/PointDataBuilder.hpp"
#include <memory>
#include <utility>

/**
 * @brief Line-to-point distance calculation test scenarios
 * 
 * This namespace contains pre-configured test data for line-to-point
 * distance calculation algorithms. These scenarios test various edge cases
 * and common patterns for both V1 and V2 implementations.
 * 
 * Each scenario returns a pair of (LineData, PointData) for testing.
 */
namespace line_distance_scenarios {

/**
 * @brief Horizontal line with point above
 * 
 * Line: (0,0) to (10,0) at t=10
 * Point: (5,5) at t=10
 * 
 * Expected distance: 5.0 (perpendicular distance from point to line)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
horizontal_line_point_above() {
    auto line_data = LineDataBuilder()
        .withHorizontal(10, 0.0f, 10.0f, 0.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(10, 5.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Vertical line with multiple points at different distances
 * 
 * Line: (5,0) to (5,10) at t=20
 * Points: (0,5), (8,5), (5,15), (6,8) at t=20
 * 
 * Expected minimum distance: 1.0 (from point at (6,8) to line at x=5)
 * Individual distances: 5.0, 3.0, 5.0, 1.0
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
vertical_line_multiple_points() {
    auto line_data = LineDataBuilder()
        .withVertical(20, 5.0f, 0.0f, 10.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoints(20, {
            {0.0f, 5.0f},
            {8.0f, 5.0f},
            {5.0f, 15.0f},
            {6.0f, 8.0f}
        })
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Point directly on the line has zero distance
 * 
 * Line: (0,0) to (10,10) at t=70 (diagonal)
 * Point: (5,5) at t=70 (on the line)
 * 
 * Expected distance: 0.0
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
point_on_line() {
    auto line_data = LineDataBuilder()
        .withDiagonal(70, 0.0f, 0.0f, 10.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(70, 5.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Multiple timesteps with different line-point pairs
 * 
 * t=30: Horizontal line (0,0)-(10,0) with point (5,2), distance = 2.0
 * t=40: Vertical line (0,0)-(0,10) with point (3,5), distance = 3.0
 * t=50: Point only (no line) - should be skipped in processing
 * 
 * Expected results: 2 distance values (t=30: 2.0, t=40: 3.0)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
multiple_timesteps() {
    auto line_data = LineDataBuilder()
        .withHorizontal(30, 0.0f, 10.0f, 0.0f, 2)
        .withVertical(40, 0.0f, 0.0f, 10.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(30, 5.0f, 2.0f)
        .withPoint(40, 3.0f, 5.0f)
        .withPoint(50, 1.0f, 1.0f)  // No line at this time
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Coordinate scaling between different image sizes
 * 
 * Line image size: 100x100
 * Point image size: 50x50
 * Line: (0,0) to (100,0) at t=60
 * Point: (25,10) in 50x50 space -> (50,20) in 100x100 space
 * 
 * Expected distance: 20.0 (after scaling)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
coordinate_scaling() {
    auto line_data = LineDataBuilder()
        .withHorizontal(60, 0.0f, 100.0f, 0.0f, 2)
        .withImageSize(100, 100)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(60, 25.0f, 10.0f)
        .withImageSize(50, 50)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Empty line data (only points, no lines)
 * 
 * No lines
 * Point: (5,5) at t=10
 * 
 * Expected: Empty result (no distance values)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
empty_line_data() {
    auto line_data = LineDataBuilder().build();
    
    auto point_data = PointDataBuilder()
        .withPoint(10, 5.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Empty point data (only lines, no points)
 * 
 * Line: (0,0) to (10,0) at t=10
 * No points
 * 
 * Expected: Empty result (no distance values)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
empty_point_data() {
    auto line_data = LineDataBuilder()
        .withHorizontal(10, 0.0f, 10.0f, 0.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder().build();
    
    return {line_data, point_data};
}

/**
 * @brief No matching timestamps between line and point data
 * 
 * Line at t=20
 * Point at t=30 (different timestamp)
 * 
 * Expected: Empty result (no distance values)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
no_matching_timestamps() {
    auto line_data = LineDataBuilder()
        .withHorizontal(20, 0.0f, 10.0f, 0.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(30, 5.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Line with only one point (invalid)
 * 
 * "Line" with only one point at (5,5) at t=40 (invalid - needs at least 2 points)
 * Point: (10,10) at t=40
 * 
 * V1 Expected: Empty result (invalid line produces no results)
 * V2 Expected: Infinity (invalid line returns infinity)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
invalid_line_one_point() {
    auto line_data = LineDataBuilder()
        .withCoords(40, {5.0f}, {5.0f})  // Only one point
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(40, 10.0f, 10.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Invalid image sizes (should fall back to no scaling)
 * 
 * Line image size: 100x100
 * Point image size: -1x-1 (invalid)
 * Line: (0,0) to (10,0) at t=50
 * Point: (5,5) at t=50
 * 
 * Expected distance: 5.0 (no scaling applied due to invalid point image size)
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
invalid_image_sizes() {
    auto line_data = LineDataBuilder()
        .withHorizontal(50, 0.0f, 10.0f, 0.0f, 2)
        .withImageSize(100, 100)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(50, 5.0f, 5.0f)
        .build();
    // Note: Can't set negative image size via builder, so this tests default behavior
    
    return {line_data, point_data};
}

/**
 * @brief Two timesteps for JSON pipeline test
 * 
 * t=100: Horizontal line (0,0)-(10,0) with point (5,5), distance = 5.0
 * t=200: Vertical line (5,0)-(5,10) with point (8,5), distance = 3.0
 * 
 * Expected results: {100: 5.0, 200: 3.0}
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
json_pipeline_two_timesteps() {
    auto line_data = LineDataBuilder()
        .withHorizontal(100, 0.0f, 10.0f, 0.0f, 2)
        .withVertical(200, 5.0f, 0.0f, 10.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(100, 5.0f, 5.0f)
        .withPoint(200, 8.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Scaling for JSON pipeline test
 * 
 * Line: 100x100 image, (0,0) to (100,0) at t=300
 * Point: 50x50 image, (25,10) -> scales to (50,20) in line space
 * 
 * Expected distance: 20.0
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
json_pipeline_scaling() {
    auto line_data = LineDataBuilder()
        .withHorizontal(300, 0.0f, 100.0f, 0.0f, 2)
        .withImageSize(100, 100)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(300, 25.0f, 10.0f)
        .withImageSize(50, 50)
        .build();
    
    return {line_data, point_data};
}

/**
 * @brief Point on line for JSON pipeline test
 * 
 * Diagonal line (0,0) to (10,10) with point (5,5) at t=400
 * 
 * Expected distance: 0.0
 */
inline std::pair<std::shared_ptr<LineData>, std::shared_ptr<PointData>> 
json_pipeline_point_on_line() {
    auto line_data = LineDataBuilder()
        .withDiagonal(400, 0.0f, 0.0f, 10.0f, 2)
        .build();
    
    auto point_data = PointDataBuilder()
        .withPoint(400, 5.0f, 5.0f)
        .build();
    
    return {line_data, point_data};
}

} // namespace line_distance_scenarios

#endif // DISTANCE_SCENARIOS_HPP
