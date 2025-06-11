#include "masks.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

Mask2D create_mask(std::vector<float> const & x, std::vector<float> const & y) {
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());// Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++) {
        //new_mask.emplace_back(Point2D<float>{x[i], y[i]});// Use emplace_back for efficiency
        new_mask.push_back({x[i], y[i]});
    }

    return new_mask;
}


Mask2D create_mask(std::vector<float> && x, std::vector<float> && y) {
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());// Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++) {
        //new_mask.emplace_back(Point2D<float>{x[i], y[i]});// Access elements directly
        new_mask.push_back({x[i], y[i]});
    }

    return new_mask;
}

std::pair<Point2D<float>, Point2D<float>> get_bounding_box(Mask2D const & mask) {
    float min_x = mask[0].x;
    float max_x = mask[0].x;
    float min_y = mask[0].y;
    float max_y = mask[0].y;

    for (auto const & point: mask) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    return {Point2D<float>{min_x, min_y}, Point2D<float>{max_x, max_y}};
}

std::vector<Point2D<float>> get_mask_outline(Mask2D const & mask) {
    if (mask.empty() || mask.size() < 2) {
        return {};
    }

    // Create maps to store extremal points
    std::map<float, float> max_y_for_x;// x -> max_y
    std::map<float, float> min_y_for_x;// x -> min_y
    std::map<float, float> max_x_for_y;// y -> max_x
    std::map<float, float> min_x_for_y;// y -> min_x

    // Find extremal points
    for (auto const & point: mask) {
        // Update max y for this x
        if (max_y_for_x.find(point.x) == max_y_for_x.end() || point.y > max_y_for_x[point.x]) {
            max_y_for_x[point.x] = point.y;
        }

        // Update min y for this x
        if (min_y_for_x.find(point.x) == min_y_for_x.end() || point.y < min_y_for_x[point.x]) {
            min_y_for_x[point.x] = point.y;
        }

        // Update max x for this y
        if (max_x_for_y.find(point.y) == max_x_for_y.end() || point.x > max_x_for_y[point.y]) {
            max_x_for_y[point.y] = point.x;
        }

        // Update min x for this y
        if (min_x_for_y.find(point.y) == min_x_for_y.end() || point.x < min_x_for_y[point.y]) {
            min_x_for_y[point.y] = point.x;
        }
    }

    // Collect all extremal points
    std::set<std::pair<float, float>> extremal_points_set;

    // Add max_y points for each x
    for (auto const & [x, max_y]: max_y_for_x) {
        extremal_points_set.insert({x, max_y});
    }

    // Add min_y points for each x
    for (auto const & [x, min_y]: min_y_for_x) {
        extremal_points_set.insert({x, min_y});
    }

    // Add max_x points for each y
    for (auto const & [y, max_x]: max_x_for_y) {
        extremal_points_set.insert({max_x, y});
    }

    // Add min_x points for each y
    for (auto const & [y, min_x]: min_x_for_y) {
        extremal_points_set.insert({min_x, y});
    }

    // Convert to vector and sort by angle from centroid for proper ordering
    std::vector<Point2D<float>> extremal_points;
    extremal_points.reserve(extremal_points_set.size());

    for (auto const & [x, y]: extremal_points_set) {
        extremal_points.push_back({x, y});
    }

    if (extremal_points.size() < 3) {
        return extremal_points;// Not enough points for proper outline
    }

    // Calculate centroid
    float centroid_x = 0.0f, centroid_y = 0.0f;
    for (auto const & point: extremal_points) {
        centroid_x += point.x;
        centroid_y += point.y;
    }
    centroid_x /= static_cast<float>(extremal_points.size());
    centroid_y /= static_cast<float>(extremal_points.size());

    // Sort points by angle from centroid (for proper connection order)
    std::sort(extremal_points.begin(), extremal_points.end(),
              [centroid_x, centroid_y](Point2D<float> const & a, Point2D<float> const & b) {
                  float const angle_a = std::atan2(a.y - centroid_y, a.x - centroid_x);
                  float const angle_b = std::atan2(b.y - centroid_y, b.x - centroid_x);
                  return angle_a < angle_b;
              });

    return extremal_points;
}

std::vector<Point2D<float>> extract_line_pixels(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size) {

    auto const height = image_size.height;
    auto const width = image_size.width;

    // Extract coordinates of the line pixels
    std::vector<Point2D<float>> line_pixels;
    line_pixels.reserve(width * height / 10);// Reserve space to avoid reallocations

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (binary_img[row * width + col] > 0) {
                line_pixels.push_back({static_cast<float>(col), static_cast<float>(row)});
            }
        }
    }

    return line_pixels;
}
