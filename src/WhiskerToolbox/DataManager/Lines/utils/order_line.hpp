#ifndef UTILS_ORDER_LINE_HPP
#define UTILS_ORDER_LINE_HPP

#include "ImageSize/ImageSize.hpp"
#include "Points/points.hpp"

#include <cstdint>
#include <vector>
#include <utility> // for std::pair

// Find putative endpoints of a line based on distance
std::pair<size_t, size_t> find_line_endpoints(const std::vector<Point2D<float>>& points);

std::vector<Point2D<float>> order_line(
        std::vector<Point2D<uint32_t>> const & line_pixels,
        Point2D<float> const & origin,
        int subsample = 1,
        float tolerance = 5.0f);

std::vector<Point2D<float>> order_line(
        std::vector<Point2D<float>> & line_pixels,
        Point2D<float> const & origin,
        int subsample = 1,
        float tolerance = 5.0f);

std::vector<Point2D<float>> order_line(
        std::vector<uint8_t> const & binary_img,
        ImageSize image_size,
        Point2D<float> const & origin,
        int subsample = 1,
        float tolerance = 5.0f);

#endif// UTILS_ORDER_LINE_HPP
