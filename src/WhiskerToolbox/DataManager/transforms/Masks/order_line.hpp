#ifndef UTILS_ORDER_LINE_HPP
#define UTILS_ORDER_LINE_HPP

#include "ImageSize/ImageSize.hpp"
#include "Points/points.hpp"

#include <cstdint>
#include <vector>

std::vector<Point2D<float>> order_line(
        std::vector<uint8_t> const & binary_img,
        ImageSize image_size,
        Point2D<float> const & origin,
        int subsample = 1,
        float tolerance = 5.0f);

#endif// UTILS_ORDER_LINE_HPP
