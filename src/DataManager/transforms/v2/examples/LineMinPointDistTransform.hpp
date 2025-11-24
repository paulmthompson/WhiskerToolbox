#ifndef WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "transforms/v2/core/ElementTransform.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
// Helper Functions
// ============================================================================

/**
 * @brief Calculate squared distance from point to line segment
 * 
 * Returns the squared Euclidean distance from a point to the closest point
 * on a line segment.
 */
inline float pointToLineSegmentDistance2(
    Point2D<float> const& point,
    Point2D<float> const& line_start,
    Point2D<float> const& line_end)
{
    // If start and end are the same point, return distance to that point
    if (line_start.x == line_end.x && line_start.y == line_end.y) {
        float dx = point.x - line_start.x;
        float dy = point.y - line_start.y;
        return dx * dx + dy * dy;
    }

    // Calculate the squared length of the line segment
    float line_length_squared = 
        (line_end.x - line_start.x) * (line_end.x - line_start.x) +
        (line_end.y - line_start.y) * (line_end.y - line_start.y);

    // Calculate the projection of point onto the line segment
    float t = ((point.x - line_start.x) * (line_end.x - line_start.x) +
               (point.y - line_start.y) * (line_end.y - line_start.y)) /
              line_length_squared;

    // Clamp t to range [0, 1] to ensure we get distance to a point on the segment
    t = std::max(0.0f, std::min(1.0f, t));

    // Calculate the closest point on the segment
    float closest_x = line_start.x + t * (line_end.x - line_start.x);
    float closest_y = line_start.y + t * (line_end.y - line_start.y);

    // Calculate the distance from original point to closest point
    float dx = point.x - closest_x;
    float dy = point.y - closest_y;

    return dx * dx + dy * dy;
}

/**
 * @brief Calculate minimum squared distance from point to entire line
 */
inline float pointToLineMinDistance2(Point2D<float> const& point, Line2D const& line) {
    if (line.size() < 2) {
        // Invalid line - return max distance
        return std::numeric_limits<float>::max();
    }

    float min_distance = std::numeric_limits<float>::max();

    // Check each segment of the line
    for (size_t i = 0; i < line.size() - 1; ++i) {
        Point2D<float> const& segment_start = line[i];
        Point2D<float> const& segment_end = line[i + 1];

        float distance = pointToLineSegmentDistance2(point, segment_start, segment_end);
        min_distance = std::min(min_distance, distance);
    }

    return min_distance;
}

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
inline float calculateLineMinPointDistance(
    Line2D const& line,
    Point2D<float> const& point,
    LineMinPointDistParams const& params)
{
    if (line.size() < 2) {
        // Invalid line - return infinity
        return std::numeric_limits<float>::infinity();
    }

    float distance_squared = pointToLineMinDistance2(point, line);

    // Return squared or actual distance based on parameters
    if (params.getReturnSquaredDistance()) {
        return distance_squared;
    } else {
        return std::sqrt(distance_squared);
    }
}

/**
 * @brief Context-aware version with progress reporting
 */
inline float calculateLineMinPointDistanceWithContext(
    Line2D const& line,
    Point2D<float> const& point,
    LineMinPointDistParams const& params,
    ComputeContext const& ctx)
{
    // Check for cancellation
    if (ctx.shouldCancel()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    
    auto result = calculateLineMinPointDistance(line, point, params);
    
    ctx.reportProgress(1.0f);

    
    return result;
}

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_LINE_MIN_POINT_DIST_TRANSFORM_HPP
