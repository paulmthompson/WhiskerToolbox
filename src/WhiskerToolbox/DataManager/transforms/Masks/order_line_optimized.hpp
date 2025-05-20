#ifndef UTILS_ORDER_LINE_OPTIMIZED_HPP
#define UTILS_ORDER_LINE_OPTIMIZED_HPP

#include "ImageSize/ImageSize.hpp"
#include "Points/points.hpp"

#include <cstdint>
#include <vector>

// Optimized version of order_line that uses a k-d tree for nearest neighbor search
std::vector<Point2D<float>> order_line_optimized(
        std::vector<uint8_t> const & binary_img,
        ImageSize image_size,
        Point2D<float> const & origin,
        int subsample = 1,
        float tolerance = 5.0f);

#endif// UTILS_ORDER_LINE_OPTIMIZED_HPP 