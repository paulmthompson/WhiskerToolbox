#include "MaskCentroid.hpp"

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

Point2D<float> calculateMaskCentroid(
        Mask2D const & mask,
        MaskCentroidParams const & params) {

    (void) params;// Parameters not used currently

    if (mask.empty()) {
        return Point2D<float>(0.0f, 0.0f);
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;

    for (auto const & pixel: mask) {
        sum_x += static_cast<float>(pixel.x);
        sum_y += static_cast<float>(pixel.y);
    }

    auto const count = static_cast<float>(mask.size());
    return Point2D<float>(sum_x / count, sum_y / count);
}

Point2D<float> calculateMaskCentroidWithContext(
        Mask2D const & mask,
        MaskCentroidParams const & params,
        ComputeContext const & ctx) {

    (void) params;// Parameters not used currently

    if (mask.empty()) {
        ctx.reportProgress(100);
        return Point2D<float>(0.0f, 0.0f);
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    int const total_pixels = static_cast<int>(mask.size());
    int processed = 0;

    for (auto const & pixel: mask) {
        if (ctx.shouldCancel()) {
            throw std::runtime_error("Computation cancelled");
        }

        sum_x += static_cast<float>(pixel.x);
        sum_y += static_cast<float>(pixel.y);
        ++processed;

        if (total_pixels > 0) {
            int progress = (processed * 100) / total_pixels;
            ctx.reportProgress(progress);
        }
    }

    auto const count = static_cast<float>(mask.size());
    return Point2D<float>(sum_x / count, sum_y / count);
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
