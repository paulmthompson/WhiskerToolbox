#include "CoreGeometry/line_resampling.hpp"

#include <cmath>// For std::sqrt, std::abs

// Function definition moved from mask_to_line.cpp
Line2D resample_line_points(
        Line2D const & input_points,
        float target_spacing) {
    if (input_points.empty() || target_spacing <= 1e-6f) {
        return input_points;// Return original if no points or invalid spacing
    }
    if (input_points.size() == 1) {
        return input_points;// Return single point as is
    }

    Line2D resampled_points;
    resampled_points.push_back(input_points.front());// Always include the first point

    auto accumulated_distance_since_last_sample = 0.0f;
    auto current_segment_traversed_distance = 0.0f;

    for (size_t i = 0; i < input_points.size() - 1; ++i) {
        Point2D<float> const & p1 = input_points[i];
        Point2D<float> const & p2 = input_points[i + 1];

        auto segment_dx = p2.x - p1.x;
        auto segment_dy = p2.y - p1.y;
        auto segment_length = std::sqrt(segment_dx * segment_dx + segment_dy * segment_dy);

        if (segment_length < 1e-6f) continue;// Skip zero-length segments

        // Position along the current segment where the next sample point should be placed
        auto dist_to_next_in_segment = target_spacing - accumulated_distance_since_last_sample;

        while (dist_to_next_in_segment <= segment_length - current_segment_traversed_distance) {
            auto interpolation_factor = (current_segment_traversed_distance + dist_to_next_in_segment) / segment_length;

            float const new_x = p1.x + segment_dx * interpolation_factor;
            float const new_y = p1.y + segment_dy * interpolation_factor;
            // Avoid duplicate points if possible by checking distance to last added point
            if (resampled_points.empty() ||
                (std::abs(new_x - resampled_points.back().x) > 1e-3f || std::abs(new_y - resampled_points.back().y) > 1e-3f)) {
                resampled_points.push_back({new_x, new_y});
            }

            current_segment_traversed_distance += dist_to_next_in_segment;// Advance position in current segment
            accumulated_distance_since_last_sample = 0.0f;                 // Reset for next sample distance calculation
            dist_to_next_in_segment = target_spacing;                     // Next sample is a full target_spacing away
        }
        // Add the remaining part of the segment to the accumulated distance for the next segment
        accumulated_distance_since_last_sample += (segment_length - current_segment_traversed_distance);
        current_segment_traversed_distance = 0.0f;// Reset for next segment
    }

    // Ensure the last point is always included if it wasn't the last one sampled
    if (!input_points.empty()) {
        auto const & last_original = input_points.back();
        bool add_last = true;
        if (!resampled_points.empty()) {
            auto const & last_sampled = resampled_points.back();
            if (std::abs(last_original.x - last_sampled.x) < 1e-3f && std::abs(last_original.y - last_sampled.y) < 1e-3f) {
                add_last = false;// Already added or very close
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

/**
 * @brief Calculates the perpendicular distance from a point to a line segment.
 *
 * @param point The point to calculate distance from.
 * @param line_start The start point of the line segment.
 * @param line_end The end point of the line segment.
 * @return The perpendicular distance from the point to the line segment.
 */
static float perpendicular_distance(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end) {

    if (std::abs(line_end.x - line_start.x) < 1e-6f && std::abs(line_end.y - line_start.y) < 1e-6f) {
        // Line segment is actually a point, calculate distance to that point
        float const dx = point.x - line_start.x;
        float const dy = point.y - line_start.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Calculate the perpendicular distance from point to line segment
    float const A = line_end.y - line_start.y;
    float const B = line_start.x - line_end.x;
    float C = line_end.x * line_start.y - line_start.x * line_end.y;

    float const numerator = std::abs(A * point.x + B * point.y + C);
    float const denominator = std::sqrt(A * A + B * B);

    return numerator / denominator;
}

/**
 * @brief Recursive helper function for Douglas-Peucker algorithm.
 *
 * @param points The input points.
 * @param start_idx Starting index of the current segment.
 * @param end_idx Ending index of the current segment.
 * @param epsilon The maximum perpendicular distance tolerance.
 * @param result_indices Vector to store indices of points to keep.
 */
static void douglas_peucker_recursive(
        Line2D const & points,
        size_t start_idx,
        size_t end_idx,
        float epsilon,
        std::vector<bool> & keep_point) {

    if (end_idx <= start_idx + 1) {
        return;// No intermediate points to check
    }

    Point2D<float> const & start_point = points[start_idx];
    Point2D<float> const & end_point = points[end_idx];

    float max_distance = 0.0f;
    size_t max_distance_idx = start_idx;

    // Find the point with maximum perpendicular distance
    for (size_t i = start_idx + 1; i < end_idx; ++i) {
        float distance = perpendicular_distance(points[i], start_point, end_point);
        if (distance > max_distance) {
            max_distance = distance;
            max_distance_idx = i;
        }
    }

    // If the maximum distance is greater than epsilon, recursively simplify
    if (max_distance > epsilon) {
        keep_point[max_distance_idx] = true;

        // Recursively simplify the two sub-segments
        douglas_peucker_recursive(points, start_idx, max_distance_idx, epsilon, keep_point);
        douglas_peucker_recursive(points, max_distance_idx, end_idx, epsilon, keep_point);
    }
}

Line2D douglas_peucker_simplify(
        Line2D const & input_points,
        float epsilon) {

    if (input_points.size() <= 2) {
        return input_points;// No simplification possible for 2 or fewer points
    }

    if (epsilon <= 0.0f) {
        return input_points;// No simplification if epsilon is non-positive
    }

    // Vector to track which points to keep
    std::vector<bool> keep_point(input_points.size(), false);

    // Always keep the first and last points
    keep_point[0] = true;
    keep_point[input_points.size() - 1] = true;

    // Recursively simplify the line
    douglas_peucker_recursive(input_points, 0, input_points.size() - 1, epsilon, keep_point);

    // Build the result from kept points
    Line2D simplified_line;
    for (size_t i = 0; i < input_points.size(); ++i) {
        if (keep_point[i]) {
            simplified_line.push_back(input_points[i]);
        }
    }

    return simplified_line;
}
