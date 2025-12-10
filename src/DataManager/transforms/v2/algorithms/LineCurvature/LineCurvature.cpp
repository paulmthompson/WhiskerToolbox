#include "LineCurvature.hpp"

#include "CoreGeometry/lines.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineCurvatureParams::validate() Implementation
// ============================================================================

void LineCurvatureParams::validate() {
    // Clamp position to [0, 1]
    float pos = getPosition();
    position = std::max(0.0f, std::min(pos, 1.0f));

    // Clamp fitting_window_percentage to [0, 1]
    float window = getFittingWindowPercentage();
    fitting_window_percentage = std::max(0.0f, std::min(window, 1.0f));

    // Clamp polynomial_order to [2, 9]
    int order = getPolynomialOrder();
    polynomial_order = std::max(2, std::min(order, 9));
}

// ============================================================================
// Transform Implementation
// ============================================================================

float calculateLineCurvature(
        Line2D const & line,
        LineCurvatureParams const & params) {
    
    if (line.size() < 2) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // Use the existing parametric polynomial curvature utility
    auto curvature_opt = calculate_polynomial_curvature(
        line,
        params.getPosition(),
        params.getPolynomialOrder(),
        params.getFittingWindowPercentage());

    if (curvature_opt.has_value()) {
        return curvature_opt.value();
    }
    
    return std::numeric_limits<float>::quiet_NaN();
}

float calculateLineCurvatureWithContext(
        Line2D const & line,
        LineCurvatureParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto result = calculateLineCurvature(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
