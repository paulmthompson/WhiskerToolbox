
#ifndef WHISKERTOOLBOX_SIMPLIFY_LINE_HPP
#define WHISKERTOOLBOX_SIMPLIFY_LINE_HPP

#define _USE_MATH_DEFINES // Why is this here?

#include "DataManager/Points/points.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>
#include <iostream>
#include <vector>

class Radian {
public:
    explicit Radian(float value) : value_(value) {}

    float getValue() const {
        return value_;
    }


    bool operator>(const Radian& other) const {
        return value_ > other.value_;
    }

    bool operator<(const Radian& other) const {
        return value_ < other.value_;
    }

private:
    float value_;
};

class Degree {
public:
    explicit Degree(float value) : value_(value) {}

    float getValue() const {
        return value_;
    }

    bool operator>(const Degree& other) const {
        return value_ > other.value_;
    }

    bool operator<(const Degree& other) const {
        return value_ < other.value_;
    }

private:
    float value_;
};


Radian calculate_angle_radian(const Point2D<float>& p1, const Point2D<float>& p2, const Point2D<float>& p3) {
    float dx1 = p2.x - p1.x;
    float dy1 = p2.y - p1.y;
    float dx2 = p3.x - p2.x;
    float dy2 = p3.y - p2.y;
    float dot_product = dx1 * dx2 + dy1 * dy2;
    float magnitude1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float magnitude2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    return Radian(std::acos(dot_product / (magnitude1 * magnitude2)));
}

Degree calculate_angle_degree(const Point2D<float>& p1, const Point2D<float>& p2, const Point2D<float>& p3) {
    return Degree(calculate_angle_radian(p1, p2, p3).getValue() * 180.0f / M_PI);
}

void remove_extreme_angles(std::vector<Point2D<float>>& line, Degree tolerance)
{
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
            --i; // Recalculate angle with the new neighbor

        }
    }
}

#endif //WHISKERTOOLBOX_SIMPLIFY_LINE_HPP
