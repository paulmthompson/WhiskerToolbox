#ifndef LINE_GEOMETRY_HPP
#define LINE_GEOMETRY_HPP

#include "Lines/lines.hpp"

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
std::vector<float> calc_cumulative_length(Line2D const & line);


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

#endif// LINE_GEOMETRY_HPP
