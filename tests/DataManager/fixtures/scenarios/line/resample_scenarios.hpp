#ifndef RESAMPLE_SCENARIOS_HPP
#define RESAMPLE_SCENARIOS_HPP

#include "fixtures/builders/LineDataBuilder.hpp"
#include <memory>

/**
 * @brief Resample test scenarios for LineData
 * 
 * This namespace contains pre-configured test data for line resampling
 * algorithms including Fixed Spacing and Douglas-Peucker simplification.
 */
namespace resample_scenarios {

// ============================================================================
// Basic Line Scenarios
// ============================================================================

/**
 * @brief Two diagonal lines at different timestamps
 * 
 * t=100: 5-point diagonal line (10,10) to (50,50)
 * t=200: 6-point diagonal line (100,100) to (150,150)
 * 
 * Useful for testing Fixed Spacing algorithm with multiple time points
 */
inline std::shared_ptr<LineData> two_diagonal_lines() {
    return LineDataBuilder()
        .withCoords(100, 
            {10.0f, 20.0f, 30.0f, 40.0f, 50.0f},
            {10.0f, 20.0f, 30.0f, 40.0f, 50.0f})
        .withCoords(200,
            {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f},
            {100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f})
        .withImageSize(1000, 1000)
        .build();
}

/**
 * @brief Dense nearly-straight line for Douglas-Peucker simplification
 * 
 * t=100: 11 points with very small y variation (almost straight line)
 * 
 * Expected: Douglas-Peucker should significantly reduce point count
 */
inline std::shared_ptr<LineData> dense_nearly_straight_line() {
    return LineDataBuilder()
        .withCoords(100,
            {10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f},
            {10.0f, 10.1f, 10.2f, 10.3f, 10.4f, 10.5f, 10.6f, 10.7f, 10.8f, 10.9f, 11.0f})
        .withImageSize(1000, 1000)
        .build();
}

/**
 * @brief Simple 3-point diagonal line
 * 
 * t=100: (10,10) to (30,30) with 3 points
 */
inline std::shared_ptr<LineData> simple_diagonal() {
    return LineDataBuilder()
        .withCoords(100,
            {10.0f, 20.0f, 30.0f},
            {10.0f, 20.0f, 30.0f})
        .withImageSize(1000, 1000)
        .build();
}

/**
 * @brief Diagonal line with one timestamp having an empty line
 * 
 * t=100: 3-point diagonal line
 * t=200: Empty line (no points)
 * 
 * Useful for testing handling of empty lines in the data
 */
inline std::shared_ptr<LineData> diagonal_with_empty() {
    return LineDataBuilder()
        .withCoords(100,
            {10.0f, 20.0f, 30.0f},
            {10.0f, 20.0f, 30.0f})
        .withCoords(200, {}, {})
        .withImageSize(1000, 1000)
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
    return LineDataBuilder()
        .withImageSize(1000, 1000)
        .build();
}

/**
 * @brief Line with single point
 * 
 * t=100: Single point at (10, 10)
 * 
 * Expected: Should preserve the single point
 */
inline std::shared_ptr<LineData> single_point() {
    return LineDataBuilder()
        .withCoords(100, {10.0f}, {10.0f})
        .withImageSize(1000, 1000)
        .build();
}

} // namespace resample_scenarios

#endif // RESAMPLE_SCENARIOS_HPP
