#include "LineLength.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Transform Implementation
// ============================================================================

float calculateLineLength(
        Line2D const & line,
        LineLengthParams const & /*params*/) {
    return calc_length(line);
}

float calculateLineLengthWithContext(
        Line2D const & line,
        LineLengthParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return 0.0f;
    }

    auto result = calculateLineLength(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

} // namespace WhiskerToolbox::Transforms::V2::Examples
