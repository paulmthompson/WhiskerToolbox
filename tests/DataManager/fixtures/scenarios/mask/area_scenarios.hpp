#ifndef AREA_SCENARIOS_HPP
#define AREA_SCENARIOS_HPP

#include "fixtures/builders/MaskDataBuilder.hpp"
#include <memory>

/**
 * @brief Area calculation test scenarios for MaskData
 * 
 * This namespace contains pre-configured test data for mask area
 * calculation algorithms. These scenarios test various edge cases
 * and common patterns.
 */
namespace mask_scenarios {

/**
 * @brief Empty mask data (no masks)
 * 
 * Expected: Empty result (no area values)
 */
inline std::shared_ptr<MaskData> empty_mask_data() {
    return MaskDataBuilder().build();
}

/**
 * @brief Single mask at one timestamp
 * 
 * Mask at t=10: 3 pixels at (1,1), (2,2), (3,3)
 * 
 * Expected area: 3.0 pixels
 */
inline std::shared_ptr<MaskData> single_mask_single_timestamp() {
    std::vector<uint32_t> xs = {1, 2, 3};
    std::vector<uint32_t> ys = {1, 2, 3};
    return MaskDataBuilder()
        .atTime(10, Mask2D(xs, ys))
        .build();
}

/**
 * @brief Multiple masks at single timestamp (should sum in v1, separate in v2)
 * 
 * Mask 1 at t=20: 3 pixels
 * Mask 2 at t=20: 5 pixels
 * 
 * V1 Expected: 8.0 (summed)
 * V2 Expected: [3.0, 5.0] (individual areas)
 */
inline std::shared_ptr<MaskData> multiple_masks_single_timestamp() {
    std::vector<uint32_t> xs1 = {1, 2, 3};
    std::vector<uint32_t> ys1 = {1, 2, 3};
    std::vector<uint32_t> xs2 = {4, 5, 6, 7, 8};
    std::vector<uint32_t> ys2 = {4, 5, 6, 7, 8};
    return MaskDataBuilder()
        .atTime(20, Mask2D(xs1, ys1))
        .atTime(20, Mask2D(xs2, ys2))
        .build();
}

/**
 * @brief Single masks across multiple timestamps
 * 
 * Mask at t=30: 2 pixels
 * Mask 1 at t=40: 3 pixels
 * Mask 2 at t=40: 4 pixels
 * 
 * V1 Expected: {30: 2.0, 40: 7.0}
 * V2 Expected: {30: [2.0], 40: [3.0, 4.0]}
 */
inline std::shared_ptr<MaskData> masks_multiple_timestamps() {
    std::vector<uint32_t> xs1 = {1, 2};
    std::vector<uint32_t> ys1 = {1, 2};
    std::vector<uint32_t> xs2 = {1, 2, 3};
    std::vector<uint32_t> ys2 = {1, 2, 3};
    std::vector<uint32_t> xs3 = {4, 5, 6, 7};
    std::vector<uint32_t> ys3 = {4, 5, 6, 7};
    return MaskDataBuilder()
        .atTime(30, Mask2D(xs1, ys1))
        .atTime(40, Mask2D(xs2, ys2))
        .atTime(40, Mask2D(xs3, ys3))
        .build();
}

/**
 * @brief Box-shaped mask
 * 
 * Box at t=50: 10x10 = 100 pixels
 * 
 * Expected area: 100.0 pixels
 */
inline std::shared_ptr<MaskData> box_mask() {
    return MaskDataBuilder()
        .withBox(50, 10, 10, 10, 10)
        .build();
}

/**
 * @brief Circular mask (approximate)
 * 
 * Circle at t=60: radius 5 pixels
 * 
 * Expected area: ~78 pixels (π * 5^2 ≈ 78.5)
 */
inline std::shared_ptr<MaskData> circle_mask() {
    return MaskDataBuilder()
        .withCircle(60, 50, 50, 5)
        .build();
}

/**
 * @brief Empty mask at a timestamp (zero pixels)
 * 
 * Empty mask at t=10
 * 
 * Expected area: 0.0 pixels
 */
inline std::shared_ptr<MaskData> empty_mask_at_timestamp() {
    return MaskDataBuilder()
        .withEmpty(10)
        .build();
}

/**
 * @brief Mixed empty and non-empty masks at same timestamp
 * 
 * Empty mask at t=20
 * Non-empty mask at t=20: 3 pixels
 * 
 * V1 Expected: 3.0 (sum includes empty)
 * V2 Expected: [0.0, 3.0]
 */
inline std::shared_ptr<MaskData> mixed_empty_nonempty() {
    std::vector<uint32_t> xs = {1, 2, 3};
    std::vector<uint32_t> ys = {1, 2, 3};
    return MaskDataBuilder()
        .withEmpty(20)
        .atTime(20, Mask2D(xs, ys))
        .build();
}

/**
 * @brief Large number of masks at one timestamp (stress test)
 * 
 * 10 masks at t=30, each with varying sizes
 * 
 * Tests performance and handling of many masks
 */
inline std::shared_ptr<MaskData> large_mask_count() {
    auto builder = MaskDataBuilder();
    for (int i = 0; i < 10; ++i) {
        std::vector<uint32_t> xs, ys;
        for (uint32_t j = 0; j <= static_cast<uint32_t>(i); ++j) {
            xs.push_back(i * 10 + j);
            ys.push_back(i * 10 + j);
        }
        builder.atTime(30, Mask2D(xs, ys));
    }
    return builder.build();
}

/**
 * @brief Two masks at different timestamps for JSON pipeline tests
 * 
 * Mask at t=100: 3 pixels
 * Mask at t=200: 4 pixels
 * 
 * V1 Expected: {100: 3.0, 200: 4.0}
 * V2 Expected: {100: [3.0], 200: [4.0]}
 */
inline std::shared_ptr<MaskData> json_pipeline_basic() {
    std::vector<uint32_t> xs1 = {1, 2, 3};
    std::vector<uint32_t> ys1 = {1, 2, 3};
    std::vector<uint32_t> xs2 = {4, 5, 6, 7};
    std::vector<uint32_t> ys2 = {4, 5, 6, 7};
    return MaskDataBuilder()
        .atTime(100, Mask2D(xs1, ys1))
        .atTime(200, Mask2D(xs2, ys2))
        .build();
}

/**
 * @brief Multiple timestamps for comprehensive JSON tests
 * 
 * Masks at t=100, t=200, t=300
 * Each with varying pixel counts
 * 
 * V1 Expected: {100: 3.0, 200: 5.0, 300: 2.0}
 * V2 Expected: {100: [3.0], 200: [5.0], 300: [2.0]}
 */
inline std::shared_ptr<MaskData> json_pipeline_multi_timestamp() {
    std::vector<uint32_t> xs1 = {1, 2, 3};
    std::vector<uint32_t> ys1 = {1, 2, 3};
    std::vector<uint32_t> xs2 = {4, 5, 6, 7, 8};
    std::vector<uint32_t> ys2 = {4, 5, 6, 7, 8};
    std::vector<uint32_t> xs3 = {9, 10};
    std::vector<uint32_t> ys3 = {9, 10};
    return MaskDataBuilder()
        .atTime(100, Mask2D(xs1, ys1))
        .atTime(200, Mask2D(xs2, ys2))
        .atTime(300, Mask2D(xs3, ys3))
        .build();
}

/**
 * @brief Multiple masks at same timestamp for JSON tests
 * 
 * 2 masks at t=500 with different sizes
 * 
 * V1 Expected: {500: 5.0} (2 + 3)
 * V2 Expected: {500: [2.0, 3.0]}
 */
inline std::shared_ptr<MaskData> json_pipeline_multi_mask() {
    std::vector<uint32_t> xs1 = {1, 2};
    std::vector<uint32_t> ys1 = {1, 2};
    std::vector<uint32_t> xs2 = {3, 4, 5};
    std::vector<uint32_t> ys2 = {3, 4, 5};
    return MaskDataBuilder()
        .atTime(500, Mask2D(xs1, ys1))
        .atTime(500, Mask2D(xs2, ys2))
        .build();
}

/**
 * @brief Single mask for statistics verification
 * 
 * Mask at t=100: 4 pixels at (1,1), (2,2), (3,3), (4,4)
 * 
 * Expected: mean=4.0, min=4.0, max=4.0, area=4.0
 */
inline std::shared_ptr<MaskData> single_mask_for_statistics() {
    std::vector<uint32_t> xs = {1, 2, 3, 4};
    std::vector<uint32_t> ys = {1, 2, 3, 4};
    return MaskDataBuilder()
        .atTime(100, Mask2D(xs, ys))
        .build();
}

} // namespace mask_scenarios

#endif // AREA_SCENARIOS_HPP
