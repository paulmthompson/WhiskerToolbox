#ifndef NEURALYZER_V2_LINE_EXTEND_TRANSFORM_HPP
#define NEURALYZER_V2_LINE_EXTEND_TRANSFORM_HPP

#include "CoreGeometry/line_geometry.hpp"// ExtendEndpoint

class Line2D;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for extending a line to a reference line
 *
 * Example JSON:
 * ```json
 * {
 *   "extend_end": "Distal",
 *   "tangent_distance_pixels": 20.0
 * }
 * ```
 */
struct LineExtendParams {
    /// Endpoint from which to extend the line
    ExtendEndpoint extend_end = ExtendEndpoint::Distal;
    /// Arc length in pixels from the endpoint into the line used to define the extension direction
    float tangent_distance_pixels = 20.0f;
};

/**
 * @brief Extend a line along a smoothed tangent until it hits a reference polyline
 *
 * @param line The line to extend
 * @param reference_line Reference polyline to intersect
 * @param params Extension parameters
 * @return Extended line, or the original line if no valid intersection is found
 */
Line2D extendLineAtReference(
        Line2D const & line,
        Line2D const & reference_line,
        LineExtendParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Line2D extendLineAtReferenceWithContext(
        Line2D const & line,
        Line2D const & reference_line,
        LineExtendParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_LINE_EXTEND_TRANSFORM_HPP
