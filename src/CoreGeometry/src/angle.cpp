#include "CoreGeometry/angle.hpp"

#include "CoreGeometry/lines.hpp"

#include <cmath>
#include <numbers>

float normalize_angle(float raw_angle, float reference_x, float reference_y) {
    // Calculate the angle of the reference vector (if not the default x-axis)
    float reference_angle = 0.0f;
    if (!(reference_x == 1.0f && reference_y == 0.0f)) {
        reference_angle = std::atan2(reference_y, reference_x);
        // Convert to degrees
        reference_angle *= 180.0f / static_cast<float>(std::numbers::pi);
    }

    // Adjust the raw angle by subtracting the reference angle
    float normalized_angle = raw_angle - reference_angle;

    // Normalize to range [-180, 180]
    while (normalized_angle > 180.0f) normalized_angle -= 360.0f;
    while (normalized_angle <= -180.0f) normalized_angle += 360.0f;

    return normalized_angle;
}


// Calculate angle using direct point comparison
float calculate_direct_angle(Line2D const & line, float position, float reference_x, float reference_y) {
    if (line.size() < 2) {
        return 0.0f;
    }

    // Calculate the index of the position point, ensuring we never select the base point
    // when computing the direction from the first point. This avoids a zero-length vector
    // for 2-point lines at position 1.0.
    auto idx = static_cast<size_t>(position * static_cast<float>((line.size() - 1)));
    if (idx == 0) {
        idx = 1; // always pick at least the second point
    } else if (idx >= line.size()) {
        idx = line.size() - 1; // clamp to last valid index
    } else if (idx >= line.size() - 1) {
        idx = line.size() - 1; // for end positions, use the last point
    }

    Point2D<float> const base = line[0];
    Point2D<float> const pos = line[idx];

    // Calculate angle in radians (atan2 returns value in range [-π, π])
    float raw_angle = std::atan2(pos.y - base.y, pos.x - base.x);

    // Convert to degrees
    float angle_degrees = raw_angle * 180.0f / static_cast<float>(std::numbers::pi);

    // Normalize with respect to the reference vector
    return normalize_angle(angle_degrees, reference_x, reference_y);
}
