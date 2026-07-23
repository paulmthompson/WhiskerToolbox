/**
 * @file MaskToLine.cpp
 * @brief Implementation of element-level mask-to-line transform (Mask2D → Line2D).
 */

#include "MaskToLine.hpp"

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/order_line.hpp"
#include "CoreGeometry/points.hpp"
#include "core/ComputeContext.hpp"

#include <algorithm>

namespace Neuralyzer::Transforms::V2::Examples {

Line2D maskToLine(
        Mask2D const & mask,
        MaskToLineParams const & params) {

    if (mask.empty()) {
        return {};
    }

    int const subsample = std::max(1, params.subsample_factor);
    Point2D<float> const origin{params.reference_x, params.reference_y};
    return order_line(mask.points(), origin, subsample);
}

Line2D maskToLineWithContext(
        Mask2D const & mask,
        MaskToLineParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(0);

    auto const result = maskToLine(mask, params);

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(100);
    return result;
}

}// namespace Neuralyzer::Transforms::V2::Examples
