#ifndef POINT_GEOMETRY_HPP
#define POINT_GEOMETRY_HPP

#include "CoreGeometry/points.hpp"

#include <cmath>

template <typename T>
float calc_distance2(Point2D<T> const & p1, Point2D<T> const & p2) {
    return (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
}

template <typename T>
float calc_distance(Point2D<T> const & p1, Point2D<T> const & p2) {
    return std::sqrt(calc_distance2(p1, p2));
}

template <typename T>
Point2D<T> interpolate_point(Point2D<T> const & p1, Point2D<T> const & p2, float t) {
    return {p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t};
}


extern template float calc_distance2(Point2D<float> const & p1, Point2D<float> const & p2);
extern template float calc_distance(Point2D<float> const & p1, Point2D<float> const & p2);
extern template Point2D<float> interpolate_point(Point2D<float> const & p1, Point2D<float> const & p2, float t);

#endif// POINT_GEOMETRY_HPP
