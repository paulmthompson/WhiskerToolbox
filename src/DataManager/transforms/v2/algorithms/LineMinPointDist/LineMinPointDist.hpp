#ifndef WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP

#include "transforms/v2/extension/ElementTransform.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

class Line2D;
template<typename T>
struct Point2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for line-to-point distance calculation
 * 
 * This transform computes the minimum distance from a set of points to a line.
 * 
 * Example JSON:
 * ```json
 * {
 *   "use_first_line_only": true,
 *   "return_squared_distance": false
 * }
 * ```
 */
struct LineMinPointDistParams {
    // Whether to use only the first line (true) or all lines (false)
    std::optional<bool> use_first_line_only;

    // Whether to return squared distance (faster, no sqrt)
    std::optional<bool> return_squared_distance;

    // Helper methods with defaults
    bool getUseFirstLineOnly() const {
        return use_first_line_only.value_or(true);
    }

    bool getReturnSquaredDistance() const {
        return return_squared_distance.value_or(false);
    }
};

// ============================================================================
// Helper Functions (declared here, defined in .cpp)
// ============================================================================

/**
 * @brief Calculate squared distance from point to line segment
 */
float pointToLineSegmentDistance2(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end);

/**
 * @brief Calculate minimum squared distance from point to entire line
 */
float pointToLineMinDistance2(Point2D<float> const & point, Line2D const & line);

// ============================================================================
// Transform Implementation (Binary - takes two inputs)
// ============================================================================

/**
 * @brief Calculate distance from a single point to a line
 * 
 * This is a **binary** element-level transform that takes a line and a single point
 * as **separate inputs**, then returns the distance from the point to the line.
 * 
 * The V2 system supports this natively via BinaryElementTransform and tuple inputs.
 * Uses 1:1 matching - each Line2D is paired with one Point2D at the same index.
 * 
 * @param line The line to measure distance from
 * @param point The point to measure distance to
 * @param params Parameters controlling the calculation
 * @return float Distance (or squared distance if configured)
 */
float calculateLineMinPointDistance(
        Line2D const & line,
        Point2D<float> const & point,
        LineMinPointDistParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
float calculateLineMinPointDistanceWithContext(
        Line2D const & line,
        Point2D<float> const & point,
        LineMinPointDistParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP
