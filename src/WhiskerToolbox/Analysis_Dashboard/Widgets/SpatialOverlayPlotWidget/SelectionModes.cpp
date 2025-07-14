#include "SelectionModes.hpp"

// PolygonSelectionRegion implementation

PolygonSelectionRegion::PolygonSelectionRegion(std::vector<Point2D<float>> const & vertices)
    : _polygon(vertices) {
}

bool PolygonSelectionRegion::containsPoint(Point2D<float> point) const {
    return _polygon.containsPoint(point);
}

void PolygonSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    auto bbox = _polygon.getBoundingBox();
    min_x = bbox.min_x;
    min_y = bbox.min_y;
    max_x = bbox.max_x;
    max_y = bbox.max_y;
}
