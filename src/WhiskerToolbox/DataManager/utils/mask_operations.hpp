#ifndef MASK_OPERATIONS_HPP
#define MASK_OPERATIONS_HPP

#include "Points/points.hpp"

#include <cstdint>
#include <vector>

std::vector<Point2D<float>> convert_mask_to_line(
        std::vector<uint8_t> mask,
        Point2D<float> base_point,
        uint8_t mask_threshold = 128);

#endif// MASK_OPERATIONS_HPP
