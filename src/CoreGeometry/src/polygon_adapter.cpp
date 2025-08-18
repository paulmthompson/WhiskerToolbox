#include "CoreGeometry/polygon_adapter.hpp"
#include <stdexcept>
#include <limits>

cbop::Point_2 PolygonAdapter::toMartinezPoint(const Point2D<float>& point) {
    return cbop::Point_2(static_cast<double>(point.x), static_cast<double>(point.y));
}

Point2D<float> PolygonAdapter::fromMartinezPoint(const cbop::Point_2& martinez_point) {
    return Point2D<float>(static_cast<float>(martinez_point.x()), static_cast<float>(martinez_point.y()));
}

cbop::Polygon PolygonAdapter::toMartinezPolygon(const Polygon& polygon) {
    cbop::Polygon martinez_polygon;
    
    if (!polygon.isValid()) {
        return martinez_polygon; // Return empty polygon
    }
    
    const auto& vertices = polygon.getVertices();
    if (vertices.size() < 3) {
        return martinez_polygon; // Return empty polygon
    }
    
    // Create the main contour
    cbop::Contour contour;
    for (const auto& vertex : vertices) {
        contour.add(toMartinezPoint(vertex));
    }
    
    // Add the contour to the polygon
    martinez_polygon.push_back(contour);
    
    return martinez_polygon;
}

Polygon PolygonAdapter::fromMartinezPolygon(const cbop::Polygon& martinez_polygon) {
    if (martinez_polygon.ncontours() == 0) {
        return Polygon(std::vector<Point2D<float>>{}); // Return empty polygon
    }
    
    if (martinez_polygon.ncontours() == 1) {
        // Simple case: single contour
        const cbop::Contour& main_contour = martinez_polygon.contour(0);
        
        std::vector<Point2D<float>> vertices;
        vertices.reserve(main_contour.nvertices());
        
        for (unsigned int i = 0; i < main_contour.nvertices(); ++i) {
            vertices.push_back(fromMartinezPoint(main_contour.vertex(i)));
        }
        
        return Polygon(vertices);
    } else {
        // Multiple contours case: create a bounding box that encompasses all contours
        // This is a reasonable approximation when we need to return a single polygon
        // but the actual result has multiple disjoint regions
        
        // Find the bounding box of all contours
        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        
        for (unsigned int c = 0; c < martinez_polygon.ncontours(); ++c) {
            const cbop::Contour& contour = martinez_polygon.contour(c);
            if (contour.external()) { // Only consider external contours, not holes
                for (unsigned int i = 0; i < contour.nvertices(); ++i) {
                    const cbop::Point_2& pt = contour.vertex(i);
                    min_x = std::min(min_x, pt.x());
                    min_y = std::min(min_y, pt.y());
                    max_x = std::max(max_x, pt.x());
                    max_y = std::max(max_y, pt.y());
                }
            }
        }
        
        // Create a rectangular polygon from the bounding box
        BoundingBox bbox(static_cast<float>(min_x), static_cast<float>(min_y), 
                        static_cast<float>(max_x), static_cast<float>(max_y));
        return Polygon(bbox);
    }
}

Polygon PolygonAdapter::performBooleanOperation(const Polygon& poly1, const Polygon& poly2, cbop::BooleanOpType operation) {
    if (!poly1.isValid() || !poly2.isValid()) {
        // Handle degenerate cases
        switch (operation) {
            case cbop::UNION:
                return poly1.isValid() ? poly1 : poly2;
            case cbop::INTERSECTION:
                return Polygon(std::vector<Point2D<float>>{}); // Empty polygon
            case cbop::DIFFERENCE:
                return poly1.isValid() ? poly1 : Polygon(std::vector<Point2D<float>>{});
            case cbop::XOR:
                return poly1.isValid() ? poly1 : poly2;
            default:
                return Polygon(std::vector<Point2D<float>>{});
        }
    }
    
    try {
        // Convert to Martinez-Rueda format
        cbop::Polygon martinez_poly1 = toMartinezPolygon(poly1);
        cbop::Polygon martinez_poly2 = toMartinezPolygon(poly2);
        cbop::Polygon result_polygon;
        
        // Perform the boolean operation
        cbop::compute(martinez_poly1, martinez_poly2, result_polygon, operation);
        
        // Convert back to our format
        return fromMartinezPolygon(result_polygon);
    }
    catch (const std::exception& e) {
        // If the operation fails, return an empty polygon
        // In a production system, you might want to log this error
        return Polygon(std::vector<Point2D<float>>{});
    }
}

Polygon PolygonAdapter::performUnion(const Polygon& poly1, const Polygon& poly2) {
    return performBooleanOperation(poly1, poly2, cbop::UNION);
}

Polygon PolygonAdapter::performIntersection(const Polygon& poly1, const Polygon& poly2) {
    return performBooleanOperation(poly1, poly2, cbop::INTERSECTION);
}

Polygon PolygonAdapter::performDifference(const Polygon& poly1, const Polygon& poly2) {
    return performBooleanOperation(poly1, poly2, cbop::DIFFERENCE);
}

Polygon PolygonAdapter::performXor(const Polygon& poly1, const Polygon& poly2) {
    return performBooleanOperation(poly1, poly2, cbop::XOR);
}
