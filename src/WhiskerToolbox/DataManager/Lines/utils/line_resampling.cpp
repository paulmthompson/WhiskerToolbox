#include "line_resampling.hpp"

#include <cmath>  // For std::sqrt, std::abs

// Function definition moved from mask_to_line.cpp
Line2D resample_line_points(
    const Line2D& input_points,
    float target_spacing) {
    if (input_points.empty() || target_spacing <= 1e-6) {
        return input_points; // Return original if no points or invalid spacing
    }
    if (input_points.size() == 1) {
        return input_points; // Return single point as is
    }

    Line2D resampled_points;
    resampled_points.push_back(input_points.front()); // Always include the first point

    double accumulated_distance_since_last_sample = 0.0;
    double current_segment_traversed_distance = 0.0;

    for (size_t i = 0; i < input_points.size() - 1; ++i) {
        const Point2D<float>& p1 = input_points[i];
        const Point2D<float>& p2 = input_points[i+1];

        double segment_dx = p2.x - p1.x;
        double segment_dy = p2.y - p1.y;
        double segment_length = std::sqrt(segment_dx * segment_dx + segment_dy * segment_dy);

        if (segment_length < 1e-6) continue; // Skip zero-length segments

        // Position along the current segment where the next sample point should be placed
        double dist_to_next_in_segment = target_spacing - accumulated_distance_since_last_sample;

        while (dist_to_next_in_segment <= segment_length - current_segment_traversed_distance) {
            double interpolation_factor = (current_segment_traversed_distance + dist_to_next_in_segment) / segment_length;
            
            float new_x = p1.x + static_cast<float>(segment_dx * interpolation_factor);
            float new_y = p1.y + static_cast<float>(segment_dy * interpolation_factor);
            // Avoid duplicate points if possible by checking distance to last added point
            if (resampled_points.empty() || 
                (std::abs(new_x - resampled_points.back().x) > 1e-3 || std::abs(new_y - resampled_points.back().y) > 1e-3)) {
                resampled_points.push_back({new_x, new_y});
            }
            
            current_segment_traversed_distance += dist_to_next_in_segment; // Advance position in current segment
            accumulated_distance_since_last_sample = 0.0; // Reset for next sample distance calculation
            dist_to_next_in_segment = target_spacing; // Next sample is a full target_spacing away
        }
        // Add the remaining part of the segment to the accumulated distance for the next segment
        accumulated_distance_since_last_sample += (segment_length - current_segment_traversed_distance);
        current_segment_traversed_distance = 0.0; // Reset for next segment
    }

    // Ensure the last point is always included if it wasn't the last one sampled
    if (!input_points.empty()) {
        const auto& last_original = input_points.back();
        bool add_last = true;
        if (!resampled_points.empty()) {
            const auto& last_sampled = resampled_points.back();
            if (std::abs(last_original.x - last_sampled.x) < 1e-3 && std::abs(last_original.y - last_sampled.y) < 1e-3) {
                add_last = false; // Already added or very close
            }
        }
        if (add_last) {
            resampled_points.push_back(last_original);
        }
    }
    // If, after all, only one point was added (the first one), and there are more original points, add the last one too.
    // This handles cases with very short lines or large target_spacing.
    if (resampled_points.size() == 1 && input_points.size() > 1) {
        resampled_points.push_back(input_points.back());
    }

    return resampled_points;
} 
