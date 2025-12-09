#ifndef CLIP_SCENARIOS_HPP
#define CLIP_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Line clipping test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line clipping
 * algorithms. These scenarios test clipping lines based on reference
 * line intersections.
 */
namespace line_clip_scenarios {

/**
 * @brief Horizontal line from (0,2) to (4,2)
 * 
 * Line at t=100: 5 points along y=2
 * 
 * Expected: Use with vertical reference line to test clipping
 */
inline std::shared_ptr<LineData> horizontal_line() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f, 4.0f}, {2.0f, 2.0f, 2.0f, 2.0f, 2.0f})
        .build();
}

/**
 * @brief Vertical reference line at x=2.5
 * 
 * Line at t=0: vertical line from (2.5,0) to (2.5,5)
 * 
 * Expected: Use as reference to clip horizontal lines at x=2.5
 */
inline std::shared_ptr<LineData> vertical_reference_at_2_5() {
    return LineDataBuilder()
        .withCoords(0, {2.5f, 2.5f}, {0.0f, 5.0f})
        .build();
}

/**
 * @brief Vertical reference line at x=2.0
 * 
 * Line at t=0: vertical line from (2.0,0) to (2.0,5)
 * 
 * Expected: Use as reference to clip lines at x=2.0
 */
inline std::shared_ptr<LineData> vertical_reference_at_2_0() {
    return LineDataBuilder()
        .withCoords(0, {2.0f, 2.0f}, {0.0f, 5.0f})
        .build();
}

/**
 * @brief Vertical reference line at x=5.0 (no intersection with standard test lines)
 * 
 * Line at t=0: vertical line from (5.0,0) to (5.0,5)
 * 
 * Expected: No intersection with lines ending at x=3 or x=4
 */
inline std::shared_ptr<LineData> vertical_reference_no_intersection() {
    return LineDataBuilder()
        .withCoords(0, {5.0f, 5.0f}, {0.0f, 5.0f})
        .build();
}

/**
 * @brief Vertical reference line at x=1.0
 * 
 * Line at t=0: vertical line from (1.0,0) to (1.0,5)
 * 
 * Expected: Use as reference to clip lines at x=1.0
 */
inline std::shared_ptr<LineData> vertical_reference_at_1_0() {
    return LineDataBuilder()
        .withCoords(0, {1.0f, 1.0f}, {0.0f, 5.0f})
        .build();
}

/**
 * @brief Horizontal line for multiple time frame tests
 * 
 * t=100: horizontal line at y=1
 * t=200: diagonal line from (0,0) to (3,3)
 * 
 * Expected: Both frames should be clipped independently
 */
inline std::shared_ptr<LineData> multiple_time_frames() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f}, {1.0f, 1.0f, 1.0f, 1.0f})
        .withCoords(200, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Horizontal line (shorter) for basic tests
 * 
 * Line at t=100: 4 points from (0,2) to (3,2)
 * 
 * Expected: Use for no-intersection tests
 */
inline std::shared_ptr<LineData> horizontal_line_short() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f}, {2.0f, 2.0f, 2.0f, 2.0f})
        .build();
}

/**
 * @brief Parallel horizontal reference line at y=4
 * 
 * Line at t=0: horizontal line from (0,4) to (3,4)
 * 
 * Expected: No intersection with lines at y=2 (parallel)
 */
inline std::shared_ptr<LineData> horizontal_reference_parallel() {
    return LineDataBuilder()
        .withCoords(0, {0.0f, 1.0f, 2.0f, 3.0f}, {4.0f, 4.0f, 4.0f, 4.0f})
        .build();
}

/**
 * @brief Single point line (edge case)
 * 
 * Line at t=100: single point at (1,2)
 * 
 * Expected: Too short to clip, should be skipped
 */
inline std::shared_ptr<LineData> single_point_line() {
    return LineDataBuilder()
        .withCoords(100, {1.0f}, {2.0f})
        .build();
}

/**
 * @brief Empty line data
 * 
 * Expected: No lines to process
 */
inline std::shared_ptr<LineData> empty_line_data() {
    return LineDataBuilder().build();
}

/**
 * @brief Diagonal line from (0,0) to (3,3)
 * 
 * Line at t=200: 4 points along diagonal
 * 
 * Expected: Intersects vertical references at various points
 */
inline std::shared_ptr<LineData> diagonal_line() {
    return LineDataBuilder()
        .withCoords(200, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

} // namespace line_clip_scenarios

#endif // CLIP_SCENARIOS_HPP
