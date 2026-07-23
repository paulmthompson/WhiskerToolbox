#ifndef NEURALYZER_V2_MASK_AREA_TRANSFORM_HPP
#define NEURALYZER_V2_MASK_AREA_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <vector>

class Mask2D;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for mask area calculation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * 
 * Example JSON:
 * ```json
 * {
 *   "scale_factor": 1.0,
 *   "min_area": 0.0,
 *   "exclude_holes": false
 * }
 * ```
 */
struct MaskAreaParams {
    /// Scale factor to multiply area by (e.g., convert pixels to mm²)
    /// Must be strictly positive (> 0)
    rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>> scale_factor = 1.0f;

    /// Minimum area threshold - masks below this are reported as 0
    /// Must be non-negative (>= 0)
    rfl::Validator<float, rfl::Minimum<0.0f>> min_area = 0.0f;

    /// Whether to exclude holes when calculating area
    bool exclude_holes = false;
};

/**
 * @brief Calculate area of a single mask
 * 
 * This is the element-level transform: Mask2D → float
 * 
 * When applied to containers:
 * - MaskData (ragged) → RaggedAnalogTimeSeries
 * - SingleMaskData (hypothetical) → AnalogTimeSeries
 * 
 * The raggedness comes from the container structure, not the element output type.
 * 
 * @param mask Input mask
 * @param params Parameters (currently unused)
 * @return Area as number of pixels in the mask
 */
float calculateMaskArea(
        Mask2D const & mask,
        MaskAreaParams const & params);

/**
 * @brief Calculate area with context support
 * 
 * Demonstrates progress reporting and cancellation checking.
 */
float calculateMaskAreaWithContext(
        Mask2D const & mask,
        MaskAreaParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_MASK_AREA_TRANSFORM_HPP
