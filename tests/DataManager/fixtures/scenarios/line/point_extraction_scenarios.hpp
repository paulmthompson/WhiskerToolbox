#ifndef POINT_EXTRACTION_SCENARIOS_HPP
#define POINT_EXTRACTION_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Point extraction test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line point
 * extraction algorithms. These scenarios support testing direct and
 * parametric point extraction at various positions along lines.
 */
namespace point_extraction_scenarios {

// ============================================================================
// Basic Line Scenarios
// ============================================================================

/**
 * @brief Simple diagonal line (y = x) with 4 points
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 100
 * 
 * At position 0.5 with interpolation:
 *   - Expected point: (1.5, 1.5)
 */
inline std::shared_ptr<LineData> diagonal_4_points() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Diagonal line at time 200
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 200
 */
inline std::shared_ptr<LineData> diagonal_at_time_200() {
    return LineDataBuilder()
        .withCoords(200, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Diagonal line at time 300
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 300
 */
inline std::shared_ptr<LineData> diagonal_at_time_300() {
    return LineDataBuilder()
        .withCoords(300, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Diagonal line at time 400
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 400
 */
inline std::shared_ptr<LineData> diagonal_at_time_400() {
    return LineDataBuilder()
        .withCoords(400, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Diagonal line at time 700
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 700
 */
inline std::shared_ptr<LineData> diagonal_at_time_700() {
    return LineDataBuilder()
        .withCoords(700, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Horizontal line (constant y = 0)
 * 
 * Data: x = {0, 1, 2}, y = {0, 0, 0}
 * Time: 600
 * 
 * At position 0.5:
 *   - Expected point: (1.0, 0.0)
 */
inline std::shared_ptr<LineData> horizontal_3_points() {
    return LineDataBuilder()
        .withCoords(600, 
            {0.0f, 1.0f, 2.0f},
            {0.0f, 0.0f, 0.0f})
        .build();
}

/**
 * @brief Multiple lines at different timestamps
 * 
 * t=500: Diagonal line (0,0) to (4,4), 3 points
 * t=600: Horizontal line (0,0) to (2,0), 3 points
 * 
 * Useful for testing batch processing across time
 */
inline std::shared_ptr<LineData> multiple_timesteps() {
    return LineDataBuilder()
        .withCoords(500, 
            {0.0f, 2.0f, 4.0f},
            {0.0f, 2.0f, 4.0f})
        .withCoords(600,
            {0.0f, 1.0f, 2.0f},
            {0.0f, 0.0f, 0.0f})
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
 * @brief Line with single point
 * 
 * Data: x = {1.5}, y = {2.5}
 * Time: 200
 * 
 * Expected: Returns the single point regardless of position
 */
inline std::shared_ptr<LineData> single_point() {
    return LineDataBuilder()
        .withCoords(200, {1.5f}, {2.5f})
        .build();
}

/**
 * @brief Line with only 2 points - may be insufficient for parametric fit
 * 
 * Data: x = {0, 1}, y = {0, 1}
 * Time: 500
 * 
 * Expected: Falls back to direct method when polynomial order too high
 */
inline std::shared_ptr<LineData> two_point_line() {
    return LineDataBuilder()
        .withCoords(500, {0.0f, 1.0f}, {0.0f, 1.0f})
        .build();
}

/**
 * @brief Line with 3 points for boundary tests
 * 
 * Data: x = {0, 1, 2}, y = {0, 1, 2}
 * Time: 300 (for position out of bounds test)
 * Time: 400 (for negative position test)
 */
inline std::shared_ptr<LineData> three_point_diagonal_at_300() {
    return LineDataBuilder()
        .withCoords(300, 
            {0.0f, 1.0f, 2.0f},
            {0.0f, 1.0f, 2.0f})
        .build();
}

inline std::shared_ptr<LineData> three_point_diagonal_at_400() {
    return LineDataBuilder()
        .withCoords(400, 
            {0.0f, 1.0f, 2.0f},
            {0.0f, 1.0f, 2.0f})
        .build();
}

} // namespace point_extraction_scenarios

#endif // POINT_EXTRACTION_SCENARIOS_HPP
