#ifndef WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <vector>

class Mask2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for mask area calculation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * Optional fields can be omitted from JSON and will use default values.
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
    // Scale factor to multiply area by (e.g., convert pixels to mm²)
    // Must be strictly positive (> 0)
    std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>> scale_factor;

    // Minimum area threshold - masks below this are reported as 0
    // Must be non-negative (>= 0)
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> min_area;

    // Whether to exclude holes when calculating area
    std::optional<bool> exclude_holes;

    // Helper methods to get values with defaults
    float getScaleFactor() const {
        return scale_factor.has_value() ? scale_factor.value().value() : 1.0f;
    }

    float getMinArea() const {
        return min_area.has_value() ? min_area.value().value() : 0.0f;
    }

    bool getExcludeHoles() const {
        return exclude_holes.value_or(false);
    }
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
 * @brief Alternative: Calculate area with context support
 * 
 * Demonstrates progress reporting and cancellation checking.
 */
float calculateMaskAreaWithContext(
        Mask2D const & mask,
        MaskAreaParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP
