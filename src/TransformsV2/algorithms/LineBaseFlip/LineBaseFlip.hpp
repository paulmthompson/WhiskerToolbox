#ifndef WHISKERTOOLBOX_V2_LINE_BASE_FLIP_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_BASE_FLIP_TRANSFORM_HPP

#include "CoreGeometry/points.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for line base flip transform
 * 
 * This transform flips lines based on the distance of their endpoints
 * to a reference point. If the current base (first point) is farther
 * from the reference than the end (last point), the line is reversed.
 * 
 * Example JSON:
 * ```json
 * {
 *   "reference_x": 12.0,
 *   "reference_y": 0.0
 * }
 * ```
 */
struct LineBaseFlipParams {
    // X coordinate of the reference point
    std::optional<float> reference_x;
    
    // Y coordinate of the reference point
    std::optional<float> reference_y;

    // Helper methods with defaults
    float getReferenceX() const { return reference_x.value_or(0.0f); }
    float getReferenceY() const { return reference_y.value_or(0.0f); }
    
    /**
     * @brief Get reference point as Point2D
     */
    Point2D<float> getReferencePoint() const {
        return Point2D<float>{getReferenceX(), getReferenceY()};
    }
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns Line2D)
// ============================================================================

/**
 * @brief Flip a line's base if it's farther from the reference point than the end
 * 
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns a Line2D (possibly reversed).
 * 
 * The transform compares the distance from both endpoints (base = first point,
 * end = last point) to the reference point. If the base is farther from the
 * reference than the end, the line is reversed so the closer endpoint becomes
 * the new base.
 * 
 * Use cases:
 * - Ensuring consistent whisker orientation (base always near face)
 * - Normalizing line direction based on a landmark point
 * 
 * When applied to containers:
 * - LineData → LineData (one line per timestamp, possibly flipped)
 * 
 * Edge cases:
 * - Empty lines: Returned unchanged
 * - Single-point lines: Returned unchanged (cannot determine orientation)
 * - Equal distances: Line is NOT flipped (keeps original orientation)
 * 
 * @param line The line to potentially flip
 * @param params Parameters containing the reference point
 * @return Line2D The line, possibly with reversed point order
 */
Line2D flipLineBase(
        Line2D const & line,
        LineBaseFlipParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Line2D flipLineBaseWithContext(
        Line2D const & line,
        LineBaseFlipParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_BASE_FLIP_TRANSFORM_HPP
