#ifndef LINE_RESAMPLING_HPP
#define LINE_RESAMPLING_HPP

#include "Lines/lines.hpp" // For Line2D and Point2D

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
    const Line2D& input_points,
    float target_spacing);

#endif // LINE_RESAMPLING_HPP 
