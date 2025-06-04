#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <cstdint>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


Line2D create_line(std::vector<float> const & x, std::vector<float> const & y);


void smooth_line(Line2D & line);

std::vector<uint8_t> line_to_image(Line2D & line, int height, int width);

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

#endif// DATAMANAGER_LINES_HPP
