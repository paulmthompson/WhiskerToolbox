#ifndef WHISKERTOOLBOX_V2_LINE_LENGTH_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_LENGTH_TRANSFORM_HPP

/**
 * @file LineLength.hpp
 * @brief Element transform computing total arc length of a Line2D
 *
 * Phase 1.4 — TableView → TensorData Refactoring
 *
 * Replaces the old LineLengthComputer in the TableView system.
 * Uses CoreGeometry's calc_length() internally.
 *
 * Example JSON:
 * ```json
 * {
 *   "transform_name": "CalculateLineLength",
 *   "parameters": {}
 * }
 * ```
 *
 * @see LineAngle.hpp for a similar Line2D → float transform
 * @see CoreGeometry/line_geometry.hpp for calc_length()
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for line length calculation
 *
 * Currently parameterless — the full arc length is always returned.
 * The struct exists for consistency with the ElementTransform pattern
 * and to allow future extensions (e.g. partial length, length units).
 */
struct LineLengthParams {
    /**
     * @brief Normalize and validate parameters (no-op for now)
     */
    void validate() {}
};

// ============================================================================
// Transform Implementation (Unary — Line2D → float)
// ============================================================================

/**
 * @brief Calculate the total arc length of a Line2D
 *
 * Computes the sum of Euclidean distances between consecutive points.
 * Returns 0 for lines with fewer than 2 points.
 *
 * When applied to containers:
 *   - LineData → AnalogTimeSeries (one length per timestamp per line)
 *
 * @param line   The line to measure
 * @param params Parameters (currently unused)
 * @return Total arc length as a float
 */
float calculateLineLength(
        Line2D const & line,
        LineLengthParams const & params);

/**
 * @brief Context-aware version with progress reporting and cancellation
 */
float calculateLineLengthWithContext(
        Line2D const & line,
        LineLengthParams const & params,
        ComputeContext const & ctx);

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_LINE_LENGTH_TRANSFORM_HPP
