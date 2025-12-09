#include "LineResample.hpp"

#include "CoreGeometry/line_resampling.hpp"
#include "CoreGeometry/lines.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Transform Implementation
// ============================================================================

Line2D resampleLine(
        Line2D const & line,
        LineResampleParams const & params) {
    
    // Edge cases: empty, single point, or two-point lines
    if (line.size() <= 2) {
        return line;
    }

    switch (params.getMethod()) {
        case LineResampleMethod::FixedSpacing:
            return resample_line_points(line, params.getTargetSpacing());
        case LineResampleMethod::DouglasPeucker:
            return douglas_peucker_simplify(line, params.getEpsilon());
        default:
            return line;
    }
}

Line2D resampleLineWithContext(
        Line2D const & line,
        LineResampleParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return line;  // Return unchanged on cancellation
    }

    auto result = resampleLine(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
