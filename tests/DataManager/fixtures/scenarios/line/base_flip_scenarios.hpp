#ifndef BASE_FLIP_SCENARIOS_HPP
#define BASE_FLIP_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Line base flip test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line base flip
 * algorithms. These scenarios test flipping line orientation based on
 * reference point proximity.
 */
namespace line_base_flip_scenarios {

/**
 * @brief Simple horizontal line from (0,0) to (10,0)
 * 
 * Line at t=0: (0,0) -> (5,0) -> (10,0)
 * 
 * Expected: Base at (0,0), end at (10,0)
 * With reference at (12,0): should flip (end closer to reference)
 * With reference at (-2,0): should NOT flip (base closer to reference)
 */
inline std::shared_ptr<LineData> simple_horizontal_line() {
    return LineDataBuilder()
        .withCoords(0, {0.0f, 5.0f, 10.0f}, {0.0f, 0.0f, 0.0f})
        .build();
}

/**
 * @brief Single point line (edge case)
 * 
 * Line at t=0: single point at (5,5)
 * 
 * Expected: Should not flip (cannot determine orientation)
 */
inline std::shared_ptr<LineData> single_point_line() {
    return LineDataBuilder()
        .withCoords(0, {5.0f}, {5.0f})
        .build();
}

/**
 * @brief Two horizontal lines at different frames
 * 
 * Line at t=0: (0,0) -> (10,0)
 * Line at t=1: (0,10) -> (10,10)
 * 
 * Expected: Both should be processed independently
 */
inline std::shared_ptr<LineData> multiple_frames() {
    return LineDataBuilder()
        .withCoords(0, {0.0f, 10.0f}, {0.0f, 0.0f})
        .withCoords(1, {0.0f, 10.0f}, {10.0f, 10.0f})
        .build();
}

/**
 * @brief Vertical line from (5,0) to (5,10)
 * 
 * Line at t=0: (5,0) -> (5,5) -> (5,10)
 * 
 * Expected: Test vertical line flipping behavior
 */
inline std::shared_ptr<LineData> vertical_line() {
    return LineDataBuilder()
        .withCoords(0, {5.0f, 5.0f, 5.0f}, {0.0f, 5.0f, 10.0f})
        .build();
}

/**
 * @brief Diagonal line from (0,0) to (10,10)
 * 
 * Line at t=0: (0,0) -> (5,5) -> (10,10)
 * 
 * Expected: Test diagonal line flipping behavior
 */
inline std::shared_ptr<LineData> diagonal_line() {
    return LineDataBuilder()
        .withCoords(0, {0.0f, 5.0f, 10.0f}, {0.0f, 5.0f, 10.0f})
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

} // namespace line_base_flip_scenarios

#endif // BASE_FLIP_SCENARIOS_HPP
