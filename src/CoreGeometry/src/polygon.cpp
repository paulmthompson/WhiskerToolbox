#include "CoreGeometry/polygon.hpp"
#include "CoreGeometry/polygon_adapter.hpp"

#include <algorithm>
#include <limits>
#include <cmath>

Polygon::Polygon(std::vector<Point2D<float>> const & vertices)
    : _vertices(vertices), _bounding_box(0.0f, 0.0f, 0.0f, 0.0f) {
    
    if (_vertices.empty()) {
        return;
    }

    // Calculate bounding box
    float min_x = _vertices[0].x;
    float max_x = _vertices[0].x;
    float min_y = _vertices[0].y;
    float max_y = _vertices[0].y;

    for (auto const & vertex : _vertices) {
        min_x = std::min(min_x, vertex.x);
        max_x = std::max(max_x, vertex.x);
        min_y = std::min(min_y, vertex.y);
        max_y = std::max(max_y, vertex.y);
    }

    _bounding_box = BoundingBox(min_x, min_y, max_x, max_y);
}

Polygon::Polygon(BoundingBox const & bbox)
    : _bounding_box(bbox) {
    
    // Create rectangular polygon from bounding box
    // Vertices ordered counter-clockwise starting from bottom-left
    _vertices = {
        {bbox.min_x, bbox.min_y}, // Bottom-left
        {bbox.max_x, bbox.min_y}, // Bottom-right
        {bbox.max_x, bbox.max_y}, // Top-right
        {bbox.min_x, bbox.max_y}  // Top-left
    };
}

bool Polygon::containsPoint(Point2D<float> const & point) const {
    if (!isValid()) {
        return false;
    }

    // Quick bounding box check first for optimization
    if (!_bounding_box.contains(point.x, point.y)) {
        return false;
    }

    // Ray casting algorithm for point-in-polygon test
    bool inside = false;
    size_t j = _vertices.size() - 1;

    for (size_t i = 0; i < _vertices.size(); ++i) {
        float xi = _vertices[i].x;
        float yi = _vertices[i].y;
        float xj = _vertices[j].x;
        float yj = _vertices[j].y;

        // Check if point is on a horizontal ray from the test point
        if (((yi > point.y) != (yj > point.y)) &&
            (point.x < (xj - xi) * (point.y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
        j = i;
    }

    return inside;
}

BoundingBox Polygon::getBoundingBox() const {
    return _bounding_box;
}

// Helper function to check if a point is on the inside side of a line segment
static bool isPointInsideLine(Point2D<float> const & point, Point2D<float> const & line_start, Point2D<float> const & line_end) {
    // Use cross product to determine which side of the line the point is on
    float cross_product = (line_end.x - line_start.x) * (point.y - line_start.y) - 
                         (line_end.y - line_start.y) * (point.x - line_start.x);
    return cross_product >= 0; // Point is on the left side or on the line
}

// Helper function to compute intersection point of two line segments
static Point2D<float> computeLineIntersection(Point2D<float> const & p1, Point2D<float> const & p2,
                                             Point2D<float> const & p3, Point2D<float> const & p4) {
    float denom = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    
    if (std::abs(denom) < 1e-10f) {
        // Lines are parallel, return midpoint
        return {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    }
    
    float t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / denom;
    return {p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y)};
}

// Sutherland-Hodgman polygon clipping algorithm
static std::vector<Point2D<float>> clipPolygonByEdge(std::vector<Point2D<float>> const & subject_polygon,
                                                     Point2D<float> const & clip_edge_start,
                                                     Point2D<float> const & clip_edge_end) {
    if (subject_polygon.empty()) {
        return {};
    }
    
    std::vector<Point2D<float>> output_list;
    
    Point2D<float> s = subject_polygon.back(); // Last vertex
    
    for (auto const & e : subject_polygon) {
        if (isPointInsideLine(e, clip_edge_start, clip_edge_end)) {
            if (!isPointInsideLine(s, clip_edge_start, clip_edge_end)) {
                // Entering: add intersection point
                output_list.push_back(computeLineIntersection(s, e, clip_edge_start, clip_edge_end));
            }
            // Add the vertex
            output_list.push_back(e);
        } else if (isPointInsideLine(s, clip_edge_start, clip_edge_end)) {
            // Leaving: add intersection point
            output_list.push_back(computeLineIntersection(s, e, clip_edge_start, clip_edge_end));
        }
        s = e;
    }
    
    return output_list;
}
bool Polygon::intersects(Polygon const & other) const {
    if (!isValid() || !other.isValid()) {
        return false;
    }
    
    // Quick bounding box check
    if (!_bounding_box.intersects(other._bounding_box)) {
        return false;
    }
    
    // Check if any vertex of one polygon is inside the other
    for (auto const & vertex : _vertices) {
        if (other.containsPoint(vertex)) {
            return true;
        }
    }
    
    for (auto const & vertex : other._vertices) {
        if (containsPoint(vertex)) {
            return true;
        }
    }
    
    // Check for edge intersections using line segment intersection
    for (size_t i = 0; i < _vertices.size(); ++i) {
        size_t next_i = (i + 1) % _vertices.size();
        Point2D<float> edge1_start = _vertices[i];
        Point2D<float> edge1_end = _vertices[next_i];
        
        for (size_t j = 0; j < other._vertices.size(); ++j) {
            size_t next_j = (j + 1) % other._vertices.size();
            Point2D<float> edge2_start = other._vertices[j];
            Point2D<float> edge2_end = other._vertices[next_j];
            
            // Check if line segments intersect
            float d1 = (edge2_end.x - edge2_start.x) * (edge1_start.y - edge2_start.y) - 
                      (edge2_end.y - edge2_start.y) * (edge1_start.x - edge2_start.x);
            float d2 = (edge2_end.x - edge2_start.x) * (edge1_end.y - edge2_start.y) - 
                      (edge2_end.y - edge2_start.y) * (edge1_end.x - edge2_start.x);
            float d3 = (edge1_end.x - edge1_start.x) * (edge2_start.y - edge1_start.y) - 
                      (edge1_end.y - edge1_start.y) * (edge2_start.x - edge1_start.x);
            float d4 = (edge1_end.x - edge1_start.x) * (edge2_end.y - edge1_start.y) - 
                      (edge1_end.y - edge1_start.y) * (edge2_end.x - edge1_start.x);
            
            if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && 
                ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
                return true; // Segments intersect
            }
        }
    }
    
    return false;
}

Polygon Polygon::unionWith(Polygon const & other) const {
    return PolygonAdapter::performUnion(*this, other);
}

Polygon Polygon::intersectionWith(Polygon const & other) const {
    return PolygonAdapter::performIntersection(*this, other);
}

Polygon Polygon::differenceWith(Polygon const & other) const {
    return PolygonAdapter::performDifference(*this, other);
}
