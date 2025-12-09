#ifndef PRINCIPAL_AXIS_SCENARIOS_HPP
#define PRINCIPAL_AXIS_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include "fixtures/scenarios/mask/area_scenarios.hpp" // For empty_mask_data()
#include <memory>
#include <cmath>

/**
 * @brief Principal axis calculation test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask principal axis
 * calculation algorithms. These scenarios test various shapes including
 * horizontal lines, vertical lines, diagonal lines, rectangles, and circles.
 */
namespace mask_scenarios {

// ============================================================================
// Core functionality scenarios
// ============================================================================

/**
 * @brief Single point mask (insufficient for principal axis calculation)
 * 
 * Single point at (5, 5) - should be skipped in principal axis calculation.
 * 
 * Expected: Empty result (single points are skipped)
 */
inline std::shared_ptr<MaskData> single_point_mask_principal_axis() {
    std::vector<uint32_t> x = {5};
    std::vector<uint32_t> y = {5};
    return MaskDataBuilder()
        .atTime(10, Mask2D(x, y))
        .build();
}

/**
 * @brief Horizontal line mask
 * 
 * Horizontal line of 6 points at y=2, x from 0 to 5.
 * Major axis should be horizontal (angle close to 0).
 * 
 * Expected: Major axis angle within ~11 degrees of horizontal
 */
inline std::shared_ptr<MaskData> horizontal_line_mask() {
    std::vector<uint32_t> x = {0, 1, 2, 3, 4, 5};
    std::vector<uint32_t> y = {2, 2, 2, 2, 2, 2};
    return MaskDataBuilder()
        .atTime(20, Mask2D(x, y))
        .build();
}

/**
 * @brief Vertical line mask
 * 
 * Vertical line of 6 points at x=3, y from 0 to 5.
 * Major axis should be vertical (angle close to π/2).
 * 
 * Expected: Major axis angle within ~11 degrees of vertical
 */
inline std::shared_ptr<MaskData> vertical_line_mask() {
    std::vector<uint32_t> x = {3, 3, 3, 3, 3, 3};
    std::vector<uint32_t> y = {0, 1, 2, 3, 4, 5};
    return MaskDataBuilder()
        .atTime(30, Mask2D(x, y))
        .build();
}

/**
 * @brief Diagonal line mask (45 degrees)
 * 
 * Diagonal line of 5 points from (0,0) to (4,4).
 * Major axis should be at approximately 45 degrees.
 * 
 * Expected: Major axis angle close to π/4 (45 degrees)
 */
inline std::shared_ptr<MaskData> diagonal_line_mask() {
    std::vector<uint32_t> x = {0, 1, 2, 3, 4};
    std::vector<uint32_t> y = {0, 1, 2, 3, 4};
    return MaskDataBuilder()
        .atTime(40, Mask2D(x, y))
        .build();
}

/**
 * @brief Rectangle mask (wider than tall)
 * 
 * Rectangle 7x3 (width=7, height=3) filled with points.
 * Major axis should be horizontal, minor axis should be vertical.
 * 
 * Expected: 
 * - Major axis angle < π/4 (more horizontal)
 * - Minor axis angle > π/4 (more vertical)
 */
inline std::shared_ptr<MaskData> wide_rectangle_mask() {
    std::vector<uint32_t> x_coords, y_coords;
    for (uint32_t x = 0; x <= 6; ++x) {
        for (uint32_t y = 0; y <= 2; ++y) {
            x_coords.push_back(x);
            y_coords.push_back(y);
        }
    }
    return MaskDataBuilder()
        .atTime(50, Mask2D(x_coords, y_coords))
        .build();
}

/**
 * @brief Multiple masks at one timestamp (horizontal + vertical lines)
 * 
 * First mask: Horizontal line at y=1, x from 0 to 3
 * Second mask: Vertical line at x=5, y from 0 to 3
 * 
 * Expected: Two principal axis lines (one per mask)
 */
inline std::shared_ptr<MaskData> multiple_masks_principal_axis() {
    std::vector<uint32_t> x1 = {0, 1, 2, 3};
    std::vector<uint32_t> y1 = {1, 1, 1, 1};
    std::vector<uint32_t> x2 = {5, 5, 5, 5};
    std::vector<uint32_t> y2 = {0, 1, 2, 3};
    return MaskDataBuilder()
        .atTime(60, Mask2D(x1, y1))
        .atTime(60, Mask2D(x2, y2))
        .build();
}

/**
 * @brief Mask with image size for verifying preservation
 * 
 * Horizontal line at y=100, x from 100 to 300.
 * Image size: 640x480
 * 
 * Expected: Image size should be preserved in output
 */
inline std::shared_ptr<MaskData> mask_with_image_size_principal_axis() {
    std::vector<uint32_t> x = {100, 200, 300};
    std::vector<uint32_t> y = {100, 100, 100};
    return MaskDataBuilder()
        .atTime(100, Mask2D(x, y))
        .withImageSize(640, 480)
        .build();
}

// ============================================================================
// Edge case scenarios
// ============================================================================

/**
 * @brief Two identical points (no variance)
 * 
 * Two points at the same location (5, 5).
 * Should be handled gracefully.
 * 
 * Expected: At most one line, or empty result
 */
inline std::shared_ptr<MaskData> identical_points_mask() {
    std::vector<uint32_t> x = {5, 5};
    std::vector<uint32_t> y = {5, 5};
    return MaskDataBuilder()
        .atTime(10, Mask2D(x, y))
        .build();
}

/**
 * @brief Circular point distribution
 * 
 * 12 points arranged in a rough circle (center at 10,10, radius 5).
 * For a circle, major and minor axes should be similar in magnitude.
 * 
 * Expected: One line with 2 points
 */
inline std::shared_ptr<MaskData> circular_mask() {
    std::vector<uint32_t> x_coords, y_coords;
    int center_x = 10, center_y = 10, radius = 5;

    for (int angle_deg = 0; angle_deg < 360; angle_deg += 30) {
        float angle_rad = static_cast<float>(angle_deg) * static_cast<float>(M_PI) / 180.0f;
        int x = center_x + static_cast<int>(static_cast<float>(radius) * std::cos(angle_rad));
        int y = center_y + static_cast<int>(static_cast<float>(radius) * std::sin(angle_rad));
        x_coords.push_back(static_cast<uint32_t>(x));
        y_coords.push_back(static_cast<uint32_t>(y));
    }
    return MaskDataBuilder()
        .atTime(20, Mask2D(x_coords, y_coords))
        .build();
}

/**
 * @brief Large coordinates mask
 * 
 * Horizontal line with large coordinate values.
 * Tests handling of large numbers.
 * 
 * Expected: One line with 2 points
 */
inline std::shared_ptr<MaskData> large_coordinates_principal_axis() {
    std::vector<uint32_t> x = {1000000, 1000001, 1000002};
    std::vector<uint32_t> y = {2000000, 2000000, 2000000};
    return MaskDataBuilder()
        .atTime(30, Mask2D(x, y))
        .build();
}

// ============================================================================
// Operation interface scenarios
// ============================================================================

/**
 * @brief Horizontal line for operation execute test (major axis)
 * 
 * Horizontal line of 5 points at y=2, x from 0 to 4.
 * 
 * Expected: One line with 2 points at TimeFrameIndex(50)
 */
inline std::shared_ptr<MaskData> operation_execute_horizontal() {
    std::vector<uint32_t> x = {0, 1, 2, 3, 4};
    std::vector<uint32_t> y = {2, 2, 2, 2, 2};
    return MaskDataBuilder()
        .atTime(50, Mask2D(x, y))
        .build();
}

/**
 * @brief Vertical line for operation execute test (minor axis)
 * 
 * Vertical line of 5 points at x=3, y from 0 to 4.
 * 
 * Expected: One line with 2 points at TimeFrameIndex(60)
 */
inline std::shared_ptr<MaskData> operation_execute_vertical() {
    std::vector<uint32_t> x = {3, 3, 3, 3, 3};
    std::vector<uint32_t> y = {0, 1, 2, 3, 4};
    return MaskDataBuilder()
        .atTime(60, Mask2D(x, y))
        .build();
}

// ============================================================================
// JSON pipeline scenarios
// ============================================================================

/**
 * @brief Multiple line types for JSON pipeline test
 * 
 * Timestamp 100: Horizontal line (6 points at y=2)
 * Timestamp 200: Vertical line (6 points at x=3)
 * Timestamp 300: Diagonal line (5 points from origin)
 * 
 * Expected: Three timestamps, each with one line of 2 points
 */
inline std::shared_ptr<MaskData> json_pipeline_multi_line_principal_axis() {
    // Horizontal line at time 100
    std::vector<uint32_t> x_horiz = {0, 1, 2, 3, 4, 5};
    std::vector<uint32_t> y_horiz = {2, 2, 2, 2, 2, 2};
    
    // Vertical line at time 200
    std::vector<uint32_t> x_vert = {3, 3, 3, 3, 3, 3};
    std::vector<uint32_t> y_vert = {0, 1, 2, 3, 4, 5};
    
    // Diagonal line at time 300
    std::vector<uint32_t> x_diag = {0, 1, 2, 3, 4};
    std::vector<uint32_t> y_diag = {0, 1, 2, 3, 4};
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x_horiz, y_horiz))
        .atTime(200, Mask2D(x_vert, y_vert))
        .atTime(300, Mask2D(x_diag, y_diag))
        .build();
}

/**
 * @brief Wide rectangle for JSON pipeline major/minor axis comparison
 * 
 * Rectangle 7x3 at TimeFrameIndex(400).
 * Major axis should be horizontal, minor axis should be vertical.
 * 
 * Expected: Major axis angle < π/4
 */
inline std::shared_ptr<MaskData> json_pipeline_rectangle_principal_axis() {
    std::vector<uint32_t> x_coords, y_coords;
    for (uint32_t x = 0; x <= 6; ++x) {
        for (uint32_t y = 0; y <= 2; ++y) {
            x_coords.push_back(x);
            y_coords.push_back(y);
        }
    }
    return MaskDataBuilder()
        .atTime(400, Mask2D(x_coords, y_coords))
        .build();
}

} // namespace mask_scenarios

#endif // PRINCIPAL_AXIS_SCENARIOS_HPP
