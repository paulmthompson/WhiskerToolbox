#include "LineBaseFlip.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

Line2D flipLineBase(
        Line2D const & line,
        LineBaseFlipParams const & params) {
    
    if (line.empty() || line.size() < 2) {
        // Return unchanged for empty or single-point lines
        return line;
    }

    Point2D<float> const reference_point = params.getReferencePoint();

    if (is_distal_end_closer(line, reference_point)) {
        return reverse_line(line);
    }

    return line;
}

Line2D flipLineBaseWithContext(
        Line2D const & line,
        LineBaseFlipParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return line;
    }

    auto result = flipLineBase(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
