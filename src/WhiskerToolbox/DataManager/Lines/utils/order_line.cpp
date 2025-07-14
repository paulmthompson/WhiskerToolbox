#include "order_line.hpp"

#include "CoreGeometry/masks.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <cstddef>  // for size_t

// Find putative endpoints of the line
std::pair<size_t, size_t> find_line_endpoints(const std::vector<Point2D<float>>& points) {
    if (points.size() <= 1) {
        return {0, 0}; // Not enough points
    }
    
    // Step 1: Pick an arbitrary starting point (e.g., the first point)
    size_t start_idx = 0;
    
    // Step 2: Find the point farthest from the starting point
    size_t first_endpoint_idx = 0;
    float max_dist = -1.0f;
    
    for (size_t i = 0; i < points.size(); ++i) {
        float dx = points[i].x - points[start_idx].x;
        float dy = points[i].y - points[start_idx].y;
        float dist = dx * dx + dy * dy;
        
        if (dist > max_dist) {
            max_dist = dist;
            first_endpoint_idx = i;
        }
    }
    
    // Step 3: Find the point farthest from the first endpoint
    size_t second_endpoint_idx = 0;
    max_dist = -1.0f;
    
    for (size_t i = 0; i < points.size(); ++i) {
        float dx = points[i].x - points[first_endpoint_idx].x;
        float dy = points[i].y - points[first_endpoint_idx].y;
        float dist = dx * dx + dy * dy;
        
        if (dist > max_dist) {
            max_dist = dist;
            second_endpoint_idx = i;
        }
    }
    
    return {first_endpoint_idx, second_endpoint_idx};
}

Line2D order_line(
        std::vector<Point2D<uint32_t>> const & line_pixels,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    std::vector<Point2D<float>> line_pixels_float;
    line_pixels_float.reserve(line_pixels.size());
    for (auto const & point : line_pixels) {
        line_pixels_float.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
    }

    return order_line(line_pixels_float, origin, subsample, tolerance);
}

Line2D order_line(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    auto line_pixels = extract_line_pixels(binary_img, image_size);

    return order_line(line_pixels, origin, subsample, tolerance);
}

Line2D order_line(
        std::vector<Point2D<float>> & line_pixels,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    static_cast<void>(tolerance);

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

    // Find the endpoints of the line
    auto [first_endpoint_idx, second_endpoint_idx] = find_line_endpoints(line_pixels);
    
    // Calculate the distance from the origin to both endpoints to determine orientation
    float dist_origin_to_first = std::pow(line_pixels[first_endpoint_idx].x - origin.x, 2) + 
                                std::pow(line_pixels[first_endpoint_idx].y - origin.y, 2);
    float dist_origin_to_second = std::pow(line_pixels[second_endpoint_idx].x - origin.x, 2) + 
                                 std::pow(line_pixels[second_endpoint_idx].y - origin.y, 2);
    
    // Start ordering from the endpoint farthest from the origin
    size_t start_point_idx = (dist_origin_to_first > dist_origin_to_second) ? 
                            first_endpoint_idx : second_endpoint_idx;
    
    const size_t num_points = line_pixels.size();
    
    // Precompute all pairwise distances between points
    std::vector<std::vector<float>> distance_matrix(num_points, std::vector<float>(num_points, 0.0f));
    
    for (size_t i = 0; i < num_points; ++i) {
        for (size_t j = i + 1; j < num_points; ++j) {
            float dx = line_pixels[i].x - line_pixels[j].x;
            float dy = line_pixels[i].y - line_pixels[j].y;
            float dist = dx * dx + dy * dy;
            
            distance_matrix[i][j] = dist;
            distance_matrix[j][i] = dist;
        }
    }
    
    // Initialize the ordered list with the starting point
    std::vector<Point2D<float>> ordered_pixels;
    ordered_pixels.reserve(num_points);
    ordered_pixels.push_back(line_pixels[start_point_idx]);
    
    // Create a visited flag array
    std::vector<bool> visited(num_points, false);
    visited[start_point_idx] = true;
    
    // Track minimum distances
    std::vector<float> min_distances;
    min_distances.reserve(num_points - 1);

    // Iteratively find the nearest neighbor
    size_t current_index = start_point_idx;
    size_t points_remaining = num_points - 1;
    
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
            break; // Safety check
        }
        
        // Add the nearest point to the ordered list
        ordered_pixels.push_back(line_pixels[nearest_neighbor_index]);
        min_distances.push_back(nearest_dist);
        
        // Mark as visited and update current point
        visited[nearest_neighbor_index] = true;
        current_index = nearest_neighbor_index;
        points_remaining--;
    }

    // Check if we need to flip the line based on which endpoint is closer to the origin
    bool should_flip = false;
    
    if (!ordered_pixels.empty()) {
        float dist_first_to_origin = std::pow(ordered_pixels.front().x - origin.x, 2) + 
                                    std::pow(ordered_pixels.front().y - origin.y, 2);
        float dist_last_to_origin = std::pow(ordered_pixels.back().x - origin.x, 2) + 
                                   std::pow(ordered_pixels.back().y - origin.y, 2);
                                   
        should_flip = dist_first_to_origin > dist_last_to_origin;
        
        if (should_flip) {
            std::reverse(ordered_pixels.begin(), ordered_pixels.end());
        }
    }

    return Line2D(ordered_pixels);
}
