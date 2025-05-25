#include "line_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

float calculate_line_length(Line2D const & line) {
    if (line.size() < 2) {
        return 0.0f;
    }
    
    float total_length = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        float dx = line[i].x - line[i-1].x;
        float dy = line[i].y - line[i-1].y;
        total_length += std::sqrt(dx * dx + dy * dy);
    }
    
    return total_length;
}

std::vector<float> calculate_cumulative_distances(Line2D const & line) {
    std::vector<float> distances;
    if (line.empty()) {
        return distances;
    }
    
    distances.reserve(line.size());
    distances.push_back(0.0f); // First point is at distance 0
    
    for (size_t i = 1; i < line.size(); ++i) {
        float dx = line[i].x - line[i-1].x;
        float dy = line[i].y - line[i-1].y;
        float segment_length = std::sqrt(dx * dx + dy * dy);
        distances.push_back(distances.back() + segment_length);
    }
    
    return distances;
}

std::optional<Point2D<float>> point_at_distance(
    Line2D const & line, 
    float target_distance, 
    bool use_interpolation) {
    
    if (line.empty()) {
        return std::nullopt;
    }
    
    if (line.size() == 1) {
        return line[0];
    }
    
    // Calculate cumulative distances
    std::vector<float> distances = calculate_cumulative_distances(line);
    
    // Clamp target distance to valid range
    target_distance = std::max(0.0f, std::min(target_distance, distances.back()));
    
    // Find the segment containing the target distance
    auto it = std::lower_bound(distances.begin(), distances.end(), target_distance);
    
    if (it == distances.end()) {
        // Target distance is beyond the end of the line
        return line.back();
    }
    
    size_t index = std::distance(distances.begin(), it);
    
    if (index == 0 || *it == target_distance || !use_interpolation) {
        // Exact match or no interpolation requested
        return line[index];
    }
    
    // Interpolate between points
    size_t prev_index = index - 1;
    float segment_start_dist = distances[prev_index];
    float segment_end_dist = distances[index];
    float segment_length = segment_end_dist - segment_start_dist;
    
    if (segment_length < 1e-6f) {
        // Segment is too short for meaningful interpolation
        return line[prev_index];
    }
    
    float t = (target_distance - segment_start_dist) / segment_length;
    
    Point2D<float> const & p1 = line[prev_index];
    Point2D<float> const & p2 = line[index];
    
    Point2D<float> interpolated_point;
    interpolated_point.x = p1.x + t * (p2.x - p1.x);
    interpolated_point.y = p1.y + t * (p2.y - p1.y);
    
    return interpolated_point;
}

std::optional<Point2D<float>> point_at_fractional_position(
    Line2D const & line, 
    float position, 
    bool use_interpolation) {
    
    if (line.empty()) {
        return std::nullopt;
    }
    
    // Clamp position to valid range
    position = std::max(0.0f, std::min(1.0f, position));
    
    float total_length = calculate_line_length(line);
    if (total_length < 1e-6f) {
        // Line has no length (all points are the same)
        return line[0];
    }
    
    float target_distance = position * total_length;
    return point_at_distance(line, target_distance, use_interpolation);
}

std::vector<Point2D<float>> extract_line_subsegment_by_distance(
    Line2D const & line,
    float start_position,
    float end_position,
    bool preserve_original_spacing) {
    
    if (line.empty()) {
        return {};
    }
    
    if (line.size() == 1) {
        return {line[0]};
    }
    
    // Ensure valid range
    start_position = std::max(0.0f, std::min(1.0f, start_position));
    end_position = std::max(0.0f, std::min(1.0f, end_position));
    
    if (start_position >= end_position) {
        return {};
    }
    
    std::vector<float> distances = calculate_cumulative_distances(line);
    float total_length = distances.back();
    
    if (total_length < 1e-6f) {
        // Line has no length
        return {line[0]};
    }
    
    float start_distance = start_position * total_length;
    float end_distance = end_position * total_length;
    
    std::vector<Point2D<float>> subsegment;
    
    if (preserve_original_spacing) {
        // Include all original points within the distance range
        for (size_t i = 0; i < line.size(); ++i) {
            if (distances[i] >= start_distance && distances[i] <= end_distance) {
                subsegment.push_back(line[i]);
            }
        }
        
        // Add interpolated start point if it's not already included
        if (subsegment.empty() || distances[0] > start_distance) {
            auto start_point = point_at_distance(line, start_distance, true);
            if (start_point.has_value()) {
                subsegment.insert(subsegment.begin(), start_point.value());
            }
        }
        
        // Add interpolated end point if it's not already included
        if (subsegment.empty() || distances.back() < end_distance) {
            auto end_point = point_at_distance(line, end_distance, true);
            if (end_point.has_value() && 
                (subsegment.empty() || 
                 std::abs(subsegment.back().x - end_point.value().x) > 1e-6f ||
                 std::abs(subsegment.back().y - end_point.value().y) > 1e-6f)) {
                subsegment.push_back(end_point.value());
            }
        }
    } else {
        // Create a resampled subsegment with interpolated points
        auto start_point = point_at_distance(line, start_distance, true);
        auto end_point = point_at_distance(line, end_distance, true);
        
        if (start_point.has_value()) {
            subsegment.push_back(start_point.value());
        }
        
        // Add intermediate original points
        for (size_t i = 0; i < line.size(); ++i) {
            if (distances[i] > start_distance && distances[i] < end_distance) {
                subsegment.push_back(line[i]);
            }
        }
        
        if (end_point.has_value() && 
            (subsegment.empty() || 
             std::abs(subsegment.back().x - end_point.value().x) > 1e-6f ||
             std::abs(subsegment.back().y - end_point.value().y) > 1e-6f)) {
            subsegment.push_back(end_point.value());
        }
    }
    
    return subsegment;
} 