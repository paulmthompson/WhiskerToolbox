#ifndef WHISKERTOOLBOX_V2_LINE_CLIP_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_CLIP_TRANSFORM_HPP

#include "CoreGeometry/line_geometry.hpp" // ClipSide

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for line clipping by reference line
 * 
 * This transform clips a line at its intersection with a reference line.
 * 
 * Example JSON:
 * ```json
 * {
 *   "clip_side": "KeepBase"
 * }
 * ```
 */
struct LineClipParams {
    /// Which side of the intersection to keep
    ClipSide clip_side = ClipSide::KeepBase;
};

// ============================================================================
// Transform Implementation (Binary - takes two inputs)
// ============================================================================

/**
 * @brief Clip a line at its intersection with a reference line
 * 
 * This is a **binary** element-level transform that takes a line and a reference line
 * as **separate inputs**, then returns the clipped line.
 * 
 * The V2 system supports this natively via BinaryElementTransform and tuple inputs.
 * Uses 1:1 matching - each Line2D is paired with one reference Line2D at the same time.
 * 
 * @param line The line to clip
 * @param reference_line The reference line to clip against
 * @param params Parameters controlling the clipping
 * @return Line2D The clipped line (or original if no intersection)
 */
Line2D clipLineAtReference(
        Line2D const & line,
        Line2D const & reference_line,
        LineClipParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Line2D clipLineAtReferenceWithContext(
        Line2D const & line,
        Line2D const & reference_line,
        LineClipParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_CLIP_TRANSFORM_HPP
