#include "CoreGeometry/angle.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"

#include <algorithm>
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


// Calculate angle using direct point comparison with interpolated position
float calculate_direct_angle(Line2D const & line, float position, float reference_x, float reference_y) {
    if (line.size() < 2) {
        return 0.0f;
    }

    // Clamp position to valid range
    position = std::clamp(position, 0.0f, 1.0f);

    // Interpolate the target point along the line's cumulative arc length.
    // This gives a smooth, evenly-parameterized result regardless of vertex spacing.
    auto interpolated = point_at_fractional_position(line, position);
    if (!interpolated.has_value()) {
        return 0.0f;
    }

    Point2D<float> const base = line[0];
    Point2D<float> const pos = interpolated.value();

    // Guard against zero-length vector (position very close to start)
    float const dx = pos.x - base.x;
    float const dy = pos.y - base.y;
    if (dx == 0.0f && dy == 0.0f) {
        return 0.0f;
    }

    // Calculate angle in radians (atan2 returns value in range [-π, π])
    float const raw_angle = std::atan2(dy, dx);

    // Convert to degrees
    float const angle_degrees = raw_angle * 180.0f / static_cast<float>(std::numbers::pi);

    // Normalize with respect to the reference vector
    return normalize_angle(angle_degrees, reference_x, reference_y);
}
