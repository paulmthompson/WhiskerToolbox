#ifndef HOLE_FILLING_SCENARIOS_HPP
#define HOLE_FILLING_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include "fixtures/scenarios/mask/area_scenarios.hpp" // For empty_mask_data()
#include <memory>

/**
 * @brief Hole filling test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask hole filling
 * algorithms. These scenarios test various edge cases including hollow
 * shapes that need filling and solid shapes that should remain unchanged.
 */
namespace mask_scenarios {

// ============================================================================
// Core functionality scenarios
// ============================================================================

/**
 * @brief Hollow rectangle with hole in the middle (6x6 outer, 4x4 hole)
 * 
 * Creates a hollow rectangle from (2,2) to (7,7) with only the border filled.
 * The interior (3,3) to (6,6) is empty and should be filled.
 * 
 * Original: 20 border pixels
 * Expected after fill: 36 pixels (6x6 solid square)
 * Interior point (4,4) should be present after filling
 */
inline std::shared_ptr<MaskData> hollow_rectangle_6x6() {
    std::vector<Point2D<uint32_t>> hollow_rect;
    for (uint32_t row = 2; row < 8; ++row) {
        for (uint32_t col = 2; col < 8; ++col) {
            if (row == 2 || row == 7 || col == 2 || col == 7) {
                hollow_rect.emplace_back(col, row);
            }
        }
    }
    return MaskDataBuilder()
        .withImageSize(10, 10)
        .atTime(0, Mask2D(hollow_rect))
        .build();
}

/**
 * @brief Solid 3x3 square (no holes to fill)
 * 
 * Creates a solid 3x3 square from (2,2) to (4,4).
 * 
 * Original: 9 pixels
 * Expected after fill: 9 pixels (unchanged)
 */
inline std::shared_ptr<MaskData> solid_square_3x3() {
    std::vector<Point2D<uint32_t>> solid_square;
    for (uint32_t row = 2; row < 5; ++row) {
        for (uint32_t col = 2; col < 5; ++col) {
            solid_square.emplace_back(col, row);
        }
    }
    return MaskDataBuilder()
        .withImageSize(8, 8)
        .atTime(1, Mask2D(solid_square))
        .build();
}

/**
 * @brief Multiple masks at same time: hollow 4x4 rectangle + solid 2x2 square
 * 
 * First mask: Hollow 4x4 rectangle (1,1) to (4,4), border only -> 12 border pixels
 * Second mask: Solid 2x2 square (7,1) to (8,2) -> 4 pixels
 * 
 * Expected after fill:
 * - First mask: 16 pixels (4x4 solid, hole filled)
 * - Second mask: 4 pixels (unchanged)
 */
inline std::shared_ptr<MaskData> multiple_masks_hollow_and_solid() {
    // First mask: hollow rectangle (4x4 with hole in middle)
    std::vector<Point2D<uint32_t>> hollow_rect;
    for (uint32_t row = 1; row < 5; ++row) {
        for (uint32_t col = 1; col < 5; ++col) {
            if (row == 1 || row == 4 || col == 1 || col == 4) {
                hollow_rect.emplace_back(col, row);
            }
        }
    }
    
    // Second mask: small solid 2x2 square
    std::vector<Point2D<uint32_t>> solid_square;
    for (uint32_t row = 1; row < 3; ++row) {
        for (uint32_t col = 7; col < 9; ++col) {
            solid_square.emplace_back(col, row);
        }
    }
    
    return MaskDataBuilder()
        .withImageSize(12, 8)
        .atTime(2, Mask2D(hollow_rect))
        .atTime(2, Mask2D(solid_square))
        .build();
}

// ============================================================================
// Operation interface scenarios
// ============================================================================

/**
 * @brief Donut shape for operation execute test (4x4 with 2x2 hole)
 * 
 * Creates a donut/ring shape: outer ring from (1,1) to (4,4) with
 * interior (2,2) to (3,3) empty.
 * 
 * Original: 12 border pixels
 * Expected after fill: 16 pixels (4x4 solid)
 */
inline std::shared_ptr<MaskData> donut_shape_4x4() {
    std::vector<Point2D<uint32_t>> donut;
    for (uint32_t row = 1; row < 5; ++row) {
        for (uint32_t col = 1; col < 5; ++col) {
            if (row == 1 || row == 4 || col == 1 || col == 4) {
                donut.emplace_back(col, row);
            }
        }
    }
    return MaskDataBuilder()
        .withImageSize(6, 6)
        .atTime(0, Mask2D(donut))
        .build();
}

// ============================================================================
// JSON pipeline scenarios
// ============================================================================

/**
 * @brief Hollow rectangle for JSON pipeline test
 * 
 * Same as hollow_rectangle_6x6 but at TimeFrameIndex(0).
 * Used for testing JSON pipeline execution.
 * 
 * Original: 20 border pixels
 * Expected after fill: 36 pixels (6x6 solid square)
 * Interior point (4,4) should be present after filling
 */
inline std::shared_ptr<MaskData> json_pipeline_hollow_rectangle_hole_filling() {
    std::vector<Point2D<uint32_t>> hollow_rect;
    for (uint32_t row = 2; row < 8; ++row) {
        for (uint32_t col = 2; col < 8; ++col) {
            if (row == 2 || row == 7 || col == 2 || col == 7) {
                hollow_rect.emplace_back(col, row);
            }
        }
    }
    return MaskDataBuilder()
        .withImageSize(10, 10)
        .atTime(0, Mask2D(hollow_rect))
        .build();
}

/**
 * @brief Multiple masks for JSON pipeline multi-mask test
 * 
 * Same as multiple_masks_hollow_and_solid but at TimeFrameIndex(0).
 * Used for testing JSON pipeline with multiple masks.
 * 
 * Expected after fill:
 * - One mask with 4 points (2x2 square unchanged)
 * - One mask with 16 points (4x4 hollow filled to solid)
 */
inline std::shared_ptr<MaskData> json_pipeline_multi_mask_hole_filling() {
    // First mask: hollow rectangle (4x4 with hole in middle)
    std::vector<Point2D<uint32_t>> hollow_rect;
    for (uint32_t row = 1; row < 5; ++row) {
        for (uint32_t col = 1; col < 5; ++col) {
            if (row == 1 || row == 4 || col == 1 || col == 4) {
                hollow_rect.emplace_back(col, row);
            }
        }
    }
    
    // Second mask: small solid 2x2 square
    std::vector<Point2D<uint32_t>> solid_square;
    for (uint32_t row = 1; row < 3; ++row) {
        for (uint32_t col = 7; col < 9; ++col) {
            solid_square.emplace_back(col, row);
        }
    }
    
    return MaskDataBuilder()
        .withImageSize(12, 8)
        .atTime(0, Mask2D(hollow_rect))
        .atTime(0, Mask2D(solid_square))
        .build();
}

} // namespace mask_scenarios

#endif // HOLE_FILLING_SCENARIOS_HPP
