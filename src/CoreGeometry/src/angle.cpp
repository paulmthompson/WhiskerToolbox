#include "CoreGeometry/angle.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kEpsilonLength = 1e-8f;
constexpr float kDegenerateSpanEps = 1e-4f;

[[nodiscard]] float wrap_degrees_open_interval_pm180(float degrees) {
    while (degrees > 180.0f) {
        degrees -= 360.0f;
    }
    while (degrees <= -180.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

}// namespace

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

    return wrap_degrees_open_interval_pm180(normalized_angle);
}

ArcLengthFractionSpan compute_sliding_secant_span(float position, float window) {
    position = std::clamp(position, 0.0f, 1.0f);
    window = std::clamp(window, 0.0f, 1.0f);

    if (window >= 1.0f - kEpsilonLength) {
        return {0.0f, 1.0f};
    }

    float const half = window * 0.5f;
    float low = position - half;
    float high = position + half;

    if (low < 0.0f) {
        high -= low;
        low = 0.0f;
    }
    if (high > 1.0f) {
        float const delta = high - 1.0f;
        low -= delta;
        high = 1.0f;
    }
    low = std::max(0.0f, low);
    high = std::min(1.0f, high);

    if (high <= low) {
        low = std::clamp(position - kDegenerateSpanEps, 0.0f, 1.0f - kDegenerateSpanEps);
        high = std::clamp(position + kDegenerateSpanEps, low + kDegenerateSpanEps, 1.0f);
        if (high <= low) {
            return {0.0f, 1.0f};
        }
    }

    return {low, high};
}

PlanarOrthonormalBasis2D make_planar_orthonormal_basis(float axis_positive_x_x,
                                                      float axis_positive_x_y,
                                                      float axis_positive_y_x,
                                                      float axis_positive_y_y) {
    float len_x = std::hypot(axis_positive_x_x, axis_positive_x_y);
    float ex_x = 1.0f;
    float ex_y = 0.0f;
    if (len_x > kEpsilonLength) {
        ex_x = axis_positive_x_x / len_x;
        ex_y = axis_positive_x_y / len_x;
    }

    float const dot = axis_positive_y_x * ex_x + axis_positive_y_y * ex_y;
    float y_ortho_x = axis_positive_y_x - dot * ex_x;
    float y_ortho_y = axis_positive_y_y - dot * ex_y;
    float len_y = std::hypot(y_ortho_x, y_ortho_y);

    float ey_x = -ex_y;
    float ey_y = ex_x;
    if (len_y > kEpsilonLength) {
        ey_x = y_ortho_x / len_y;
        ey_y = y_ortho_y / len_y;
    }

    float const cross_z = ex_x * ey_y - ex_y * ey_x;
    if (cross_z < 0.0f) {
        ey_x = -ey_x;
        ey_y = -ey_y;
    }

    return {ex_x, ex_y, ey_x, ey_y};
}

float angle_degrees_vector_in_basis(float vx, float vy, PlanarOrthonormalBasis2D const & basis) {
    float const tx = vx * basis.ex_x + vy * basis.ex_y;
    float const ty = vx * basis.ey_x + vy * basis.ey_y;
    float const radians = std::atan2(ty, tx);
    float const degrees = radians * 180.0f / static_cast<float>(std::numbers::pi);
    return wrap_degrees_open_interval_pm180(degrees);
}

float calculate_direct_angle_secant(Line2D const & line,
                                    float t_low,
                                    float t_high,
                                    PlanarOrthonormalBasis2D const & basis) {
    if (line.size() < 2) {
        return 0.0f;
    }

    t_low = std::clamp(t_low, 0.0f, 1.0f);
    t_high = std::clamp(t_high, 0.0f, 1.0f);
    if (t_high < t_low) {
        std::swap(t_low, t_high);
    }

    auto const p_lo = point_at_fractional_position(line, t_low, true);
    auto const p_hi = point_at_fractional_position(line, t_high, true);
    if (!p_lo.has_value() || !p_hi.has_value()) {
        return 0.0f;
    }

    float const vx = p_hi->x - p_lo->x;
    float const vy = p_hi->y - p_lo->y;
    if (vx * vx + vy * vy < 1e-20f) {
        return 0.0f;
    }

    return angle_degrees_vector_in_basis(vx, vy, basis);
}
