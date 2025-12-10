#include "LinePointExtraction.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"

#include <algorithm>
#include <cmath>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LinePointExtractionParams::validate() Implementation
// ============================================================================

void LinePointExtractionParams::validate() {
    // Clamp position to [0, 1]
    float pos = getPosition();
    position = std::max(0.0f, std::min(pos, 1.0f));

    // Clamp polynomial order to valid range [1, 9]
    int order = getPolynomialOrder();
    polynomial_order = std::max(1, std::min(order, 9));
}

// ============================================================================
// Transform Implementation
// ============================================================================

Point2D<float> extractLinePoint(
        Line2D const & line,
        LinePointExtractionParams const & params) {
    
    // Handle edge cases
    if (line.empty()) {
        return Point2D<float>(0.0f, 0.0f);
    }
    
    if (line.size() == 1) {
        return Point2D<float>(line[0].x, line[0].y);
    }

    if (params.getMethod() == LinePointExtractionMethod::Direct) {
        // Use distance-based extraction from CoreGeometry
        auto result = point_at_fractional_position(
            line, 
            params.getPosition(), 
            params.getUseInterpolation());
        
        if (result.has_value()) {
            return result.value();
        }
        // Fallback to first point if extraction fails
        return Point2D<float>(line[0].x, line[0].y);
    } else {
        // Use parametric polynomial fitting
        auto result = extract_parametric_point(
            line, 
            params.getPosition(), 
            params.getPolynomialOrder());
        
        if (result.has_value()) {
            return result.value();
        }
        // Fallback to direct method if parametric fails
        auto direct_result = point_at_fractional_position(
            line, 
            params.getPosition(), 
            params.getUseInterpolation());
        
        if (direct_result.has_value()) {
            return direct_result.value();
        }
        return Point2D<float>(line[0].x, line[0].y);
    }
}

Point2D<float> extractLinePointWithContext(
        Line2D const & line,
        LinePointExtractionParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return Point2D<float>(0.0f, 0.0f);
    }

    auto result = extractLinePoint(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
