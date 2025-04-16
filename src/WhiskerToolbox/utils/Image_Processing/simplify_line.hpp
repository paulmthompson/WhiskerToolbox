
#ifndef WHISKERTOOLBOX_SIMPLIFY_LINE_HPP
#define WHISKERTOOLBOX_SIMPLIFY_LINE_HPP

#include "DataManager/Points/points.hpp"

#include <vector>

class Radian {
public:
    explicit Radian(float value)
        : value_(value) {}

    [[nodiscard]] float getValue() const {
        return value_;
    }

    bool operator>(Radian const & other) const {
        return value_ > other.value_;
    }

    bool operator<(Radian const & other) const {
        return value_ < other.value_;
    }

private:
    float value_;
};

class Degree {
public:
    explicit Degree(float value)
        : value_(value) {}

    [[nodiscard]] float getValue() const {
        return value_;
    }

    bool operator>(Degree const & other) const {
        return value_ > other.value_;
    }

    bool operator<(Degree const & other) const {
        return value_ < other.value_;
    }

private:
    float value_;
};


Radian calculate_angle_radian(Point2D<float> const & p1, Point2D<float> const & p2, Point2D<float> const & p3);

Degree calculate_angle_degree(Point2D<float> const & p1, Point2D<float> const & p2, Point2D<float> const & p3);

void remove_extreme_angles(std::vector<Point2D<float>> & line, Degree tolerance);

#endif//WHISKERTOOLBOX_SIMPLIFY_LINE_HPP
