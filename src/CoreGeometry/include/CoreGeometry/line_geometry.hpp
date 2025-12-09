#ifndef LINE_GEOMETRY_HPP
#define LINE_GEOMETRY_HPP

#include "CoreGeometry/lines.hpp"

#include <optional>

/**
 * @brief Calculate the total calc_length of a line using Euclidean distance
 * @param line The line to calculate calc_length for
 * @return Total calc_length of the line, or 0.0 if line has fewer than 2 points
 */
float calc_length(Line2D const & line);

float calc_length2(Line2D const & line);


/**
 * @brief Calculate cumulative distances along a line
 * @param line The line to calculate distances for
 * @return Vector of cumulative distances from start of line to each point
 */
std::vector<float> calc_cumulative_length_vector(Line2D const & line);


/**
 * @brief Find the point at a specific distance along a line
 * @param line The line to search
 * @param target_distance The distance from start of line
 * @param use_interpolation Whether to interpolate between points or use nearest
 * @return The point at the specified distance, or nullopt if line is empty
 */
std::optional<Point2D<float>> point_at_distance(
        Line2D const & line,
        float target_distance,
        bool use_interpolation = true);


/**
 * @brief Find the point at a fractional position along a line (0.0 to 1.0)
 * @param line The line to search
 * @param position Fractional position (0.0 = start, 1.0 = end)
 * @param use_interpolation Whether to interpolate between points or use nearest
 * @return The point at the specified position, or nullopt if line is empty
 */
std::optional<Point2D<float>> point_at_fractional_position(
        Line2D const & line,
        float position,
        bool use_interpolation = true);

/**
 * @brief Extract a subsegment of a line between two fractional positions
 * @param line The line to extract from
 * @param start_position Start position as fraction (0.0 to 1.0)
 * @param end_position End position as fraction (0.0 to 1.0)
 * @param preserve_original_spacing Whether to keep original points or resample
 * @return Vector of points forming the subsegment
 */
std::vector<Point2D<float>> extract_line_subsegment_by_distance(
        Line2D const & line,
        float start_position,
        float end_position,
        bool preserve_original_spacing = true);

/**
 * @brief Calculate the position along a line at a given percentage of cumulative length
 *
 * @param line The line to calculate position on
 * @param percentage Percentage along the line (0.0 = start, 1.0 = end)
 * @return Point2D<float> The interpolated position along the line
 */
Point2D<float> get_position_at_percentage(Line2D const & line, float percentage);

/**
 * @brief Extract a segment of a line between two percentage points along its cumulative distance
 *
 * This function calculates the cumulative distance along the line and extracts
 * a continuous segment between the specified start and end percentages.
 *
 * @param line The input line (vector of Point2D)
 * @param start_percentage Start percentage (0.0 to 1.0, where 0.0 is beginning, 1.0 is end)
 * @param end_percentage End percentage (0.0 to 1.0, where 0.0 is beginning, 1.0 is end)
 * @return Line2D containing the extracted segment
 *
 * @note If start_percentage >= end_percentage, returns empty line
 * @note If line is empty or has only one point, returns empty line
 * @note Percentages are clamped to [0.0, 1.0] range
 */
Line2D get_segment_between_percentages(Line2D const & line, float start_percentage, float end_percentage);

/**
 * @brief Calculate the perpendicular direction at a specific vertex of a line
 *
 * For internal vertices (1 to n-1), calculates the average of perpendicular directions
 * from adjacent segments. For edge vertices (0 and n-1), calculates the perpendicular
 * of the single adjacent segment.
 *
 * @param line The line to calculate perpendicular direction for
 * @param vertex_index The index of the vertex (0 to line.size()-1)
 * @return Point2D<float> representing the normalized perpendicular direction vector
 *
 * @note Returns (0,0) if line has fewer than 2 points or vertex_index is invalid
 * @note For edge vertices, uses the single adjacent segment
 * @note For internal vertices, averages perpendiculars from both adjacent segments
 */
Point2D<float> calculate_perpendicular_direction(Line2D const & line, size_t vertex_index);

/**
 * @brief Calculate the squared distance from a point to a line segment
 * @param point The point to measure distance from
 * @param line_start Start point of the line segment
 * @param line_end End point of the line segment
 * @return Squared distance from the point to the nearest point on the segment
 */
float point_to_line_segment_distance2(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end);

/**
 * @brief Find the intersection point between two line segments
 * @param p1 First point of first segment
 * @param p2 Second point of first segment
 * @param p3 First point of second segment
 * @param p4 Second point of second segment
 * @return The intersection point if it exists, or std::nullopt if segments don't intersect
 */
std::optional<Point2D<float>> line_segment_intersection(
    Point2D<float> const & p1, Point2D<float> const & p2,
    Point2D<float> const & p3, Point2D<float> const & p4);

#endif// LINE_GEOMETRY_HPP
