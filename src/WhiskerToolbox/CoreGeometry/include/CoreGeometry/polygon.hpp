#ifndef COREGEOMETRY_POLYGON_HPP
#define COREGEOMETRY_POLYGON_HPP

#include "boundingbox.hpp"
#include "points.hpp"

#include <vector>

/**
 * @brief A polygon defined by a list of 2D vertices
 * 
 * Provides functionality for:
 * - Point-in-polygon testing using ray casting algorithm
 * - Bounding box calculation
 * - Vertex access
 */
class Polygon {
public:
    /**
     * @brief Construct a polygon from a list of vertices
     * @param vertices The vertices defining the polygon boundary
     */
    explicit Polygon(std::vector<Point2D<float>> const & vertices);

    /**
     * @brief Construct a rectangular polygon from a bounding box
     * @param bbox The bounding box to convert to a polygon
     * 
     * Creates a rectangular polygon with vertices at the four corners of the bounding box.
     * Vertices are ordered counter-clockwise starting from bottom-left.
     */
    explicit Polygon(BoundingBox const & bbox);

    /**
     * @brief Check if a point is inside this polygon
     * @param point The point to test
     * @return True if the point is inside the polygon, false otherwise
     * 
     * Uses ray casting algorithm for point-in-polygon test.
     * Returns false for polygons with fewer than 3 vertices.
     */
    bool containsPoint(Point2D<float> const & point) const;

    /**
     * @brief Get the bounding box that encloses this polygon
     * @return BoundingBox that tightly bounds all vertices
     */
    BoundingBox getBoundingBox() const;

    /**
     * @brief Get the polygon vertices
     * @return Const reference to the vector of vertices
     */
    std::vector<Point2D<float>> const & getVertices() const { return _vertices; }

    /**
     * @brief Check if this polygon is valid for geometric operations
     * @return True if polygon has at least 3 vertices
     */
    bool isValid() const { return _vertices.size() >= 3; }

    /**
     * @brief Get the number of vertices in the polygon
     * @return Number of vertices
     */
    size_t vertexCount() const { return _vertices.size(); }

    /**
     * @brief Compute the union of this polygon with another polygon
     * @param other The other polygon to union with
     * @return A new polygon representing the union, or empty polygon if operation fails
     * 
     * Note: This is a simplified implementation that works by computing the bounding box
     * union for most cases. For complex polygon unions, consider using a dedicated
     * computational geometry library like CGAL or Clipper.
     */
    Polygon unionWith(Polygon const & other) const;

    /**
     * @brief Compute the intersection of this polygon with another polygon
     * @param other The other polygon to intersect with
     * @return A new polygon representing the intersection, or empty polygon if no intersection
     * 
     * Uses the Sutherland-Hodgman clipping algorithm for polygon intersection.
     * Works well for convex polygons and many simple polygon cases.
     */
    Polygon intersectionWith(Polygon const & other) const;

    /**
     * @brief Check if this polygon intersects with another polygon
     * @param other The other polygon to check intersection with
     * @return True if the polygons intersect (including touching), false otherwise
     */
    bool intersects(Polygon const & other) const;

private:
    std::vector<Point2D<float>> _vertices;
    BoundingBox _bounding_box;
};

#endif // COREGEOMETRY_POLYGON_HPP
