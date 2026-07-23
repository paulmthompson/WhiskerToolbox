#include "MaskArea.hpp"

#include "CoreGeometry/masks.hpp"
#include "core/ComputeContext.hpp"

namespace Neuralyzer::Transforms::V2::Examples {

float calculateMaskArea(
        Mask2D const & mask,
        MaskAreaParams const & params) {

    // Count pixels in the mask
    int count = 0;
    for (auto const & pixel: mask) {
        ++count;
    }

    float area = static_cast<float>(count);

    if (area < params.min_area.value()) {
        return 0.0f;
    }

    return area * params.scale_factor.value();
}

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


}// namespace Neuralyzer::Transforms::V2::Examples