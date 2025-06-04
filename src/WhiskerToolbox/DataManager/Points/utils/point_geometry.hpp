#ifndef POINT_GEOMETRY_HPP
#define POINT_GEOMETRY_HPP

#include "Points/points.hpp"

#include <cmath>

template <typename T>
float calc_distance2(Point2D<T> const & p1, Point2D<T> const & p2) {
    return (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
}

template <typename T>
float calc_distance(Point2D<T> const & p1, Point2D<T> const & p2) {
    return std::sqrt(calc_distance2(p1, p2));
}

#endif// POINT_GEOMETRY_HPP
