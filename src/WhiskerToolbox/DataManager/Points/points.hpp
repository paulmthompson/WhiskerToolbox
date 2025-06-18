#ifndef DATAMANGER_POINTS_HPP
#define DATAMANGER_POINTS_HPP

#include <cstdint>

template<typename T>
struct Point2D {
    T x;
    T y;

    bool operator==(Point2D<T> const & other) const {
        return x == other.x && y == other.y;
    }
};

extern template struct Point2D<float>;
extern template struct Point2D<uint32_t>;

#endif // DATAMANGER_POINTS_HPP
