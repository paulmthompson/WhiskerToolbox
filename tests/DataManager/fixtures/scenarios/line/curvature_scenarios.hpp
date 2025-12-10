#ifndef CURVATURE_SCENARIOS_HPP
#define CURVATURE_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Curvature-related test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line curvature
 * calculation algorithms. These scenarios support testing polynomial fit
 * curvature calculations at various positions along curves.
 */
namespace curvature_scenarios {

// ============================================================================
// Basic Curve Scenarios
// ============================================================================

/**
 * @brief Parabolic curve (y = x^2) - curved line with predictable curvature
 * 
 * Data: x = {0, 1, 2, 3, 4, 5}, y = {0, 1, 4, 9, 16, 25}
 * Time: 100
 * 
 * Expected: Non-zero curvature at any position along the line
 * For a parabola y = x^2, the curvature formula gives k = 2/(1 + 4x^2)^(3/2)
 */
inline std::shared_ptr<LineData> parabola() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})
        .build();
}

/**
 * @brief Straight line (y = x) - zero curvature
 * 
 * Data: x = {0, 1, 2, 3, 4, 5}, y = {0, 1, 2, 3, 4, 5}
 * Time: 100
 * 
 * Expected: Curvature should be very close to zero (< 0.1)
 */
inline std::shared_ptr<LineData> straight_line() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .build();
}

/**
 * @brief Curved line with more points for higher order polynomial fitting
 * 
 * Data: x = {0, 1, 2, 3, 4, 5, 6, 7}, y follows smooth curve
 * Time: 100
 * 
 * Suitable for testing polynomial orders 2-4
 */
inline std::shared_ptr<LineData> smooth_curve() {
    return LineDataBuilder()
        .withCoords(100,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
            {0.0f, 0.5f, 1.8f, 3.9f, 6.8f, 10.5f, 15.0f, 20.3f})
        .build();
}

// ============================================================================
// Edge Case Scenarios
// ============================================================================

/**
 * @brief Empty line data (no lines)
 * 
 * Expected: Empty result, should handle gracefully
 */
inline std::shared_ptr<LineData> empty() {
    return LineDataBuilder().build();
}

/**
 * @brief Line with only 2 points - insufficient for polynomial fit
 * 
 * Data: x = {0, 1}, y = {0, 1}
 * Time: 100
 * 
 * Expected: Should handle gracefully, likely empty result
 */
inline std::shared_ptr<LineData> two_point_line() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f}, {0.0f, 1.0f})
        .build();
}

/**
 * @brief Line with single point - invalid for curvature
 * 
 * Expected: Should handle gracefully, likely empty result
 */
inline std::shared_ptr<LineData> single_point() {
    return LineDataBuilder()
        .withCoords(100, {1.0f}, {1.0f})
        .build();
}

// ============================================================================
// Multiple Timestamp Scenarios
// ============================================================================

/**
 * @brief Multiple curved lines at different timestamps
 * 
 * t=100: Parabola y = x^2
 * t=200: Straight line y = x
 * t=300: Different curve
 * 
 * Useful for testing batch processing across time
 */
inline std::shared_ptr<LineData> multiple_timesteps() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})
        .withCoords(200,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .withCoords(300,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 0.5f, 2.0f, 4.5f, 8.0f, 12.5f})
        .build();
}

// ============================================================================
// V2 JSON Pipeline Scenarios
// ============================================================================

/**
 * @brief Two timesteps: parabola at t=100, straight line at t=200
 * 
 * Used for testing JSON pipeline with expected curvature differences.
 * Useful for V2 transform pipeline testing.
 */
inline std::shared_ptr<LineData> json_pipeline_two_timesteps() {
    return LineDataBuilder()
        // t=100: Parabola y = x^2 (should have positive curvature)
        .withCoords(100,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})
        // t=200: Straight line y = x (should have ~zero curvature)
        .withCoords(200,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .build();
}

/**
 * @brief Multiple curvature scenarios: parabola, straight, and other curve
 * 
 * t=100: Parabola y = x^2 (predictable curvature)
 * t=200: Straight line y = x (near-zero curvature)
 * t=300: Different curve with moderate curvature
 * 
 * Useful for V2 transform pipeline testing with multiple timesteps.
 */
inline std::shared_ptr<LineData> json_pipeline_multiple_curvatures() {
    return LineDataBuilder()
        .withCoords(100,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})
        .withCoords(200,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .withCoords(300,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 0.5f, 2.0f, 4.5f, 8.0f, 12.5f})
        .build();
}

} // namespace curvature_scenarios

#endif // CURVATURE_SCENARIOS_HPP
