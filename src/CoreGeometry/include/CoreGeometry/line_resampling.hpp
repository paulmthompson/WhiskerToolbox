#ifndef LINE_RESAMPLING_HPP
#define LINE_RESAMPLING_HPP

#include "CoreGeometry/lines.hpp"// For Line2D and Point2D

/**
 * @brief Resamples a line to a target point spacing.
 *
 * Iterates through the input polyline, placing new points at target_spacing
 * intervals, interpolating between original points as needed.
 * The first and last points of the original line are preserved.
 *
 * @param input_points The original ordered list of points representing the line.
 * @param target_spacing The desired approximate spacing between points in the output line.
 * @return A new Line2D containing the resampled points.
 */
Line2D resample_line_points(
        Line2D const & input_points,
        float target_spacing);

/**
 * @brief Simplifies a line using the Douglas-Peucker algorithm.
 *
 * The Douglas-Peucker algorithm recursively simplifies a polyline by removing
 * points that are within epsilon distance of the line segment between two endpoints.
 * This algorithm preserves the overall shape of the line while reducing the number
 * of points.
 *
 * @param input_points The original ordered list of points representing the line.
 * @param epsilon The maximum perpendicular distance tolerance for point removal.
 * @return A new Line2D containing the simplified points.
 */
Line2D douglas_peucker_simplify(
        Line2D const & input_points,
        float epsilon);

#endif// LINE_RESAMPLING_HPP
