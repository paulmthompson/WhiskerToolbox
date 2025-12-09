#include "CoreGeometry/point_geometry.hpp"

Point2D<float> scale_point(Point2D<float> const & point, ImageSize const & from_size, ImageSize const & to_size) {
    // Handle invalid image sizes
    if (from_size.width <= 0 || from_size.height <= 0 ||
        to_size.width <= 0 || to_size.height <= 0) {
        return point;// Return original point if any dimension is invalid
    }

    float scale_x = static_cast<float>(to_size.width) / static_cast<float>(from_size.width);
    float scale_y = static_cast<float>(to_size.height) / static_cast<float>(from_size.height);

    return Point2D<float>{
            point.x * scale_x,
            point.y * scale_y};
}

template float calc_distance2(Point2D<float> const & p1, Point2D<float> const & p2);
template float calc_distance(Point2D<float> const & p1, Point2D<float> const & p2);
template Point2D<float> interpolate_point(Point2D<float> const & p1, Point2D<float> const & p2, float t);
