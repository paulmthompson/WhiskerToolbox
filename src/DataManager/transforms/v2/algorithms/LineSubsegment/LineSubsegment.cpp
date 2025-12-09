#include "LineSubsegment.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "utils/polynomial/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineSubsegmentParams::validate() Implementation
// ============================================================================

void LineSubsegmentParams::validate() {
    // Clamp positions to [0, 1]
    float start = getStartPosition();
    float end = getEndPosition();
    
    start = std::max(0.0f, std::min(start, 1.0f));
    end = std::max(0.0f, std::min(end, 1.0f));
    
    start_position = start;
    end_position = end;
    
    // Clamp polynomial order to reasonable range
    int order = getPolynomialOrder();
    order = std::max(1, std::min(order, 9));
    polynomial_order = order;
    
    // Clamp output points to reasonable range
    int points = getOutputPoints();
    points = std::max(2, std::min(points, 1000));
    output_points = points;
}

// ============================================================================
// Transform Implementation
// ============================================================================

Line2D extractLineSubsegment(
        Line2D const & line,
        LineSubsegmentParams const & params) {
    
    // Edge cases: empty or single-point lines
    if (line.empty()) {
        return line;
    }
    
    if (line.size() == 1) {
        return line;
    }
    
    float start_pos = params.getStartPosition();
    float end_pos = params.getEndPosition();
    
    // Clamp positions to valid range
    start_pos = std::max(0.0f, std::min(start_pos, 1.0f));
    end_pos = std::max(0.0f, std::min(end_pos, 1.0f));
    
    // Invalid range check
    if (start_pos >= end_pos) {
        return Line2D{};
    }
    
    if (params.getMethod() == LineSubsegmentMethod::Direct) {
        auto result = extract_line_subsegment_by_distance(
            line,
            start_pos,
            end_pos,
            params.getPreserveOriginalSpacing()
        );
        return Line2D(std::move(result));
    } else {
        // Parametric method
        auto result = extract_parametric_subsegment(
            line,
            start_pos,
            end_pos,
            params.getPolynomialOrder(),
            params.getOutputPoints()
        );
        return Line2D(std::move(result));
    }
}

Line2D extractLineSubsegmentWithContext(
        Line2D const & line,
        LineSubsegmentParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return line;  // Return unchanged on cancellation
    }

    auto result = extractLineSubsegment(line, params);
    ctx.reportProgress(100);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
