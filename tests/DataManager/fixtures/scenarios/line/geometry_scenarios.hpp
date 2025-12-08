#ifndef GEOMETRY_SCENARIOS_HPP
#define GEOMETRY_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Geometry-related test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line angle
 * calculation algorithms. These scenarios are extracted from
 * LineAngleTestFixture to enable reuse across v1 and v2 transform tests.
 */
namespace line_scenarios {

// ============================================================================
// Core Functionality Scenarios
// ============================================================================

/**
 * @brief Horizontal line pointing right
 * 
 * Line: (0,1) to (3,1) at t=10
 * Expected angle at position 0.33: 0 degrees (horizontal)
 */
inline std::shared_ptr<LineData> horizontal_line() {
    return LineDataBuilder()
        .withCoords(10, {0.0f, 1.0f, 2.0f, 3.0f}, {1.0f, 1.0f, 1.0f, 1.0f})
        .build();
}

/**
 * @brief Vertical line pointing up
 * 
 * Line: (1,0) to (1,3) at t=20
 * Expected angle at position 0.25: 90 degrees (vertical)
 */
inline std::shared_ptr<LineData> vertical_line() {
    return LineDataBuilder()
        .withCoords(20, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Diagonal line at 45 degrees
 * 
 * Line: (0,0) to (3,3) at t=30
 * Expected angle at position 0.5: 45 degrees
 */
inline std::shared_ptr<LineData> diagonal_45_degrees() {
    return LineDataBuilder()
        .withCoords(30, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Multiple lines at different timestamps
 * 
 * t=40: horizontal line (0 degrees)
 * t=50: vertical line (90 degrees)
 * t=60: 45-degree line (45 degrees)
 */
inline std::shared_ptr<LineData> multiple_timesteps() {
    return LineDataBuilder()
        .withCoords(40, {0.0f, 1.0f, 2.0f}, {1.0f, 1.0f, 1.0f})
        .withCoords(50, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 2.0f})
        .withCoords(60, {0.0f, 1.0f, 2.0f}, {0.0f, 1.0f, 2.0f})
        .build();
}

/**
 * @brief Parabolic curve (y = x^2)
 * 
 * Line: points on a parabola at t=70
 * Expected: polynomial fit should capture curvature
 */
inline std::shared_ptr<LineData> parabola() {
    return LineDataBuilder()
        .withCoords(70, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})
        .build();
}

/**
 * @brief Smooth curve for polynomial order testing
 * 
 * Line: smooth curve at t=80
 */
inline std::shared_ptr<LineData> smooth_curve() {
    return LineDataBuilder()
        .withCoords(80,
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
            {0.0f, 0.5f, 1.8f, 3.9f, 6.8f, 10.5f, 15.0f, 20.3f})
        .build();
}

/**
 * @brief Simple horizontal line at origin
 * 
 * Line: (0,0) to (3,0) at t=100
 * Expected angle: 0 degrees
 */
inline std::shared_ptr<LineData> horizontal_at_origin() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 0.0f})
        .build();
}

// ============================================================================
// Reference Vector Test Scenarios
// ============================================================================

/**
 * @brief 45-degree line for reference vector tests
 * 
 * Line: (0,0) to (3,3) at t=110
 * Used for testing different reference vectors
 */
inline std::shared_ptr<LineData> diagonal_for_reference() {
    return LineDataBuilder()
        .withCoords(110, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Horizontal line for 45-degree reference test
 * 
 * Line: (0,1) to (3,1) at t=130
 */
inline std::shared_ptr<LineData> horizontal_for_reference() {
    return LineDataBuilder()
        .withCoords(130, {0.0f, 1.0f, 2.0f, 3.0f}, {1.0f, 1.0f, 1.0f, 1.0f})
        .build();
}

/**
 * @brief Parabolic curve for polynomial reference tests
 * 
 * Line: y = x^2 at t=140
 */
inline std::shared_ptr<LineData> parabola_for_reference() {
    return LineDataBuilder()
        .withCoords(140, {0.0f, 1.0f, 2.0f, 3.0f, 4.0f}, {0.0f, 1.0f, 4.0f, 9.0f, 16.0f})
        .build();
}

// ============================================================================
// Edge Cases Test Scenarios
// ============================================================================

/**
 * @brief Line with only one point (invalid)
 * 
 * Expected: Empty result or NaN
 */
inline std::shared_ptr<LineData> single_point_line() {
    return LineDataBuilder()
        .withCoords(10, {1.0f}, {1.0f})
        .build();
}

/**
 * @brief Two-point diagonal line
 * 
 * Line: (0,0) to (3,3) at t=20
 * Expected angle: 45 degrees
 */
inline std::shared_ptr<LineData> two_point_diagonal() {
    return LineDataBuilder()
        .withCoords(20, {0.0f, 3.0f}, {0.0f, 3.0f})
        .build();
}

/**
 * @brief Line with few points for polynomial fallback test
 * 
 * Line: (0,0) to (1,1) at t=40
 * Expected: Falls back to direct method when polynomial order too high
 */
inline std::shared_ptr<LineData> two_point_line() {
    return LineDataBuilder()
        .withCoords(40, {0.0f, 1.0f}, {0.0f, 1.0f})
        .build();
}

/**
 * @brief Vertical collinear line (all x values same)
 * 
 * Tests polynomial fit with collinear points
 */
inline std::shared_ptr<LineData> vertical_collinear() {
    return LineDataBuilder()
        .withCoords(50, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
}

/**
 * @brief Simple 45-degree line for null params test
 */
inline std::shared_ptr<LineData> simple_diagonal() {
    return LineDataBuilder()
        .withCoords(60, {0.0f, 1.0f, 2.0f}, {0.0f, 1.0f, 2.0f})
        .build();
}

/**
 * @brief Large line with 1000 points (stress test)
 * 
 * Creates a 1000-point diagonal line for performance testing
 */
inline std::shared_ptr<LineData> large_diagonal_line() {
    std::vector<float> x_coords, y_coords;
    for (int i = 0; i < 1000; ++i) {
        x_coords.push_back(static_cast<float>(i));
        y_coords.push_back(static_cast<float>(i));
    }
    return LineDataBuilder()
        .withCoords(70, x_coords, y_coords)
        .build();
}

/**
 * @brief Horizontal line for reference normalization test
 */
inline std::shared_ptr<LineData> horizontal_for_normalization() {
    return LineDataBuilder()
        .withCoords(90, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 0.0f})
        .build();
}

/**
 * @brief Problematic 2-point line 1 with negative coordinates
 */
inline std::shared_ptr<LineData> problematic_line_1() {
    return LineDataBuilder()
        .withCoords(200, {565.0f, 408.0f}, {253.0f, 277.0f})
        .build();
}

/**
 * @brief Problematic 2-point line 2 with negative coordinates
 */
inline std::shared_ptr<LineData> problematic_line_2() {
    return LineDataBuilder()
        .withCoords(210, {567.0f, 434.0f}, {252.0f, 265.0f})
        .build();
}

// ============================================================================
// JSON Pipeline Test Scenarios
// ============================================================================

/**
 * @brief Basic JSON pipeline test
 * 
 * t=100: horizontal line (0 degrees)
 * t=200: diagonal line (45 degrees)
 */
inline std::shared_ptr<LineData> json_pipeline_two_timesteps() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 0.0f})
        .withCoords(200, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Multiple lines for JSON pipeline test
 * 
 * t=100: horizontal line (0 degrees)
 * t=200: vertical line (90 degrees)
 * t=300: 45-degree line (45 degrees)
 */
inline std::shared_ptr<LineData> json_pipeline_multiple_angles() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f}, {0.0f, 0.0f, 0.0f})
        .withCoords(200, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 2.0f})
        .withCoords(300, {0.0f, 1.0f, 2.0f}, {0.0f, 1.0f, 2.0f})
        .build();
}

/**
 * @brief Empty line data
 */
inline std::shared_ptr<LineData> empty_line_data() {
    return LineDataBuilder().build();
}

} // namespace line_scenarios

#endif // GEOMETRY_SCENARIOS_HPP
