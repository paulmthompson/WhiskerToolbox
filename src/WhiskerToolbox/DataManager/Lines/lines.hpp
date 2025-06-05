#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <cstdint>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


Line2D create_line(std::vector<float> const & x, std::vector<float> const & y);


void smooth_line(Line2D & line);

std::vector<uint8_t> line_to_image(Line2D & line, int height, int width);



#endif// DATAMANAGER_LINES_HPP
