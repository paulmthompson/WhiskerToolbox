
#include "simplify_line.hpp"

#include <cmath>
#include <iostream>
#include <numbers>

Radian calculate_angle_radian(Point2D<float> const & p1, Point2D<float> const & p2, Point2D<float> const & p3) {
    float const dx1 = p2.x - p1.x;
    float const dy1 = p2.y - p1.y;
    float const dx2 = p3.x - p2.x;
    float const dy2 = p3.y - p2.y;
    float const dot_product = dx1 * dx2 + dy1 * dy2;
    float const magnitude1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float const magnitude2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    return Radian(std::acos(dot_product / (magnitude1 * magnitude2)));
}

Degree calculate_angle_degree(Point2D<float> const & p1, Point2D<float> const & p2, Point2D<float> const & p3) {
    return Degree(calculate_angle_radian(p1, p2, p3).getValue() * 180.0f / std::numbers::pi_v<float>);
}

void remove_extreme_angles(std::vector<Point2D<float>> & line, Degree tolerance) {
    if (line.size() < 3) {
        return;
    }

    for (size_t i = 1; i < line.size() - 1; ++i) {
        auto angle = calculate_angle_degree(line[i - 1], line[i], line[i + 1]);
        if (angle > tolerance) {
            if (i == 1) {
                line.erase(line.begin());
                std::cout << "Removed first point with angle " << angle.getValue() << std::endl;
            } else {
                line.erase(line.begin() + i + 1);
                std::cout << "Removed point with angle " << angle.getValue() << " at position " << i << std::endl;
            }
            --i;// Recalculate angle with the new neighbor
        }
    }
}
