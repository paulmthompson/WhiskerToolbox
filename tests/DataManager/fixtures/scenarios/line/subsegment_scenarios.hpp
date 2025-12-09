#ifndef SUBSEGMENT_SCENARIOS_HPP
#define SUBSEGMENT_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Subsegment extraction test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line subsegment
 * extraction algorithms including Direct and Parametric methods.
 */
namespace subsegment_scenarios {

// ============================================================================
// Basic Line Scenarios
// ============================================================================

/**
 * @brief 5-point diagonal line (0,0) to (4,4)
 * 
 * Data: x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}
 * Time: 100
 * 
 * Total length: ~5.66 (4 * sqrt(2))
 * Useful for testing subsegment extraction with various position ranges
 */
inline std::shared_ptr<LineData> diagonal_5_points() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
}

/**
 * @brief 4-point diagonal line at time 200
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 200
 */
inline std::shared_ptr<LineData> diagonal_4_points_at_200() {
    return LineDataBuilder()
        .withCoords(200, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief 5-point diagonal line at time 300
 * 
 * Data: x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}
 * Time: 300
 */
inline std::shared_ptr<LineData> diagonal_5_points_at_300() {
    return LineDataBuilder()
        .withCoords(300, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
}

/**
 * @brief 5-point diagonal line at time 400
 * 
 * Data: x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}
 * Time: 400
 */
inline std::shared_ptr<LineData> diagonal_5_points_at_400() {
    return LineDataBuilder()
        .withCoords(400, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
}

/**
 * @brief 5-point diagonal line at time 500
 * 
 * Data: x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}
 * Time: 500
 */
inline std::shared_ptr<LineData> diagonal_5_points_at_500() {
    return LineDataBuilder()
        .withCoords(500, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
}

// ============================================================================
// Edge Case Scenarios
// ============================================================================

/**
 * @brief Empty line data (no lines at any time)
 * 
 * Expected: Should return empty result
 */
inline std::shared_ptr<LineData> empty() {
    return LineDataBuilder().build();
}

/**
 * @brief Line with single point
 * 
 * Data: x = {1}, y = {2}
 * Time: 100
 * 
 * Expected: Should return the single point
 */
inline std::shared_ptr<LineData> single_point() {
    return LineDataBuilder()
        .withCoords(100, {1.0f}, {2.0f})
        .build();
}

/**
 * @brief 4-point diagonal line at time 100
 * 
 * Data: x = {0, 1, 2, 3}, y = {0, 1, 2, 3}
 * Time: 100
 * 
 * Useful for testing invalid position ranges and clamping
 */
inline std::shared_ptr<LineData> diagonal_4_points() {
    return LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f},
            {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
}

/**
 * @brief Two-point line - insufficient for high-order parametric fit
 * 
 * Data: x = {0, 1}, y = {0, 1}
 * Time: 100
 * 
 * Expected: Should fall back to direct method
 */
inline std::shared_ptr<LineData> two_point_line() {
    return LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f}, {0.0f, 1.0f})
        .build();
}

/**
 * @brief Zero-length line (all points at same location)
 * 
 * Data: x = {1, 1, 1}, y = {2, 2, 2}
 * Time: 100
 * 
 * Expected: Should return single point
 */
inline std::shared_ptr<LineData> zero_length_line() {
    return LineDataBuilder()
        .withCoords(100, 
            {1.0f, 1.0f, 1.0f},
            {2.0f, 2.0f, 2.0f})
        .build();
}

} // namespace subsegment_scenarios

#endif // SUBSEGMENT_SCENARIOS_HPP
