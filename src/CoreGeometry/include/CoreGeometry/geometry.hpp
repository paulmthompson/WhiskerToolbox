#ifndef NEURALYZER_GEOMETRY_HPP
#define NEURALYZER_GEOMETRY_HPP

#include "ImageSize.hpp"
#include "points.hpp"

#include <cstdint>
#include <vector>

template <typename T>
struct BoundingBox {
    Point2D<T> min_point;
    Point2D<T> max_point;

    BoundingBox(Point2D<T> const & min_point, Point2D<T> const & max_point)
        : min_point(min_point), max_point(max_point) {}

    BoundingBox(T const & min_x, T const & min_y, T const & max_x, T const & max_y)
        : min_point({min_x, min_y}), max_point({max_x, max_y}) {}

    T width() const { return max_point.x - min_point.x; }
    T height() const { return max_point.y - min_point.y; }
    T area() const { return width() * height(); }

    bool contains(Point2D<T> const & point) const {
        return point.x >= min_point.x && point.x <= max_point.x &&
               point.y >= min_point.y && point.y <= max_point.y;
    }

    bool intersects(BoundingBox const & other) const {
        return !(max_point.x < other.min_point.x || min_point.x > other.max_point.x ||
                 max_point.y < other.min_point.y || min_point.y > other.max_point.y);
    }

    bool contains(BoundingBox const & other) const {
        return contains(other.min_point) && contains(other.max_point);
    }

    bool operator==(BoundingBox const & other) const {
        return min_point == other.min_point && max_point == other.max_point;
    }

    bool operator!=(BoundingBox const & other) const {
        return !(*this == other);
    }

    bool operator<(BoundingBox const & other) const {
        return min_point < other.min_point || (min_point == other.min_point && max_point < other.max_point);
    }
};




#endif // NEURALYZER_GEOMETRY_HPP