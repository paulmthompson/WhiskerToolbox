#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


inline Line2D create_line(std::vector<float> const & x, std::vector<float> const & y) {
    auto new_line = Line2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++) {
        new_line[i] = Point2D<float>{x[i], y[i]};
    }

    return new_line;
}


inline void smooth_line(Line2D & line) {
    if (line.size() < 3) return;// No need to smooth if less than 3 points

    Line2D smoothed_line;
    smoothed_line.push_back(line.front());// First point remains the same

    for (std::size_t i = 1; i < line.size() - 1; ++i) {
        float const smoothed_x = (line[i - 1].x + line[i].x + line[i + 1].x) / 3.0f;
        float const smoothed_y = (line[i - 1].y + line[i].y + line[i + 1].y) / 3.0f;
        smoothed_line.push_back(Point2D<float>{smoothed_x, smoothed_y});
    }

    smoothed_line.push_back(line.back());// Last point remains the same
    line = std::move(smoothed_line);
}


inline std::vector<uint8_t> line_to_image(Line2D & line, int height, int width) {
    auto image = std::vector<uint8_t>(static_cast<size_t>(height * width));

    for (auto point: line) {
        auto x = std::lround(point.x);
        auto y = std::lround(point.y);

        auto index = width * y + x;
        image[index] = 255;
    }

    return image;
}

/**
 * @brief Calculate the position along a line at a given percentage of cumulative length
 * 
 * @param line The line to calculate position on
 * @param percentage Percentage along the line (0.0 = start, 1.0 = end)
 * @return Point2D<float> The interpolated position along the line
 */
inline Point2D<float> get_position_at_percentage(Line2D const & line, float percentage) {
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
        float dx = line[i].x - line[i-1].x;
        float dy = line[i].y - line[i-1].y;
        float segment_length = std::sqrt(dx * dx + dy * dy);
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
            
            // Linear interpolation between points
            Point2D<float> p1 = line[i-1];
            Point2D<float> p2 = line[i];
            
            return Point2D<float>{
                p1.x + t * (p2.x - p1.x),
                p1.y + t * (p2.y - p1.y)
            };
        }
    }
    
    // If we reach here, return the last point
    return line.back();
}

#endif// DATAMANAGER_LINES_HPP
