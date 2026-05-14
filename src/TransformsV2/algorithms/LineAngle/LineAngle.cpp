#include "LineAngle.hpp"

#include "CoreGeometry/angle.hpp"
#include "CoreGeometry/lines.hpp"
#include "core/ComputeContext.hpp"
#include "CoreMath/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WhiskerToolbox::Transforms::V2::Examples {

void LineAngleParams::validate() {
    position = std::clamp(position, 0.0f, 1.0f);
    window = std::clamp(window, 0.0f, 1.0f);
}

float calculateLineAngle(
        Line2D const & line,
        LineAngleParams const & params) {

    if (line.size() < 2) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    PlanarOrthonormalBasis2D const basis = make_planar_orthonormal_basis(
            params.axis_x_x,
            params.axis_x_y,
            params.axis_y_x,
            params.axis_y_y);

    ArcLengthFractionSpan const span = compute_sliding_secant_span(params.position, params.window);

    if (params.method == LineAngleMethod::DirectPoints) {
        return calculate_direct_angle_secant(line, span.t_low, span.t_high, basis);
    }
    return calculate_polynomial_angle(
            line,
            params.position,
            params.window,
            params.polynomial_order,
            basis);
}

float calculateLineAngleWithContext(
        Line2D const & line,
        LineAngleParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float const result = calculateLineAngle(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
