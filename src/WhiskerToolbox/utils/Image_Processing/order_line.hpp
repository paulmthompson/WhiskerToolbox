#ifndef ORDER_LINE_HPP
#define ORDER_LINE_HPP

#include "DataManager/Points/points.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

std::vector<Point2D<float>> order_line(
    const std::vector<uint8_t>& binary_img,
    int height,
    int width,
    const Point2D<float>& origin,
    int subsample = 1,
    float tolerance = 5.0f) {

    // Extract coordinates of the line pixels
    std::vector<Point2D<float>> line_pixels;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (binary_img[row * width + col] == 1) {
                line_pixels.push_back({static_cast<float>(col), static_cast<float>(row)});
            }
        }
    }

    // If there are no line pixels, return empty vector
    if (line_pixels.empty()) {
        return {};
    }

    // Subsample the line pixels if subsample > 1
    if (subsample > 1) {
        std::vector<Point2D<float>> subsampled_pixels;
        for (size_t i = 0; i < line_pixels.size(); i += subsample) {
            subsampled_pixels.push_back(line_pixels[i]);
        }
        line_pixels = std::move(subsampled_pixels);
    }

    // Calculate distances from each pixel to the origin
    std::vector<float> distances_to_origin(line_pixels.size());
    std::transform(line_pixels.begin(), line_pixels.end(), distances_to_origin.begin(), [&origin](const Point2D<float>& p) {
        return std::pow(p.x - origin.x, 2) + std::pow(p.y - origin.y, 2);
    });

    // Find the base point (closest to the origin)
    auto base_point_iter = std::min_element(distances_to_origin.begin(), distances_to_origin.end());
    size_t base_point_index = std::distance(distances_to_origin.begin(), base_point_iter);
    Point2D base_point = line_pixels[base_point_index];

    // Initialize the ordered list with the base point
    std::vector<Point2D<float>> ordered_pixels = {base_point};
    line_pixels.erase(line_pixels.begin() + base_point_index);

    // Track minimum distances
    std::vector<float> min_distances;

    // Iteratively find the nearest neighbor
    Point2D<float> current_point = base_point;
    while (!line_pixels.empty()) {
        std::vector<float> distances_to_current(line_pixels.size());
        std::transform(line_pixels.begin(), line_pixels.end(), distances_to_current.begin(), [&current_point](const Point2D<float>& p) {
            return std::pow(p.x - current_point.x, 2) + std::pow(p.y - current_point.y, 2);
        });

        auto nearest_neighbor_iter = std::min_element(distances_to_current.begin(), distances_to_current.end());
        size_t nearest_neighbor_index = std::distance(distances_to_current.begin(), nearest_neighbor_iter);
        Point2D nearest_neighbor = line_pixels[nearest_neighbor_index];
        min_distances.push_back(distances_to_current[nearest_neighbor_index]);
        ordered_pixels.push_back(nearest_neighbor);
        line_pixels.erase(line_pixels.begin() + nearest_neighbor_index);
        current_point = nearest_neighbor;
    }

    // Apply tolerance and remove points with no nearest neighbors within the tolerance
    std::vector<Point2D<float>> valid_points = {ordered_pixels[0]};
    for (size_t i = 1; i < ordered_pixels.size(); ++i) {
        if (min_distances[i - 1] <= tolerance) {
            valid_points.push_back(ordered_pixels[i]);
        }
    }

    return valid_points;
}

#endif // ORDER_LINE_HPP
