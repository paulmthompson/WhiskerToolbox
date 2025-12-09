#ifndef SKELETONIZE_SCENARIOS_HPP
#define SKELETONIZE_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include "fixtures/scenarios/mask/area_scenarios.hpp" // For empty_mask_data()
#include <memory>

/**
 * @brief Skeletonization test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask skeletonization
 * algorithms. These scenarios test various shapes including rectangles,
 * single points, and multi-frame data.
 */
namespace mask_scenarios {

// ============================================================================
// Core functionality scenarios
// ============================================================================

/**
 * @brief Simple 10x10 rectangular mask for skeletonization
 * 
 * A solid 10x10 rectangle from (1,1) to (10,10).
 * After skeletonization, should have fewer points than original.
 * 
 * Original: 100 points
 * Expected: Fewer points after skeletonization (skeleton is thinner)
 */
inline std::shared_ptr<MaskData> rectangular_mask_10x10() {
    std::vector<uint32_t> x_coords;
    std::vector<uint32_t> y_coords;
    
    for (uint32_t row = 1; row <= 10; ++row) {
        for (uint32_t col = 1; col <= 10; ++col) {
            x_coords.push_back(col);
            y_coords.push_back(row);
        }
    }
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x_coords, y_coords))
        .build();
}

/**
 * @brief Single point mask for skeletonization edge case
 * 
 * A single point at (5, 5). Should remain a single point after skeletonization.
 * 
 * Original: 1 point
 * Expected: 1 point after skeletonization
 */
inline std::shared_ptr<MaskData> single_point_mask_skeletonize() {
    std::vector<uint32_t> x_coords = {5};
    std::vector<uint32_t> y_coords = {5};
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x_coords, y_coords))
        .build();
}

/**
 * @brief Multiple time frames with 5x5 square masks
 * 
 * Creates 5x5 square masks at time frames 100 and 105.
 * Tests that skeletonization processes all time frames.
 * 
 * Expected: Both time frames should have skeletonized results
 */
inline std::shared_ptr<MaskData> multi_frame_masks_skeletonize() {
    std::vector<uint32_t> x_coords;
    std::vector<uint32_t> y_coords;
    
    for (uint32_t row = 1; row <= 5; ++row) {
        for (uint32_t col = 1; col <= 5; ++col) {
            x_coords.push_back(col);
            y_coords.push_back(row);
        }
    }
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x_coords, y_coords))
        .atTime(105, Mask2D(x_coords, y_coords))
        .build();
}

// ============================================================================
// JSON pipeline scenarios
// ============================================================================

/**
 * @brief Rectangular mask for JSON pipeline test
 * 
 * Same as rectangular_mask_10x10, used for JSON pipeline testing.
 * 
 * Original: 100 points
 * Expected: Fewer points after skeletonization
 */
inline std::shared_ptr<MaskData> json_pipeline_rectangular_skeletonize() {
    std::vector<uint32_t> x_coords;
    std::vector<uint32_t> y_coords;
    
    for (uint32_t row = 1; row <= 10; ++row) {
        for (uint32_t col = 1; col <= 10; ++col) {
            x_coords.push_back(col);
            y_coords.push_back(row);
        }
    }
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(x_coords, y_coords))
        .build();
}

} // namespace mask_scenarios

#endif // SKELETONIZE_SCENARIOS_HPP
