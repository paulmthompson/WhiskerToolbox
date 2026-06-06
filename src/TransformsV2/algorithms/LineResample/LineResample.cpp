#include "LineResample.hpp"

#include "CoreGeometry/line_resampling.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreMath/parametric_polynomial_utils.hpp"
#include "core/ComputeContext.hpp"

#include <algorithm>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineResampleParams::validate() Implementation
// ============================================================================

void LineResampleParams::validate() {
    polynomial_order = std::max(1, std::min(polynomial_order, 9));
}

namespace {

/**
 * @brief Smooth a line with parametric polynomial fitting and resample at target spacing
 */
Line2D smooth_line_polynomial(Line2D const & line, int order, float target_spacing) {
    if (line.size() <= static_cast<size_t>(order)) {
        return line;
    }

    ParametricCoefficients const coeffs = fit_parametric_polynomials(line, order);
    if (coeffs.success) {
        return generate_smoothed_line(line, coeffs.x_coeffs, coeffs.y_coeffs, order, target_spacing);
    }
    return resample_line_points(line, target_spacing);
}

}// namespace

// ============================================================================
// Transform Implementation
// ============================================================================

Line2D resampleLine(
        Line2D const & line,
        LineResampleParams const & params) {

    if (line.empty()) {
        return line;
    }

    int const polynomial_order = std::max(1, std::min(params.polynomial_order, 9));

    switch (params.method) {
        case LineResampleMethod::FixedSpacing:
            if (line.size() <= 2) {
                return line;
            }
            return resample_line_points(line, params.target_spacing.value());
        case LineResampleMethod::DouglasPeucker:
            if (line.size() <= 2) {
                return line;
            }
            return douglas_peucker_simplify(line, params.epsilon.value());
        case LineResampleMethod::PolynomialSmooth:
            if (line.size() <= 2) {
                return line;
            }
            return smooth_line_polynomial(line, polynomial_order, params.target_spacing.value());
        default:
            return line;
    }
}

Line2D resampleLineWithContext(
        Line2D const & line,
        LineResampleParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return line;// Return unchanged on cancellation
    }

    auto const result = resampleLine(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
