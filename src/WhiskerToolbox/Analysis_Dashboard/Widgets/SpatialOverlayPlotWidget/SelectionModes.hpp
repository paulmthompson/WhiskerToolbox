#ifndef SPATIALOVERLAY_SELECTION_MODES_HPP
#define SPATIALOVERLAY_SELECTION_MODES_HPP

#include <QString>
#include <QVector2D>

#include <cstdint>
#include <vector>

/**
 * @brief Selection modes for spatial selection
 */
enum class SelectionMode {
    None,           ///< No selection mode active
    PointSelection, ///< Individual point selection (Ctrl+click)
    PolygonSelection ///< Polygon area selection (click and drag)
};

/**
 * @brief Abstract base class for selection regions that can be applied to different data types
 */
class SelectionRegion {
public:
    virtual ~SelectionRegion() = default;
    
    /**
     * @brief Check if a 2D point is inside this selection region
     * @param x X coordinate in world space
     * @param y Y coordinate in world space
     * @return True if point is inside the selection region
     */
    virtual bool containsPoint(float x, float y) const = 0;
    
    /**
     * @brief Get the bounding box of this selection region for optimization
     * @param min_x Output: minimum X coordinate
     * @param min_y Output: minimum Y coordinate  
     * @param max_x Output: maximum X coordinate
     * @param max_y Output: maximum Y coordinate
     */
    virtual void getBoundingBox(float& min_x, float& min_y, float& max_x, float& max_y) const = 0;
};

/**
 * @brief Polygon selection region for area-based selection
 */
class PolygonSelectionRegion : public SelectionRegion {
public:
    explicit PolygonSelectionRegion(std::vector<QVector2D> const& vertices);
    
    bool containsPoint(float x, float y) const override;
    void getBoundingBox(float& min_x, float& min_y, float& max_x, float& max_y) const override;
    
    /**
     * @brief Get the polygon vertices in world coordinates
     */
    std::vector<QVector2D> const& getVertices() const { return _vertices; }
    
private:
    std::vector<QVector2D> _vertices;
    float _min_x, _min_y, _max_x, _max_y; // Cached bounding box
};

/**
 * @brief Data structure for storing point information with time frame
 */
struct SpatialPointData {
    float x, y;
    int64_t time_frame_index;
    QString point_data_key;

    SpatialPointData(float x, float y, int64_t time_frame_index, QString const & key)
        : x(x),
          y(y),
          time_frame_index(time_frame_index),
          point_data_key(key) {}
};


#endif // SPATIALOVERLAY_SELECTION_MODES_HPP
