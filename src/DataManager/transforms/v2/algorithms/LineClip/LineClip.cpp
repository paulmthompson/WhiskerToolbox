#include "LineClip.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Transform Implementation (Binary - takes two inputs)
// ============================================================================

/**
 * @brief Clip a line at its intersection with a reference line
 */
Line2D clipLineAtReference(
        Line2D const & line,
        Line2D const & reference_line,
        LineClipParams const & params) {
    
    // Use the existing CoreGeometry function for clipping
    return clip_line_at_intersection(line, reference_line, params.getClipSide());
}

/**
 * @brief Context-aware version with progress reporting
 */
Line2D clipLineAtReferenceWithContext(
        Line2D const & line,
        Line2D const & reference_line,
        LineClipParams const & params,
        [[maybe_unused]] ComputeContext const & ctx) {
    
    // For a single element transform, there's not much to report
    // Just delegate to the non-context version
    return clipLineAtReference(line, reference_line, params);
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
