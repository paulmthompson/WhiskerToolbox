#include "CoreGeometry/line_geometry.hpp"

#include "CoreGeometry/point_geometry.hpp"

#include <optional>

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

    auto index = static_cast<size_t>(std::distance(distances.begin(), it));

    if (index == 0 || *it == target_distance || !use_interpolation) {
        // Exact match or no interpolation requested
        return line[index];
    }

    // Interpolate between points
    size_t const prev_index = index - 1;
    float const segment_start_dist = distances[prev_index];
    float const segment_end_dist = distances[index];
    float const segment_length = segment_end_dist - segment_start_dist;

    if (segment_length < 1e-6f) {
        // Segment is too short for meaningful interpolation
        return line[prev_index];
    }

    float const t = (target_distance - segment_start_dist) / segment_length;

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

    float const total_length = calc_length(line);
    if (total_length < 1e-6f) {
        // Line has no length (all points are the same)
        return line[0];
    }

    float const target_distance = position * total_length;
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
    float const total_length = distances.back();

    if (total_length < 1e-6f) {
        // Line has no length
        return {line[0]};
    }

    float const start_distance = start_position * total_length;
    float const end_distance = end_position * total_length;

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
        float const segment_length = calc_distance(line[i], line[i - 1]);
        total_length += segment_length;
        cumulative_distances.push_back(total_length);
    }

    if (total_length == 0.0f) {
        return line[0];
    }

    // Find target distance
    float const target_distance = percentage * total_length;

    // Find the segment containing the target distance
    for (size_t i = 1; i < cumulative_distances.size(); ++i) {
        if (target_distance <= cumulative_distances[i]) {
            // Interpolate within this segment
            float const segment_start_distance = cumulative_distances[i - 1];
            float const segment_end_distance = cumulative_distances[i];
            float const segment_length = segment_end_distance - segment_start_distance;

            if (segment_length == 0.0f) {
                return line[i-1];
            }

            float const t = (target_distance - segment_start_distance) / segment_length;

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
        float const segment_distance = calc_distance(line[i], line[i - 1]);
        total_distance += segment_distance;
        cumulative_distances.push_back(total_distance);
    }

    if (total_distance == 0.0f) {
        return Line2D{};
    }

    // Calculate target distances
    float const start_distance = start_percentage * total_distance;
    float const end_distance = end_percentage * total_distance;

    Line2D segment;
    bool started = false;

    for (size_t i = 0; i < line.size() - 1; ++i) {
        float const current_distance = cumulative_distances[i];
        float const next_distance = cumulative_distances[i + 1];

        // Check if this segment contains the start point
        if (!started && start_distance >= current_distance && start_distance <= next_distance) {
            started = true;
            if (current_distance == next_distance) {
                // Degenerate segment, add the point
                segment.push_back(line[i]);
            } else {
                // Interpolate start point
                float const t = (start_distance - current_distance) / (next_distance - current_distance);
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
                float const t = (end_distance - current_distance) / (next_distance - current_distance);
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

Point2D<float> calculate_perpendicular_direction(Line2D const & line, size_t vertex_index) {
    if (line.size() < 2 || vertex_index >= line.size()) {
        return Point2D<float>{0.0f, 0.0f};
    }

    if (vertex_index == 0) {
        // First vertex - use the segment from vertex 0 to vertex 1
        Point2D<float> const segment = {
                line[1].x - line[0].x,
                line[1].y - line[0].y};

        // Calculate perpendicular: rotate 90 degrees counterclockwise
        Point2D<float> perp = {-segment.y, segment.x};
        
        // Normalize
        float const length = std::sqrt(perp.x * perp.x + perp.y * perp.y);
        if (length > 0.0f) {
            perp.x /= length;
            perp.y /= length;
        }
        
        return perp;
    } else if (vertex_index == line.size() - 1) {
        // Last vertex - use the segment from vertex n-2 to vertex n-1
        Point2D<float> const segment = {
                line[vertex_index].x - line[vertex_index - 1].x,
                line[vertex_index].y - line[vertex_index - 1].y};

        // Calculate perpendicular: rotate 90 degrees counterclockwise
        Point2D<float> perp = {-segment.y, segment.x};
        
        // Normalize
        float const length = std::sqrt(perp.x * perp.x + perp.y * perp.y);
        if (length > 0.0f) {
            perp.x /= length;
            perp.y /= length;
        }
        
        return perp;
    } else {
        // Internal vertex - average perpendiculars from both adjacent segments
        
        // Segment from previous vertex to current vertex
        Point2D<float> const segment1 = {
                line[vertex_index].x - line[vertex_index - 1].x,
                line[vertex_index].y - line[vertex_index - 1].y};

        // Segment from current vertex to next vertex
        Point2D<float> const segment2 = {
                line[vertex_index + 1].x - line[vertex_index].x,
                line[vertex_index + 1].y - line[vertex_index].y};

        // Calculate perpendiculars
        Point2D<float> perp1 = {-segment1.y, segment1.x};
        Point2D<float> perp2 = {-segment2.y, segment2.x};
        
        // Normalize both perpendiculars
        float const length1 = std::sqrt(perp1.x * perp1.x + perp1.y * perp1.y);
        float const length2 = std::sqrt(perp2.x * perp2.x + perp2.y * perp2.y);

        if (length1 > 0.0f) {
            perp1.x /= length1;
            perp1.y /= length1;
        }
        
        if (length2 > 0.0f) {
            perp2.x /= length2;
            perp2.y /= length2;
        }
        
        // Average the perpendiculars
        Point2D<float> avg_perp = {
            (perp1.x + perp2.x) / 2.0f,
            (perp1.y + perp2.y) / 2.0f
        };
        
        // Normalize the average
        float const avg_length = std::sqrt(avg_perp.x * avg_perp.x + avg_perp.y * avg_perp.y);
        if (avg_length > 0.0f) {
            avg_perp.x /= avg_length;
            avg_perp.y /= avg_length;
        }
        
        return avg_perp;
    }
}

float point_to_line_segment_distance2(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end) {

    // If start and end are the same point, just return distance to that point
    if (line_start.x == line_end.x && line_start.y == line_end.y) {
        float dx = point.x - line_start.x;
        float dy = point.y - line_start.y;
        return dx * dx + dy * dy;
    }

    // Calculate the squared length of the line segment
    float line_length_squared = (line_end.x - line_start.x) * (line_end.x - line_start.x) +
                                (line_end.y - line_start.y) * (line_end.y - line_start.y);

    // Calculate the projection of point onto the line segment
    float t = ((point.x - line_start.x) * (line_end.x - line_start.x) +
               (point.y - line_start.y) * (line_end.y - line_start.y)) /
              line_length_squared;

    // Clamp t to range [0, 1] to ensure we get distance to a point on the segment
    t = std::max(0.0f, std::min(1.0f, t));

    // Calculate the closest point on the segment
    float closest_x = line_start.x + t * (line_end.x - line_start.x);
    float closest_y = line_start.y + t * (line_end.y - line_start.y);

    // Calculate the distance from original point to closest point
    float dx = point.x - closest_x;
    float dy = point.y - closest_y;

    return dx * dx + dy * dy;
}

std::optional<Point2D<float>> line_segment_intersection(
    Point2D<float> const & p1, Point2D<float> const & p2,
    Point2D<float> const & p3, Point2D<float> const & p4) {
    
    // Calculate direction vectors
    float d1x = p2.x - p1.x;
    float d1y = p2.y - p1.y;
    float d2x = p4.x - p3.x;
    float d2y = p4.y - p3.y;
    
    // Calculate the denominator for the intersection calculation
    float denominator = d1x * d2y - d1y * d2x;
    
    // Check if lines are parallel (denominator is zero)
    if (std::abs(denominator) < 1e-10f) {
        return std::nullopt;
    }
    
    // Calculate parameters for intersection point
    float dx = p3.x - p1.x;
    float dy = p3.y - p1.y;
    
    float t1 = (dx * d2y - dy * d2x) / denominator;
    float t2 = (dx * d1y - dy * d1x) / denominator;
    
    // Check if intersection point lies within both line segments
    if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f) {
        // Calculate intersection point
        Point2D<float> intersection;
        intersection.x = p1.x + t1 * d1x;
        intersection.y = p1.y + t1 * d1y;
        
        return intersection;
    }
    
    return std::nullopt;
}


std::optional<std::pair<Point2D<float>, size_t>> find_line_intersection(
    Line2D const & line, Line2D const & reference_line) {
    
    if (line.size() < 2 || reference_line.size() < 2) {
        return std::nullopt;
    }
    
    // Check each segment of the main line against each segment of the reference line
    for (size_t i = 0; i < line.size() - 1; ++i) {
        for (size_t j = 0; j < reference_line.size() - 1; ++j) {
            auto intersection = line_segment_intersection(
                line[i], line[i + 1],
                reference_line[j], reference_line[j + 1]
            );
            
            if (intersection.has_value()) {
                return std::make_pair(intersection.value(), i);
            }
        }
    }
    
    return std::nullopt;
}

Line2D clip_line_at_intersection(
    Line2D const & line, 
    Line2D const & reference_line, 
    ClipSide clip_side) {
    
    if (line.size() < 2) {
        return line; // Return original line if too short
    }
    
    auto intersection_result = find_line_intersection(line, reference_line);
    
    if (!intersection_result.has_value()) {
        // No intersection found, return original line
        return line;
    }

    Point2D<float> const intersection_point = intersection_result->first;
    size_t const segment_index = intersection_result->second;

    Line2D clipped_line;
    
    if (clip_side == ClipSide::KeepBase) {
        // Keep from start to intersection
        for (size_t i = 0; i <= segment_index; ++i) {
            clipped_line.push_back(line[i]);
        }
        // Add intersection point if it's not the same as the last point
        if (clipped_line.empty() || 
            (std::abs(clipped_line.back().x - intersection_point.x) > 1e-6f ||
             std::abs(clipped_line.back().y - intersection_point.y) > 1e-6f)) {
            clipped_line.push_back(intersection_point);
        }
    } else { // ClipSide::KeepDistal
        // Keep from intersection to end
        clipped_line.push_back(intersection_point);
        for (size_t i = segment_index + 1; i < line.size(); ++i) {
            clipped_line.push_back(line[i]);
        }
    }
    
    return clipped_line;
}

float point_to_line_min_distance2(Point2D<float> const & point, Line2D const & line) {
    if (line.size() < 2) {
        // If the line has fewer than 2 points, it's not a valid line
        return std::numeric_limits<float>::max();
    }

    float min_distance = std::numeric_limits<float>::max();

    // Check each segment of the line
    for (size_t i = 0; i < line.size() - 1; ++i) {
        Point2D<float> const & segment_start = line[i];
        Point2D<float> const & segment_end = line[i + 1];

        float distance = point_to_line_segment_distance2(point, segment_start, segment_end);
        min_distance = std::min(min_distance, distance);
    }

    return min_distance;
}