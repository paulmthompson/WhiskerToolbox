#ifndef BOUNDINGBOX_HPP
#define BOUNDINGBOX_HPP

#include "points.hpp"

struct BoundingBox {
    float min_x, min_y, max_x, max_y;

    BoundingBox(float min_x, float min_y, float max_x, float max_y)
        : min_x(min_x),
          min_y(min_y),
          max_x(max_x),
          max_y(max_y) {}

    bool contains(float x, float y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }

    bool intersects(BoundingBox const & other) const {
        return !(other.min_x > max_x || other.max_x < min_x ||
                 other.min_y > max_y || other.max_y < min_y);
    }

    float width() const { return max_x - min_x; }
    float height() const { return max_y - min_y; }
    float center_x() const { return (min_x + max_x) * 0.5f; }
    float center_y() const { return (min_y + max_y) * 0.5f; }
    Point2D<float> min() const { return {min_x, min_y}; }
    Point2D<float> max() const { return {max_x, max_y}; }
};

/**
     * @brief Check if one bounding box is completely contained within another
     * @param inner The potentially contained bounding box
     * @param outer The potentially containing bounding box
     * @return True if inner is completely contained within outer
     */
inline bool isCompletelyContained(BoundingBox const & inner, BoundingBox const & outer) {
    return (inner.min_x >= outer.min_x && inner.min_y >= outer.min_y &&
            inner.max_x <= outer.max_x && inner.max_y <= outer.max_y);
}

#endif // BOUNDINGBOX_HPP