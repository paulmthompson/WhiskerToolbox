#ifndef NEURALYZER_GEOMETRY_POINTS_HPP
#define NEURALYZER_GEOMETRY_POINTS_HPP

#include <cstdint>

template<typename T>
struct Point2D {
    T x;
    T y;

    Point2D();

    Point2D(T x, T y);

    bool operator==(Point2D<T> const & other) const;
};

extern template struct Point2D<float>;
extern template struct Point2D<uint32_t>;

#endif // NEURALYZER_GEOMETRY_POINTS_HPP
