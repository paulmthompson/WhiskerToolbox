#include "CoreGeometry/masks.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

Mask2D create_mask(std::vector<uint32_t> const & x, std::vector<uint32_t> const & y) {
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());// Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++) {
        new_mask.push_back({x[i], y[i]});
    }

    return new_mask;
}

Mask2D create_mask(std::vector<float> const & x, std::vector<float> const & y) {
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());// Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++) {
        // Round coordinates to nearest integers and ensure non-negative
        uint32_t rounded_x = static_cast<uint32_t>(std::max(0.0f, std::round(x[i])));
        uint32_t rounded_y = static_cast<uint32_t>(std::max(0.0f, std::round(y[i])));
        new_mask.push_back({rounded_x, rounded_y});
    }

    return new_mask;
}

std::pair<Point2D<uint32_t>, Point2D<uint32_t>> get_bounding_box(Mask2D const & mask) {
    uint32_t min_x = mask[0].x;
    uint32_t max_x = mask[0].x;
    uint32_t min_y = mask[0].y;
    uint32_t max_y = mask[0].y;

    for (auto const & point: mask) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    return {Point2D<uint32_t>{min_x, min_y}, Point2D<uint32_t>{max_x, max_y}};
}

std::vector<Point2D<uint32_t>> get_mask_outline(Mask2D const & mask) {
    if (mask.empty() || mask.size() < 2) {
        return {};
    }

    // Create maps to store extremal points
    std::map<uint32_t, uint32_t> max_y_for_x;// x -> max_y
    std::map<uint32_t, uint32_t> min_y_for_x;// x -> min_y
    std::map<uint32_t, uint32_t> max_x_for_y;// y -> max_x
    std::map<uint32_t, uint32_t> min_x_for_y;// y -> min_x

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
    std::set<std::pair<uint32_t, uint32_t>> extremal_points_set;

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
    std::vector<Point2D<uint32_t>> extremal_points;
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
        centroid_x += static_cast<float>(point.x);
        centroid_y += static_cast<float>(point.y);
    }
    centroid_x /= static_cast<float>(extremal_points.size());
    centroid_y /= static_cast<float>(extremal_points.size());

    // Sort points by angle from centroid (for proper connection order)
    std::sort(extremal_points.begin(), extremal_points.end(),
              [centroid_x, centroid_y](Point2D<uint32_t> const & a, Point2D<uint32_t> const & b) {
                  float const angle_a = std::atan2(static_cast<float>(a.y) - centroid_y, static_cast<float>(a.x) - centroid_x);
                  float const angle_b = std::atan2(static_cast<float>(b.y) - centroid_y, static_cast<float>(b.x) - centroid_x);
                  return angle_a < angle_b;
              });

    return extremal_points;
}

std::vector<Point2D<uint32_t>> generate_ellipse_pixels(float center_x, float center_y, float radius_x, float radius_y) {
    std::vector<Point2D<uint32_t>> ellipse_pixels;

    // Round center coordinates for calculation
    int rounded_center_x = static_cast<int>(std::round(center_x));
    int rounded_center_y = static_cast<int>(std::round(center_y));

    // Generate all pixels within the elliptical region (circle when radius_x == radius_y)
    int max_radius = static_cast<int>(std::max(radius_x, radius_y)) + 1;
    for (int dx = -max_radius; dx <= max_radius; ++dx) {
        for (int dy = -max_radius; dy <= max_radius; ++dy) {
            // Check if point is within elliptical radius using ellipse equation
            // (dx/radius_x)^2 + (dy/radius_y)^2 <= 1
            float normalized_dx = static_cast<float>(dx) / radius_x;
            float normalized_dy = static_cast<float>(dy) / radius_y;
            float ellipse_distance = normalized_dx * normalized_dx + normalized_dy * normalized_dy;

            if (ellipse_distance <= 1.0f) {
                int x = rounded_center_x + dx;
                int y = rounded_center_y + dy;

                // Only add pixels that are within valid bounds (non-negative)
                if (x >= 0 && y >= 0) {
                    ellipse_pixels.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                }
            }
        }
    }

    return ellipse_pixels;
}

Mask2D combine_masks(Mask2D const & mask1, Mask2D const & mask2) {
    // Use a set to efficiently track unique pixel coordinates
    std::set<std::pair<uint32_t, uint32_t>> unique_pixels;
    Mask2D combined_mask;

    // Add all pixels from mask1
    for (auto const & point: mask1) {
        std::pair<uint32_t, uint32_t> pixel_key = {point.x, point.y};

        if (unique_pixels.find(pixel_key) == unique_pixels.end()) {
            unique_pixels.insert(pixel_key);
            combined_mask.push_back(point);
        }
    }

    // Add pixels from mask2 that aren't already present
    for (auto const & point: mask2) {
        std::pair<uint32_t, uint32_t> pixel_key = {point.x, point.y};

        if (unique_pixels.find(pixel_key) == unique_pixels.end()) {
            unique_pixels.insert(pixel_key);
            combined_mask.push_back(point);
        }
    }

    return combined_mask;
}

Mask2D subtract_masks(Mask2D const & mask1, Mask2D const & mask2) {
    // Create a set of pixels in mask2 for efficient lookup
    std::set<std::pair<uint32_t, uint32_t>> mask2_pixels;
    for (auto const & point: mask2) {
        mask2_pixels.insert({point.x, point.y});
    }

    // Keep only pixels from mask1 that are NOT in mask2
    Mask2D result_mask;
    for (auto const & point: mask1) {
        std::pair<uint32_t, uint32_t> pixel_key = {point.x, point.y};

        if (mask2_pixels.find(pixel_key) == mask2_pixels.end()) {
            result_mask.push_back(point);
        }
    }

    return result_mask;
}

Mask2D generate_outline_mask(Mask2D const & mask, int thickness, uint32_t image_width, uint32_t image_height) {
    if (mask.empty() || thickness <= 0) {
        return {};
    }

    // Create a set for fast lookup of mask pixels
    std::set<std::pair<uint32_t, uint32_t>> mask_pixels;
    for (auto const & point: mask) {
        mask_pixels.insert({point.x, point.y});
    }

    // Find outline pixels
    Mask2D outline_mask;
    std::set<std::pair<uint32_t, uint32_t>> outline_pixels;

    // For each pixel in the original mask, check if it's on the edge
    for (auto const & point: mask) {
        bool is_edge = false;

        // Check all 8 neighbors (including diagonals)
        for (int dx = -thickness; dx <= thickness; ++dx) {
            for (int dy = -thickness; dy <= thickness; ++dy) {
                if (dx == 0 && dy == 0) continue;// Skip the center pixel itself

                int32_t neighbor_x = static_cast<int32_t>(point.x) + dx;
                int32_t neighbor_y = static_cast<int32_t>(point.y) + dy;

                // Check bounds
                if (neighbor_x < 0 || neighbor_y < 0) {
                    is_edge = true;
                    break;
                }

                if (image_width != UINT32_MAX && static_cast<uint32_t>(neighbor_x) >= image_width) {
                    is_edge = true;
                    break;
                }

                if (image_height != UINT32_MAX && static_cast<uint32_t>(neighbor_y) >= image_height) {
                    is_edge = true;
                    break;
                }

                // Check if neighbor is not in the mask
                if (mask_pixels.find({static_cast<uint32_t>(neighbor_x), static_cast<uint32_t>(neighbor_y)}) == mask_pixels.end()) {
                    is_edge = true;
                    break;
                }
            }
            if (is_edge) break;
        }

        // If this pixel is on the edge, add it to outline
        if (is_edge) {
            outline_pixels.insert({point.x, point.y});
        }
    }

    // Convert set to vector
    outline_mask.reserve(outline_pixels.size());
    for (auto const & pixel: outline_pixels) {
        outline_mask.push_back({pixel.first, pixel.second});
    }

    return outline_mask;
}

std::vector<Point2D<uint32_t>> extract_line_pixels(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size) {

    auto const height = image_size.height;
    auto const width = image_size.width;

    // Extract coordinates of the line pixels
    std::vector<Point2D<uint32_t>> line_pixels;
    line_pixels.reserve(width * height / 10);// Reserve space to avoid reallocations

    for (size_t row = 0; row < static_cast<size_t>(height); ++row) {
        for (size_t col = 0; col < static_cast<size_t>(width); ++col) {
            if (binary_img[row * static_cast<size_t>(width) + col] > 0) {
                line_pixels.push_back({static_cast<uint32_t>(col), static_cast<uint32_t>(row)});
            }
        }
    }

    return line_pixels;
}
