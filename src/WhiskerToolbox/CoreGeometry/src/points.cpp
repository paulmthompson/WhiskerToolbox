#include "CoreGeometry/points.hpp"

template<typename T>
Point2D<T>::Point2D() : x(0), y(0) {}

template<typename T>
Point2D<T>::Point2D(T x, T y) : x(x), y(y) {}

template<typename T>
bool Point2D<T>::operator==(Point2D<T> const & other) const {
    return x == other.x && y == other.y;
}
    

template struct Point2D<float>;
template struct Point2D<uint32_t>;