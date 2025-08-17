#ifndef COREGEOMETRY_POLYGON_ADAPTER_HPP
#define COREGEOMETRY_POLYGON_ADAPTER_HPP

#include "polygon.hpp"
#include "CoreGeometry/bop12/polygon.h"
#include "CoreGeometry/bop12/booleanop.h"

/**
 * @brief Adapter class to interface between our Polygon class and the Martinez-Rueda library
 * 
 * This adapter converts between our Point2D<float>/Polygon representation and the 
 * cbop::Point_2/cbop::Polygon representation used by the Martinez-Rueda clipping library.
 */
class PolygonAdapter {
public:
    /**
     * @brief Convert our Polygon to Martinez-Rueda cbop::Polygon
     * @param polygon The polygon to convert
     * @return Martinez-Rueda polygon representation
     */
    static cbop::Polygon toMartinezPolygon(const Polygon& polygon);
    
    /**
     * @brief Convert Martinez-Rueda cbop::Polygon to our Polygon
     * @param martinez_polygon The Martinez-Rueda polygon to convert
     * @return Our polygon representation
     */
    static Polygon fromMartinezPolygon(const cbop::Polygon& martinez_polygon);
    
    /**
     * @brief Convert our Point2D<float> to Martinez-Rueda cbop::Point_2
     * @param point The point to convert
     * @return Martinez-Rueda point representation
     */
    static cbop::Point_2 toMartinezPoint(const Point2D<float>& point);
    
    /**
     * @brief Convert Martinez-Rueda cbop::Point_2 to our Point2D<float>
     * @param martinez_point The Martinez-Rueda point to convert
     * @return Our point representation
     */
    static Point2D<float> fromMartinezPoint(const cbop::Point_2& martinez_point);
    
    /**
     * @brief Perform polygon union using Martinez-Rueda algorithm
     * @param poly1 First polygon
     * @param poly2 Second polygon
     * @return Union of the two polygons
     */
    static Polygon performUnion(const Polygon& poly1, const Polygon& poly2);
    
    /**
     * @brief Perform polygon intersection using Martinez-Rueda algorithm
     * @param poly1 First polygon
     * @param poly2 Second polygon
     * @return Intersection of the two polygons
     */
    static Polygon performIntersection(const Polygon& poly1, const Polygon& poly2);
    
    /**
     * @brief Perform polygon difference using Martinez-Rueda algorithm
     * @param poly1 First polygon (minuend)
     * @param poly2 Second polygon (subtrahend)
     * @return Difference of the two polygons (poly1 - poly2)
     */
    static Polygon performDifference(const Polygon& poly1, const Polygon& poly2);
    
    /**
     * @brief Perform polygon XOR using Martinez-Rueda algorithm
     * @param poly1 First polygon
     * @param poly2 Second polygon
     * @return XOR of the two polygons
     */
    static Polygon performXor(const Polygon& poly1, const Polygon& poly2);

private:
    /**
     * @brief Helper to perform boolean operation
     * @param poly1 First polygon
     * @param poly2 Second polygon
     * @param operation The boolean operation to perform
     * @return Result polygon
     */
    static Polygon performBooleanOperation(const Polygon& poly1, const Polygon& poly2, cbop::BooleanOpType operation);
};

#endif // COREGEOMETRY_POLYGON_ADAPTER_HPP
