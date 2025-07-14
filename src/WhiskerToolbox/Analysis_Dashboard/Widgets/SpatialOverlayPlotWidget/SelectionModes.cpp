#include "SelectionModes.hpp"

// PolygonSelectionRegion implementation

PolygonSelectionRegion::PolygonSelectionRegion(std::vector<QVector2D> const & vertices)
    : _vertices(vertices) {

    if (_vertices.empty()) {
        _min_x = _min_y = _max_x = _max_y = 0.0f;
        return;
    }

    // Calculate bounding box
    _min_x = _max_x = _vertices[0].x();
    _min_y = _max_y = _vertices[0].y();

    for (auto const & vertex: _vertices) {
        _min_x = std::min(_min_x, vertex.x());
        _max_x = std::max(_max_x, vertex.x());
        _min_y = std::min(_min_y, vertex.y());
        _max_y = std::max(_max_y, vertex.y());
    }
}

bool PolygonSelectionRegion::containsPoint(Point2D<float> point) const {
    if (_vertices.size() < 3) return false;

    // Quick bounding box check first
    if (point.x < _min_x || point.x > _max_x || point.y < _min_y || point.y > _max_y) {
        return false;
    }

    // Ray casting algorithm for point-in-polygon test
    bool inside = false;
    size_t j = _vertices.size() - 1;

    for (size_t i = 0; i < _vertices.size(); ++i) {
        float xi = _vertices[i].x();
        float yi = _vertices[i].y();
        float xj = _vertices[j].x();
        float yj = _vertices[j].y();

        if (((yi > point.y) != (yj > point.y)) &&
            (point.x < (xj - xi) * (point.y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
        j = i;
    }

    return inside;
}

void PolygonSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    min_x = _min_x;
    min_y = _min_y;
    max_x = _max_x;
    max_y = _max_y;
}
