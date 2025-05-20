#include "order_line.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <cstddef>  // for size_t

std::vector<Point2D<float>> extract_line_pixels(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size) {

    auto const height = image_size.height;
    auto const width = image_size.width;
    
    // Extract coordinates of the line pixels
    std::vector<Point2D<float>> line_pixels;
    line_pixels.reserve(width * height / 10); // Reserve space to avoid reallocations
    
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (binary_img[row * width + col] == 1) {
                line_pixels.push_back({static_cast<float>(col), static_cast<float>(row)});
            }
        }
    }

    return line_pixels;
}

std::vector<Point2D<float>> order_line(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    auto line_pixels = extract_line_pixels(binary_img, image_size);

    return order_line(line_pixels, origin, subsample, tolerance);
}

std::vector<Point2D<float>> order_line(
        std::vector<Point2D<float>> & line_pixels,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    // If there are no line pixels, return empty vector
    if (line_pixels.empty()) {
        return {};
    }

    // Subsample the line pixels if subsample > 1
    if (subsample > 1) {
        std::vector<Point2D<float>> subsampled_pixels;
        subsampled_pixels.reserve(line_pixels.size() / subsample + 1);
        
        for (size_t i = 0; i < line_pixels.size(); i += subsample) {
            subsampled_pixels.push_back(line_pixels[i]);
        }
        line_pixels = std::move(subsampled_pixels);
    }

    const size_t num_points = line_pixels.size();
    
    // Find base point (closest to origin)
    float min_dist = std::numeric_limits<float>::max();
    size_t base_point_index = 0;
    
    for (size_t i = 0; i < num_points; ++i) {
        float dist = std::pow(line_pixels[i].x - origin.x, 2) + std::pow(line_pixels[i].y - origin.y, 2);
        if (dist < min_dist) {
            min_dist = dist;
            base_point_index = i;
        }
    }
    
    // Precompute all pairwise distances between points
    // This uses more memory but avoids repeated calculations
    std::vector<std::vector<float>> distance_matrix(num_points, std::vector<float>(num_points, 0.0f));
    
    for (size_t i = 0; i < num_points; ++i) {
        for (size_t j = i + 1; j < num_points; ++j) {
            // Calculate squared distance between points
            float dx = line_pixels[i].x - line_pixels[j].x;
            float dy = line_pixels[i].y - line_pixels[j].y;
            float dist = dx * dx + dy * dy;
            
            // Store distance in both directions (symmetric matrix)
            distance_matrix[i][j] = dist;
            distance_matrix[j][i] = dist;
        }
    }
    
    // Initialize the ordered list with the base point
    std::vector<Point2D<float>> ordered_pixels;
    ordered_pixels.reserve(num_points);
    ordered_pixels.push_back(line_pixels[base_point_index]);
    
    // Create a visited flag array
    std::vector<bool> visited(num_points, false);
    visited[base_point_index] = true;
    
    // Track minimum distances
    std::vector<float> min_distances;
    min_distances.reserve(num_points - 1);

    // Iteratively find the nearest neighbor
    size_t current_index = base_point_index;
    size_t points_remaining = num_points - 1; // Count of unvisited points
    
    while (points_remaining > 0) {
        float nearest_dist = std::numeric_limits<float>::max();
        size_t nearest_neighbor_index = 0;
        bool found_neighbor = false;
        
        // Find the nearest unvisited neighbor using precomputed distances
        for (size_t i = 0; i < num_points; ++i) {
            if (!visited[i]) {
                float dist = distance_matrix[current_index][i];
                
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_neighbor_index = i;
                    found_neighbor = true;
                }
            }
        }
        
        if (!found_neighbor) {
            break; // Should never happen if points_remaining > 0, but safety check
        }
        
        // Add the nearest point to the ordered list
        ordered_pixels.push_back(line_pixels[nearest_neighbor_index]);
        min_distances.push_back(nearest_dist);
        
        // Mark as visited and update current point
        visited[nearest_neighbor_index] = true;
        current_index = nearest_neighbor_index;
        points_remaining--;
    }

    // Apply tolerance and remove points with distances exceeding the tolerance
    /*
    if (tolerance > 0.0f) {
        std::vector<Point2D<float>> valid_points;
        valid_points.reserve(ordered_pixels.size());
        
        // Always include the first point
        valid_points.push_back(ordered_pixels[0]);
        
        // Filter subsequent points based on distance
        const float squared_tolerance = tolerance * tolerance;
        for (size_t i = 1; i < ordered_pixels.size(); ++i) {
            if (min_distances[i - 1] <= squared_tolerance) {
                valid_points.push_back(ordered_pixels[i]);
            }
        }
        
        return valid_points;
    }
    */

    return ordered_pixels;
}
