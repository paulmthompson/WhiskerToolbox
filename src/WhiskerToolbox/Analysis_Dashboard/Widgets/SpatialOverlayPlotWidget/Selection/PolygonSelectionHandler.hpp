#ifndef POLYGONSELECTIONHANDLER_HPP
#define POLYGONSELECTIONHANDLER_HPP

#include "SelectionModes.hpp"
#include "CoreGeometry/points.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPoint>
#include <QVector2D>

#include <functional>
#include <memory>
#include <vector>

class QMouseEvent;

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

/**
 * @brief Handles polygon selection functionality for spatial overlay widgets
 * 
 * This class encapsulates all the logic and OpenGL resources needed for polygon selection,
 * including vertex management, rendering, and selection region creation.
 */
class PolygonSelectionHandler : protected QOpenGLFunctions_4_1_Core {
public:
    using ApplySelectionRegionCallback = std::function<void(SelectionRegion const &, bool)>;

    explicit PolygonSelectionHandler(
            ApplySelectionRegionCallback apply_selection_region_callback = nullptr);
    ~PolygonSelectionHandler();

    /**
     * @brief Set the callbacks for communicating with the parent widget
     * @param apply_selection_region_callback Callback for applying selection regions
     */
    void setCallbacks(
            ApplySelectionRegionCallback apply_selection_region_callback);

    /**
     * @brief Initialize OpenGL resources
     * Must be called from an OpenGL context
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources
     * Must be called from an OpenGL context
     */
    void cleanupOpenGLResources();

    /**
     * @brief Check if currently in polygon selection mode
     * @return True if actively selecting a polygon
     */
    bool isPolygonSelecting() const { return _is_polygon_selecting; }

    /**
     * @brief Get the number of vertices in the current polygon
     * @return Number of vertices
     */
    size_t getVertexCount() const { return _polygon_vertices.size(); }

    /**
     * @brief Start polygon selection at given world coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void startPolygonSelection(int world_x, int world_y);

    /**
     * @brief Add point to current polygon selection
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void addPolygonVertex(int world_x, int world_y);

    /**
     * @brief Complete polygon selection and create selection region
     */
    void completePolygonSelection();

    /**
     * @brief Cancel current polygon selection
     */
    void cancelPolygonSelection();

    /**
     * @brief Render polygon selection overlay using OpenGL
     * @param line_shader_program Shader program for line rendering
     * @param mvp_matrix Model-View-Projection matrix
     */
    void renderPolygonOverlay(QOpenGLShaderProgram * line_shader_program, QMatrix4x4 const & mvp_matrix);

    /**
     * @brief Get the current active selection region (if any)
     * @return Pointer to selection region, or nullptr if none active
     */
    std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const { return _active_selection_region; }


    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos); 
private:
  
    ApplySelectionRegionCallback _apply_selection_region_callback;

    // OpenGL rendering resources
    QOpenGLBuffer _polygon_vertex_buffer;
    QOpenGLVertexArrayObject _polygon_vertex_array_object;
    QOpenGLBuffer _polygon_line_buffer;
    QOpenGLVertexArrayObject _polygon_line_array_object;
    bool _opengl_resources_initialized;

    // Polygon selection state
    bool _is_polygon_selecting;
    std::vector<Point2D<float>> _polygon_vertices;                 // Current polygon vertices in world coordinates
    std::unique_ptr<SelectionRegion> _active_selection_region;// Current selection region

    /**
     * @brief Update polygon vertex and line buffers
     */
    void updatePolygonBuffers();
};

#endif// POLYGONSELECTIONHANDLER_HPP
