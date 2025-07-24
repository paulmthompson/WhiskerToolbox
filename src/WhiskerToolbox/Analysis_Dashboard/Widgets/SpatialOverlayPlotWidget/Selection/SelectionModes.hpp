#ifndef SPATIALOVERLAY_SELECTION_MODES_HPP
#define SPATIALOVERLAY_SELECTION_MODES_HPP

#include "CoreGeometry/points.hpp"
#include "CoreGeometry/polygon.hpp"

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Selection modes for spatial selection
 */
enum class SelectionMode {
    None,            ///< No selection mode active
    PointSelection,  ///< Individual point selection (Ctrl+click)
    PolygonSelection,///< Polygon area selection (click and drag)
    LineIntersection ///< Line intersection selection (click and drag to create line)
};

/**
 * @brief Abstract base class for selection regions that can be applied to different data types
 */
class SelectionRegion {
public:
    virtual ~SelectionRegion() = default;

    /**
     * @brief Check if a 2D point is inside this selection region
     * @param point The point to check
     * @return True if point is inside the selection region
     */
    virtual bool containsPoint(::Point2D<float> point) const = 0;

    /**
     * @brief Get the bounding box of this selection region for optimization
     * @param min_x Output: minimum X coordinate
     * @param min_y Output: minimum Y coordinate  
     * @param max_x Output: maximum X coordinate
     * @param max_y Output: maximum Y coordinate
     */
    virtual void getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const = 0;
};

/**
 * @brief Polygon selection region for area-based selection
 */
class PolygonSelectionRegion : public SelectionRegion {
public:
    explicit PolygonSelectionRegion(std::vector<Point2D<float>> const & vertices);

    bool containsPoint(Point2D<float> point) const override;
    void getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const override;

    /**
     * @brief Get the polygon vertices in world coordinates
     */
    std::vector<Point2D<float>> const & getVertices() const { return _polygon.getVertices(); }

private:
    Polygon _polygon;
};

#endif// SPATIALOVERLAY_SELECTION_MODES_HPP
