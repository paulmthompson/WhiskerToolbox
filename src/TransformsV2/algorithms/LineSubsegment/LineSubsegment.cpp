#include "LineSubsegment.hpp"

#include "core/ComputeContext.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreMath/polynomial_fit.hpp"

#include <algorithm>
#include <cmath>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// LineSubsegmentParams::validate() Implementation
// ============================================================================

void LineSubsegmentParams::validate() {
    start_position = std::max(0.0f, std::min(start_position, 1.0f));
    end_position = std::max(0.0f, std::min(end_position, 1.0f));
    polynomial_order = std::max(1, std::min(polynomial_order, 9));
    output_points = std::max(2, std::min(output_points, 1000));
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
    
    float start_pos = std::max(0.0f, std::min(params.start_position, 1.0f));
    float end_pos = std::max(0.0f, std::min(params.end_position, 1.0f));

    if (start_pos >= end_pos) {
        return Line2D{};
    }

    if (params.method == LineSubsegmentMethod::Direct) {
        auto result = extract_line_subsegment_by_distance(
                line,
                start_pos,
                end_pos,
                params.preserve_original_spacing);
        return Line2D(std::move(result));
    }

    auto result = extract_parametric_subsegment(
            line,
            start_pos,
            end_pos,
            params.polynomial_order,
            params.output_points);
    return Line2D(std::move(result));
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
