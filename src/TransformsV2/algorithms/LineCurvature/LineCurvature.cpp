#include "LineCurvature.hpp"

#include "core/ComputeContext.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreMath/parametric_polynomial_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Neuralyzer::Transforms::V2::Examples {

// ============================================================================
// LineCurvatureParams::validate() Implementation
// ============================================================================

void LineCurvatureParams::validate() {
    position = std::max(0.0f, std::min(position, 1.0f));
    fitting_window_percentage = std::max(0.0f, std::min(fitting_window_percentage, 1.0f));
    polynomial_order = std::max(2, std::min(polynomial_order, 9));
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
            params.position,
            params.polynomial_order,
            params.fitting_window_percentage);

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

}// namespace Neuralyzer::Transforms::V2::Examples
