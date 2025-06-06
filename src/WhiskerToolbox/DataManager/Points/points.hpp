#ifndef DATAMANGER_POINTS_HPP
#define DATAMANGER_POINTS_HPP

template<typename T>
struct Point2D {
    T x;
    T y;

    bool operator==(Point2D<T> const & other) const {
        return x == other.x && y == other.y;
    }
};

extern template struct Point2D<float>;

#endif // DATAMANGER_POINTS_HPP
