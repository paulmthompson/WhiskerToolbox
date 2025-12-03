#include "MaskArea.hpp"

#include "CoreGeometry/masks.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

float calculateMaskArea(
        Mask2D const & mask,
        MaskAreaParams const & params) {

    // Count pixels in the mask
    int count = 0;
    for (auto const & pixel: mask) {
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
float calculateMaskAreaWithContext(
        Mask2D const & mask,
        MaskAreaParams const & params,
        ComputeContext const & ctx) {

    (void) params;

    int count = 0;
    int total_pixels = static_cast<int>(mask.size());// Count of pixels in mask
    int processed = 0;

    for (auto const & pixel: mask) {
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

    return static_cast<float>(count);
}


}// namespace WhiskerToolbox::Transforms::V2::Examples