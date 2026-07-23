#include "LinePointExtraction.hpp"

#include "core/ComputeContext.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreMath/parametric_polynomial_utils.hpp"

#include <algorithm>
#include <cmath>

namespace Neuralyzer::Transforms::V2::Examples {

// ============================================================================
// LinePointExtractionParams::validate() Implementation
// ============================================================================

void LinePointExtractionParams::validate() {
    position = std::max(0.0f, std::min(position, 1.0f));
    polynomial_order = std::max(1, std::min(polynomial_order, 9));
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

    if (params.method == LinePointExtractionMethod::Direct) {
        // Use distance-based extraction from CoreGeometry
        auto result = point_at_fractional_position(
                line,
                params.position,
                params.use_interpolation);
        
        if (result.has_value()) {
            return result.value();
        }
        // Fallback to first point if extraction fails
        return Point2D<float>(line[0].x, line[0].y);
    } else {
        // Use parametric polynomial fitting
        auto result = extract_parametric_point(
                line,
                params.position,
                params.polynomial_order);
        
        if (result.has_value()) {
            return result.value();
        }
        // Fallback to direct method if parametric fails
        auto direct_result = point_at_fractional_position(
                line,
                params.position,
                params.use_interpolation);
        
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

}// namespace Neuralyzer::Transforms::V2::Examples
