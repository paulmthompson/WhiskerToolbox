/**
 * @file RemoveLineOutliers.cpp
 * @brief Implementation of element-level geometric outlier removal (Line2D → Line2D).
 */

#include "RemoveLineOutliers.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreMath/parametric_polynomial_utils.hpp"
#include "core/ComputeContext.hpp"

#include <algorithm>

namespace WhiskerToolbox::Transforms::V2::Examples {

void RemoveLineOutliersParams::validate() {
    polynomial_order = std::max(1, std::min(polynomial_order, 9));
}

Line2D removeLineOutliers(
        Line2D const & line,
        RemoveLineOutliersParams const & params) {

    if (line.empty()) {
        return line;
    }

    int const polynomial_order = std::max(1, std::min(params.polynomial_order, 9));

    if (line.size() <= static_cast<size_t>(polynomial_order + 2)) {
        return line;
    }

    return remove_outliers(line, params.error_threshold, polynomial_order);
}

Line2D removeLineOutliersWithContext(
        Line2D const & line,
        RemoveLineOutliersParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return line;
    }

    auto const result = removeLineOutliers(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
