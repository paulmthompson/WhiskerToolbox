#ifndef MASK_TO_LINE_SCENARIOS_HPP
#define MASK_TO_LINE_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include <memory>

/**
 * @brief Mask to line conversion test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask to line
 * conversion algorithms. These scenarios test various mask shapes
 * and edge cases relevant to line extraction.
 */
namespace mask_to_line_scenarios {

/**
 * @brief Simple 5x5 rectangular mask for basic skeletonization
 * 
 * Box at t=100: 5x5 = 25 pixels starting at (10,10)
 * 
 * Expected: Line extraction should produce a valid skeleton/line
 */
inline std::shared_ptr<MaskData> simple_rectangle() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 5)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief L-shaped mask for nearest-to-reference method testing
 * 
 * Rectangle at t=100: 5x5 at (10,10) plus horizontal extension
 * 
 * Expected: Line should follow the L-shape structure
 */
inline std::shared_ptr<MaskData> l_shaped_mask() {
    // Create L-shape: 5x5 box + horizontal extension at bottom
    std::vector<uint32_t> xs, ys;
    
    // 5x5 box portion
    for (uint32_t dy = 0; dy < 5; ++dy) {
        for (uint32_t dx = 0; dx < 5; ++dx) {
            xs.push_back(10 + dx);
            ys.push_back(10 + dy);
        }
    }
    
    // Horizontal extension at y=14 (bottom of box)
    for (uint32_t dx = 5; dx < 10; ++dx) {
        xs.push_back(10 + dx);
        ys.push_back(14);
    }
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(xs, ys))
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief Simple 5x3 rectangular mask for smoothing tests
 * 
 * Box at t=100: 5 wide x 3 tall = 15 pixels
 * 
 * Expected: Thin rectangular mask for smoothing algorithm testing
 */
inline std::shared_ptr<MaskData> thin_rectangle() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 3)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief Two masks at different time frames
 * 
 * Box 1 at t=100: 5x2 at (10,10)
 * Box 2 at t=200: 5x2 at (20,20)
 * 
 * Expected: Two separate lines, one at each time frame
 */
inline std::shared_ptr<MaskData> multiple_time_frames() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 2)
        .withBox(200, 20, 20, 5, 2)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief Empty mask data (no masks)
 * 
 * Expected: Empty result (no lines generated)
 */
inline std::shared_ptr<MaskData> empty_mask_data() {
    return MaskDataBuilder()
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief Single point mask (edge case)
 * 
 * Single pixel at t=100: (10,10)
 * 
 * Expected: May not produce a line, but should not crash
 */
inline std::shared_ptr<MaskData> single_point() {
    return MaskDataBuilder()
        .withPoint(100, 10, 10)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief Horizontal line mask (5 pixels in a row)
 * 
 * Line at t=100: 5 pixels from (10,10) to (14,10)
 * 
 * Expected: Tests edge cases with linear masks
 */
inline std::shared_ptr<MaskData> horizontal_line_mask() {
    std::vector<uint32_t> xs = {10, 11, 12, 13, 14};
    std::vector<uint32_t> ys = {10, 10, 10, 10, 10};
    
    return MaskDataBuilder()
        .atTime(100, Mask2D(xs, ys))
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief 5x2 thin mask for polynomial order edge cases
 * 
 * Box at t=100: 5x2 = 10 pixels
 * 
 * Expected: Tests high polynomial order with few points
 */
inline std::shared_ptr<MaskData> thin_mask_few_points() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 2)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief 5x3 mask for subsample factor testing
 * 
 * Box at t=100: 5x3 = 15 pixels
 * 
 * Expected: Tests high subsample factor handling
 */
inline std::shared_ptr<MaskData> subsample_test_mask() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 3)
        .withImageSize(100, 100)
        .build();
}

/**
 * @brief JSON pipeline test mask (5x5 box)
 * 
 * Box at t=100: 5x5 = 25 pixels
 * 
 * Expected: Standard mask for JSON pipeline testing
 */
inline std::shared_ptr<MaskData> json_pipeline_mask() {
    return MaskDataBuilder()
        .withBox(100, 10, 10, 5, 5)
        .withImageSize(100, 100)
        .build();
}

} // namespace mask_to_line_scenarios

#endif // MASK_TO_LINE_SCENARIOS_HPP
