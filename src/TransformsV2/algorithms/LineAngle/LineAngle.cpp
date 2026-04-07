#include "LineAngle.hpp"

#include "CoreGeometry/angle.hpp"
#include "CoreGeometry/lines.hpp"
#include "core/ComputeContext.hpp"
#include "CoreMath/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineAngleParams::validate() Implementation
// ============================================================================

void LineAngleParams::validate() {
    // Clamp position to [0, 1]
    position = std::max(0.0f, std::min(position, 1.0f));

    // Normalize reference vector
    float ref_x = reference_x;
    float ref_y = reference_y;
    
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

    if (params.method == LineAngleMethod::DirectPoints) {
        return calculate_direct_angle(
            line, 
            params.position, 
            params.reference_x, 
            params.reference_y);
    } else {
        return calculate_polynomial_angle(
            line, 
            params.position, 
            params.polynomial_order, 
            params.reference_x, 
            params.reference_y);
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
