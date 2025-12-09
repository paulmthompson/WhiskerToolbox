#ifndef CONNECTED_COMPONENT_SCENARIOS_HPP
#define CONNECTED_COMPONENT_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include "fixtures/scenarios/mask/area_scenarios.hpp" // For empty_mask_data()
#include <memory>

/**
 * @brief Connected component filtering test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask connected
 * component filtering algorithms. These scenarios test various component
 * sizes and filtering thresholds.
 */
namespace mask_scenarios {

// ============================================================================
// Core functionality scenarios
// ============================================================================

/**
 * @brief Large component (9px) + small components (1px, 2px)
 * 
 * Image size: 10x10
 * Time 0:
 *   - Large component: 3x3 square at (1,1) = 9 pixels
 *   - Small component 1: single pixel at (7,1) = 1 pixel
 *   - Small component 2: two pixels at (7,7)-(8,7) = 2 pixels
 * 
 * With threshold=5: keeps large (9px), removes small (1px, 2px)
 * Expected preserved: 1 mask with 9 pixels
 */
inline std::shared_ptr<MaskData> large_and_small_components() {
    // Large component: 3x3 square = 9 pixels
    std::vector<uint32_t> large_xs = {1, 2, 3, 1, 2, 3, 1, 2, 3};
    std::vector<uint32_t> large_ys = {1, 1, 1, 2, 2, 2, 3, 3, 3};
    
    // Small component 1: 1 pixel
    std::vector<uint32_t> small1_xs = {7};
    std::vector<uint32_t> small1_ys = {1};
    
    // Small component 2: 2 pixels
    std::vector<uint32_t> small2_xs = {7, 8};
    std::vector<uint32_t> small2_ys = {7, 7};
    
    return MaskDataBuilder()
        .atTime(0, Mask2D(large_xs, large_ys))
        .atTime(0, Mask2D(small1_xs, small1_ys))
        .atTime(0, Mask2D(small2_xs, small2_ys))
        .withImageSize(10, 10)
        .build();
}

/**
 * @brief Multiple small components (all 1-2 pixels)
 * 
 * Image size: 5x5
 * Time 10:
 *   - Component 1: single pixel at (1,1) = 1 pixel
 *   - Component 2: single pixel at (3,3) = 1 pixel  
 *   - Component 3: two pixels at (0,4)-(1,4) = 2 pixels
 * 
 * Total: 4 pixels across 3 components
 * 
 * With threshold=1: preserves all 3 components (4 total pixels)
 */
inline std::shared_ptr<MaskData> multiple_small_components() {
    std::vector<uint32_t> c1_xs = {1};
    std::vector<uint32_t> c1_ys = {1};
    
    std::vector<uint32_t> c2_xs = {3};
    std::vector<uint32_t> c2_ys = {3};
    
    std::vector<uint32_t> c3_xs = {0, 1};
    std::vector<uint32_t> c3_ys = {4, 4};
    
    return MaskDataBuilder()
        .atTime(10, Mask2D(c1_xs, c1_ys))
        .atTime(10, Mask2D(c2_xs, c2_ys))
        .atTime(10, Mask2D(c3_xs, c3_ys))
        .withImageSize(5, 5)
        .build();
}

/**
 * @brief Medium-sized components (3px and 2px)
 * 
 * Image size: 10x10
 * Time 5:
 *   - Component 1: 3 pixels at (0,0), (1,0), (0,1)
 *   - Component 2: 2 pixels at (5,5), (6,5)
 * 
 * With threshold=10: removes all (max component is 3 pixels)
 */
inline std::shared_ptr<MaskData> medium_components() {
    std::vector<uint32_t> c1_xs = {0, 1, 0};
    std::vector<uint32_t> c1_ys = {0, 0, 1};
    
    std::vector<uint32_t> c2_xs = {5, 6};
    std::vector<uint32_t> c2_ys = {5, 5};
    
    return MaskDataBuilder()
        .atTime(5, Mask2D(c1_xs, c1_ys))
        .atTime(5, Mask2D(c2_xs, c2_ys))
        .withImageSize(10, 10)
        .build();
}

/**
 * @brief Multiple timestamps with different component sizes
 * 
 * Image size: 8x8
 * Time 0: Large component (6 pixels)
 * Time 1: Small component (2 pixels)
 * Time 2: Medium component (5 pixels)
 * 
 * With threshold=4:
 *   - Time 0: preserved (6 >= 4)
 *   - Time 1: removed (2 < 4)
 *   - Time 2: preserved (5 >= 4)
 */
inline std::shared_ptr<MaskData> multiple_timestamps() {
    // Time 0: Large component (6 pixels)
    std::vector<uint32_t> large_xs = {0, 1, 2, 0, 1, 2};
    std::vector<uint32_t> large_ys = {0, 0, 0, 1, 1, 1};
    
    // Time 1: Small component (2 pixels)
    std::vector<uint32_t> small_xs = {5, 5};
    std::vector<uint32_t> small_ys = {5, 6};
    
    // Time 2: Medium component (5 pixels)
    std::vector<uint32_t> medium_xs = {3, 4, 3, 4, 3};
    std::vector<uint32_t> medium_ys = {3, 3, 4, 4, 5};
    
    return MaskDataBuilder()
        .atTime(0, Mask2D(large_xs, large_ys))
        .atTime(1, Mask2D(small_xs, small_ys))
        .atTime(2, Mask2D(medium_xs, medium_ys))
        .withImageSize(8, 8)
        .build();
}

// ============================================================================
// Operation interface test scenarios
// ============================================================================

/**
 * @brief Large component (12px) + small component (1px)
 * 
 * Image size: 6x6
 * Time 0:
 *   - Large component: 12 pixels (4x3 rectangle)
 *   - Small component: 1 pixel at (5,5)
 * 
 * With default threshold=10: keeps large (12px), removes small (1px)
 * Expected: 1 mask with 12 pixels
 */
inline std::shared_ptr<MaskData> operation_test_data() {
    // Large component: 12 pixels (4x3 rectangle)
    std::vector<uint32_t> large_xs = {0, 1, 2, 0, 1, 2, 0, 1, 2, 3, 3, 3};
    std::vector<uint32_t> large_ys = {0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2};
    
    // Small component: 1 pixel
    std::vector<uint32_t> small_xs = {5};
    std::vector<uint32_t> small_ys = {5};
    
    return MaskDataBuilder()
        .atTime(0, Mask2D(large_xs, large_ys))
        .atTime(0, Mask2D(small_xs, small_ys))
        .withImageSize(6, 6)
        .build();
}

// ============================================================================
// JSON pipeline test scenarios
// ============================================================================

/**
 * @brief Mixed components for JSON pipeline testing
 * 
 * Image size: 10x10
 * Time 0:
 *   - Large component: 3x3 square = 9 pixels
 *   - Small component: 1 pixel
 *   - Medium component: 2x2 square = 4 pixels
 * 
 * With threshold=3: keeps large + medium (13 total pixels)
 * With threshold=5: keeps large only (9 pixels)
 * With threshold=1: keeps all (14 total pixels)
 */
inline std::shared_ptr<MaskData> json_pipeline_mixed() {
    // Large component: 3x3 square = 9 pixels
    std::vector<uint32_t> large_xs = {1, 2, 3, 1, 2, 3, 1, 2, 3};
    std::vector<uint32_t> large_ys = {1, 1, 1, 2, 2, 2, 3, 3, 3};
    
    // Small component: 1 pixel
    std::vector<uint32_t> small_xs = {7};
    std::vector<uint32_t> small_ys = {1};
    
    // Medium component: 2x2 square = 4 pixels
    std::vector<uint32_t> medium_xs = {5, 6, 5, 6};
    std::vector<uint32_t> medium_ys = {5, 5, 6, 6};
    
    return MaskDataBuilder()
        .atTime(0, Mask2D(large_xs, large_ys))
        .atTime(0, Mask2D(small_xs, small_ys))
        .atTime(0, Mask2D(medium_xs, medium_ys))
        .withImageSize(10, 10)
        .build();
}

} // namespace mask_scenarios

#endif // CONNECTED_COMPONENT_SCENARIOS_HPP
