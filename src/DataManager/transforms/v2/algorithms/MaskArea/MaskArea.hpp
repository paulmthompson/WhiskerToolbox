#ifndef WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP

#include "CoreGeometry/masks.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "transforms/v2/core/ElementTransform.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <vector>

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
inline float calculateMaskArea(
    Mask2D const& mask, 
    MaskAreaParams const& params) {
    
    // Count pixels in the mask
    int count = 0;
    for (auto const& pixel : mask) {
        ++count;
    }
    
    float area = static_cast<float>(count);
    
    // Apply minimum area threshold
    if (area < params.getMinArea()) {
        return 0.0f;
    }
    
    // Apply scale factor
    return area * params.getScaleFactor();
}

/**
 * @brief Alternative: Calculate area with context support
 * 
 * Demonstrates progress reporting and cancellation checking.
 */
inline std::vector<float> calculateMaskAreaWithContext(
    Mask2D const& mask,
    MaskAreaParams const& params,
    ComputeContext const& ctx) {
    
    (void)params;
    
    int count = 0;
    int total_pixels = static_cast<int>(mask.size());  // Count of pixels in mask
    int processed = 0;
    
    for (auto const& pixel : mask) {
        // Check for cancellation periodically
        if (ctx.shouldCancel()) {
            throw std::runtime_error("Computation cancelled");
        }
        
        ++count;
        ++processed;
        
        // Report progress
        if (total_pixels > 0) {
            int progress = (processed * 100) / total_pixels;
            ctx.reportProgress(progress);
        }
    }
    
    return {static_cast<float>(count)};
}

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP
