#ifndef WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_MASK_AREA_TRANSFORM_HPP

#include "CoreGeometry/masks.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "transforms/v2/core/ElementTransform.hpp"

#include <rfl.hpp>  // reflect-cpp
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for mask area calculation
 * 
 * Using reflect-cpp for automatic JSON serialization
 */
struct MaskAreaParams {
    // For now just holds the number of mask values to return per Mask2D
    // This demonstrates outputting a std::vector<float> for each Mask2D
    // Could add: bool exclude_holes = false; float scale_factor = 1.0f;
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
    
    (void)params;  // Unused for now
    
    // Count pixels in the mask
    int count = 0;
    for (auto const& pixel : mask) {
        ++count;
    }
    
    return static_cast<float>(count);
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
