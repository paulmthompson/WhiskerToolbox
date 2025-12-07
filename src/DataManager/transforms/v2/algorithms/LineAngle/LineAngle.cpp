#include "LineAngle.hpp"

#include "CoreGeometry/angle.hpp"
#include "CoreGeometry/lines.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineAngleParams::validate() Implementation
// ============================================================================

void LineAngleParams::validate() {
    // Clamp position to [0, 1]
    float pos = getPosition();
    position = std::max(0.0f, std::min(pos, 1.0f));

    // Normalize reference vector
    float ref_x = getReferenceX();
    float ref_y = getReferenceY();
    
    if (ref_x != 0.0f || ref_y != 0.0f) {
        float length = std::sqrt(ref_x * ref_x + ref_y * ref_y);
        if (length > 0.0f) {
            reference_x = ref_x / length;
            reference_y = ref_y / length;
        }
    } else {
        // Default to x-axis if zero vector
        reference_x = 1.0f;
        reference_y = 0.0f;
    }
}

// ============================================================================
// Transform Implementation
// ============================================================================

float calculateLineAngle(
        Line2D const & line,
        LineAngleParams const & params) {
    
    if (line.size() < 2) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (params.getMethod() == LineAngleMethod::DirectPoints) {
        return calculate_direct_angle(
            line, 
            params.getPosition(), 
            params.getReferenceX(), 
            params.getReferenceY());
    } else {
        return calculate_polynomial_angle(
            line, 
            params.getPosition(), 
            params.getPolynomialOrder(), 
            params.getReferenceX(), 
            params.getReferenceY());
    }
}

float calculateLineAngleWithContext(
        Line2D const & line,
        LineAngleParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto result = calculateLineAngle(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
