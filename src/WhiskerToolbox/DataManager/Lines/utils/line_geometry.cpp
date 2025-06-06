#include "line_geometry.hpp"

#include "Points/utils/point_geometry.hpp"

float calc_length(Line2D const & line) {
    if (line.size() < 2) {
        return 0.0f;
    }

    float total_length = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        total_length += calc_distance(line[i], line[i-1]);
    }

    return total_length;
}

float calc_length2(Line2D const & line) {
    if (line.size() < 2) {
        return 0.0f;
    }

    float total_length = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        total_length += calc_distance2(line[i], line[i-1]);
    }

    return total_length;
}

std::vector<float> calc_cumulative_length_vector(Line2D const & line) {
    std::vector<float> distances;
    if (line.empty()) {
        return distances;
    }

    distances.reserve(line.size());
    distances.push_back(0.0f); // First point is at distance 0

    for (size_t i = 1; i < line.size(); ++i) {
        float const segment_length = calc_distance(line[i], line[i-1]);
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
    std::vector<float> distances = calc_cumulative_length_vector(line);

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

    return interpolate_point(line[prev_index], line[index], t);
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

    float total_length = calc_length(line);
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

    std::vector<float> distances = calc_cumulative_length_vector(line);
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

Point2D<float> get_position_at_percentage(Line2D const & line, float percentage) {
    if (line.empty()) {
        return Point2D<float>{0.0f, 0.0f};
    }

    if (line.size() == 1) {
        return line[0];
    }

    // Clamp percentage to valid range
    percentage = std::max(0.0f, std::min(1.0f, percentage));

    // Calculate cumulative distances
    std::vector<float> cumulative_distances;
    cumulative_distances.reserve(line.size());
    cumulative_distances.push_back(0.0f);

    float total_length = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        float segment_length = calc_distance(line[i], line[i-1]);
        total_length += segment_length;
        cumulative_distances.push_back(total_length);
    }

    if (total_length == 0.0f) {
        return line[0];
    }

    // Find target distance
    float target_distance = percentage * total_length;

    // Find the segment containing the target distance
    for (size_t i = 1; i < cumulative_distances.size(); ++i) {
        if (target_distance <= cumulative_distances[i]) {
            // Interpolate within this segment
            float segment_start_distance = cumulative_distances[i-1];
            float segment_end_distance = cumulative_distances[i];
            float segment_length = segment_end_distance - segment_start_distance;

            if (segment_length == 0.0f) {
                return line[i-1];
            }

            float t = (target_distance - segment_start_distance) / segment_length;

            return interpolate_point(line[i-1], line[i], t);
        }
    }

    // If we reach here, return the last point
    return line.back();
}

Line2D get_segment_between_percentages(Line2D const & line, float start_percentage, float end_percentage) {
    if (line.empty() || line.size() < 2) {
        return Line2D{};
    }

    // Clamp percentages to valid range
    start_percentage = std::max(0.0f, std::min(1.0f, start_percentage));
    end_percentage = std::max(0.0f, std::min(1.0f, end_percentage));

    // Ensure start is before end
    if (start_percentage >= end_percentage) {
        return Line2D{};
    }

    // Calculate cumulative distances
    std::vector<float> cumulative_distances;
    cumulative_distances.reserve(line.size());
    cumulative_distances.push_back(0.0f);

    float total_distance = 0.0f;
    for (size_t i = 1; i < line.size(); ++i) {
        float segment_distance = calc_distance(line[i], line[i-1]);
        total_distance += segment_distance;
        cumulative_distances.push_back(total_distance);
    }

    if (total_distance == 0.0f) {
        return Line2D{};
    }

    // Calculate target distances
    float start_distance = start_percentage * total_distance;
    float end_distance = end_percentage * total_distance;

    Line2D segment;
    bool started = false;

    for (size_t i = 0; i < line.size() - 1; ++i) {
        float current_distance = cumulative_distances[i];
        float next_distance = cumulative_distances[i + 1];

        // Check if this segment contains the start point
        if (!started && start_distance >= current_distance && start_distance <= next_distance) {
            started = true;
            if (current_distance == next_distance) {
                // Degenerate segment, add the point
                segment.push_back(line[i]);
            } else {
                // Interpolate start point
                float t = (start_distance - current_distance) / (next_distance - current_distance);
                segment.push_back(interpolate_point(line[i], line[i+1], t));
            }
        }

        // If we've started, check if this segment contains the end point
        if (started && end_distance >= current_distance && end_distance <= next_distance) {
            if (current_distance == next_distance) {
                // Degenerate segment, add the point if not already added
                if (segment.empty() || !(segment.back().x == line[i].x && segment.back().y == line[i].y)) {
                    segment.push_back(line[i]);
                }
            } else {
                // Interpolate end point
                float t = (end_distance - current_distance) / (next_distance - current_distance);
                segment.push_back(interpolate_point(line[i], line[i+1], t));
            }
            break; // We've reached the end
        }

        // If we've started but haven't reached the end, add the next point
        if (started && next_distance < end_distance) {
            segment.push_back(line[i+1]);
        }
    }

    return segment;
}


