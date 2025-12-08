#ifndef CENTROID_SCENARIOS_HPP
#define CENTROID_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include "fixtures/scenarios/mask/area_scenarios.hpp" // For empty_mask_data()
#include <memory>

/**
 * @brief Centroid calculation test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask centroid
 * calculation algorithms. These scenarios test various edge cases
 * and common patterns.
 * 
 * Note: For empty_mask_data(), use mask_scenarios::empty_mask_data() from area_scenarios.hpp
 */
namespace mask_scenarios {

// ============================================================================
// Core functionality scenarios
// ============================================================================

/**
 * @brief Single mask at single timestamp - triangle (3 points)
 * 
 * Vertices at (0,0), (3,0), (0,3) -> centroid at (1,1)
 * 
 * Expected: {10: [(1.0, 1.0)]}
 */
inline std::shared_ptr<MaskData> single_mask_triangle() {
    std::vector<uint32_t> x = {0, 3, 0};
    std::vector<uint32_t> y = {0, 0, 3};
    return MaskDataBuilder()
        .atTime(10, Mask2D(x, y))
        .build();
}

/**
 * @brief Multiple masks at single timestamp - two squares
 * 
 * First: (0,0), (1,0), (0,1), (1,1) -> centroid at (0.5, 0.5)
 * Second: (4,4), (5,4), (4,5), (5,5) -> centroid at (4.5, 4.5)
 * 
 * Expected: {20: [(0.5, 0.5), (4.5, 4.5)]}
 */
inline std::shared_ptr<MaskData> multiple_masks_single_timestamp_centroid() {
    std::vector<uint32_t> x1 = {0, 1, 0, 1};
    std::vector<uint32_t> y1 = {0, 0, 1, 1};
    std::vector<uint32_t> x2 = {4, 5, 4, 5};
    std::vector<uint32_t> y2 = {4, 4, 5, 5};
    return MaskDataBuilder()
        .atTime(20, Mask2D(x1, y1))
        .atTime(20, Mask2D(x2, y2))
        .build();
}

/**
 * @brief Single masks across multiple timestamps
 * 
 * Timestamp 30: Horizontal line (0,0), (2,0), (4,0) -> centroid at (2, 0)
 * Timestamp 40: Vertical line (1,0), (1,3), (1,6) -> centroid at (1, 3)
 * 
 * Expected: {30: [(2.0, 0.0)], 40: [(1.0, 3.0)]}
 */
inline std::shared_ptr<MaskData> masks_multiple_timestamps_centroid() {
    std::vector<uint32_t> x1 = {0, 2, 4};
    std::vector<uint32_t> y1 = {0, 0, 0};
    std::vector<uint32_t> x2 = {1, 1, 1};
    std::vector<uint32_t> y2 = {0, 3, 6};
    return MaskDataBuilder()
        .atTime(30, Mask2D(x1, y1))
        .atTime(40, Mask2D(x2, y2))
        .build();
}

/**
 * @brief Mask with image size set
 * 
 * Image size: 640x480
 * Points: (100, 100), (200, 150), (300, 200) -> centroid at (200, 150)
 * 
 * Expected: {100: [(200.0, 150.0)]} with image size preserved
 */
inline std::shared_ptr<MaskData> mask_with_image_size_centroid() {
    std::vector<uint32_t> x = {100, 200, 300};
    std::vector<uint32_t> y = {100, 150, 200};
    return MaskDataBuilder()
        .atTime(100, Mask2D(x, y))
        .withImageSize(640, 480)
        .build();
}

// ============================================================================
// Edge case scenarios
// ============================================================================

/**
 * @brief Empty mask (mask with zero pixels) at a timestamp
 * 
 * Expected: empty PointData (empty masks are skipped)
 */
inline std::shared_ptr<MaskData> empty_mask_at_timestamp_centroid() {
    return MaskDataBuilder()
        .withEmpty(10)
        .build();
}

/**
 * @brief Mixed empty and non-empty masks at same timestamp
 * 
 * Empty mask + mask with points (2,1), (4,3) -> centroid at (3, 2)
 * 
 * Expected: {20: [(3.0, 2.0)]} (empty mask skipped)
 */
inline std::shared_ptr<MaskData> mixed_empty_nonempty_centroid() {
    std::vector<uint32_t> x = {2, 4};
    std::vector<uint32_t> y = {1, 3};
    return MaskDataBuilder()
        .withEmpty(20)
        .atTime(20, Mask2D(x, y))
        .build();
}

/**
 * @brief Single point masks - two masks each with one point
 * 
 * First: (5, 7) -> centroid at (5, 7)
 * Second: (10, 15) -> centroid at (10, 15)
 * 
 * Expected: {30: [(5.0, 7.0), (10.0, 15.0)]}
 */
inline std::shared_ptr<MaskData> single_point_masks_centroid() {
    std::vector<uint32_t> x1 = {5};
    std::vector<uint32_t> y1 = {7};
    std::vector<uint32_t> x2 = {10};
    std::vector<uint32_t> y2 = {15};
    return MaskDataBuilder()
        .atTime(30, Mask2D(x1, y1))
        .atTime(30, Mask2D(x2, y2))
        .build();
}

/**
 * @brief Large coordinate values to test for overflow
 * 
 * Points: (1000000, 2000000), (1000001, 2000001), (1000002, 2000002)
 * -> centroid at (1000001, 2000001)
 * 
 * Expected: {40: [(1000001.0, 2000001.0)]}
 */
inline std::shared_ptr<MaskData> large_coordinates_centroid() {
    std::vector<uint32_t> x = {1000000, 1000001, 1000002};
    std::vector<uint32_t> y = {2000000, 2000001, 2000002};
    return MaskDataBuilder()
        .atTime(40, Mask2D(x, y))
        .build();
}

// ============================================================================
// Operation interface scenarios
// ============================================================================

/**
 * @brief Horizontal line mask for operation execute test
 * 
 * Points: (0, 0), (2, 0), (4, 0) -> centroid at (2, 0)
 * 
 * Expected: {50: [(2.0, 0.0)]}
 */
inline std::shared_ptr<MaskData> operation_execute_test_centroid() {
    std::vector<uint32_t> x = {0, 2, 4};
    std::vector<uint32_t> y = {0, 0, 0};
    return MaskDataBuilder()
        .atTime(50, Mask2D(x, y))
        .build();
}

// ============================================================================
// JSON pipeline scenarios
// ============================================================================

/**
 * @brief Basic JSON pipeline test: triangle, square, and multi-mask
 * 
 * Timestamp 100: Triangle (0,0), (3,0), (0,3) -> centroid at (1, 1)
 * Timestamp 200: Square (1,1), (3,1), (1,3), (3,3) -> centroid at (2, 2)
 * Timestamp 300: Two squares -> centroids at (1, 1) and (6, 6)
 * 
 * Expected: {100: [(1,1)], 200: [(2,2)], 300: [(1,1), (6,6)]}
 */
inline std::shared_ptr<MaskData> json_pipeline_basic_centroid() {
    // Timestamp 100: Triangle
    std::vector<uint32_t> x1 = {0, 3, 0};
    std::vector<uint32_t> y1 = {0, 0, 3};
    
    // Timestamp 200: Square
    std::vector<uint32_t> x2 = {1, 3, 1, 3};
    std::vector<uint32_t> y2 = {1, 1, 3, 3};
    
    // Timestamp 300: Two squares
    std::vector<uint32_t> x3a = {0, 2, 0, 2};
    std::vector<uint32_t> y3a = {0, 0, 2, 2};
    std::vector<uint32_t> x3b = {5, 7, 5, 7};
    std::vector<uint32_t> y3b = {5, 5, 7, 7};
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x1, y1))
        .atTime(200, Mask2D(x2, y2))
        .atTime(300, Mask2D(x3a, y3a))
        .atTime(300, Mask2D(x3b, y3b))
        .build();
}

} // namespace mask_scenarios

#endif // CENTROID_SCENARIOS_HPP
