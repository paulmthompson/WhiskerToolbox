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

// ============================================================================
// JSON Pipeline Test Scenarios
// ============================================================================

/**
 * @brief Two timesteps with lines that should and shouldn't flip
 * 
 * Line at t=100: (0,0) -> (5,0) -> (10,0) - horizontal, base at left
 * Line at t=200: (0,0) -> (5,5) -> (10,10) - diagonal, base at origin
 * 
 * With reference at (12,0):
 * - t=100: should flip (end closer to reference)
 * - t=200: should flip (end (10,10) is closer to (12,0) than base (0,0))
 */
inline std::shared_ptr<LineData> json_pipeline_two_timesteps() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 5.0f, 10.0f}, {0.0f, 0.0f, 0.0f})
        .withCoords(200, {0.0f, 5.0f, 10.0f}, {0.0f, 5.0f, 10.0f})
        .build();
}

/**
 * @brief Three timesteps with different flip outcomes
 * 
 * Line at t=100: (0,0) -> (10,0) - horizontal, base at origin
 * Line at t=200: (10,0) -> (0,0) - horizontal reversed (base at right)
 * Line at t=300: (5,0) -> (5,8) - vertical, base at bottom
 * 
 * With reference at (12,5):
 * - t=100: should flip (end at (10,0) closer to (12,5))
 *   Base (0,0) to (12,5): sqrt(144+25) = sqrt(169) = 13
 *   End (10,0) to (12,5): sqrt(4+25) = sqrt(29) ≈ 5.4
 * - t=200: should NOT flip (base at (10,0) already closer to (12,5))
 *   Base (10,0) to (12,5): sqrt(4+25) = sqrt(29) ≈ 5.4
 *   End (0,0) to (12,5): sqrt(144+25) = sqrt(169) = 13
 * - t=300: should flip (end at (5,8) closer to (12,5) than base at (5,0))
 *   Base (5,0) to (12,5): sqrt(49+25) = sqrt(74) ≈ 8.6
 *   End (5,8) to (12,5): sqrt(49+9) = sqrt(58) ≈ 7.6
 */
inline std::shared_ptr<LineData> json_pipeline_mixed_outcomes() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 10.0f}, {0.0f, 0.0f})
        .withCoords(200, {10.0f, 0.0f}, {0.0f, 0.0f})
        .withCoords(300, {5.0f, 5.0f}, {0.0f, 8.0f})
        .build();
}

/**
 * @brief Edge cases for JSON pipeline testing
 * 
 * Line at t=100: Single point (5,5) - should not change
 * Line at t=200: Two points (0,0) -> (10,10) - should flip with ref at (15,15)
 */
inline std::shared_ptr<LineData> json_pipeline_edge_cases() {
    return LineDataBuilder()
        .withCoords(100, {5.0f}, {5.0f})
        .withCoords(200, {0.0f, 10.0f}, {0.0f, 10.0f})
        .build();
}

} // namespace line_base_flip_scenarios

#endif // BASE_FLIP_SCENARIOS_HPP
