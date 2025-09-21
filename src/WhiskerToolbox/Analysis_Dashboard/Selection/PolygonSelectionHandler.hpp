#ifndef POLYGONSELECTIONHANDLER_HPP
#define POLYGONSELECTIONHANDLER_HPP

#include "CoreGeometry/points.hpp"
#include "SelectionModes.hpp"
#include "ShaderManager/ShaderProgram.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QPoint>
#include <QVector2D>

#include <functional>
#include <memory>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QOpenGLShaderProgram;

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
    [[nodiscard]] std::vector<Point2D<float>> const & getVertices() const { return _polygon.getVertices(); }

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
    using NotificationCallback = std::function<void()>;

    explicit PolygonSelectionHandler();
    ~PolygonSelectionHandler();

    /**
     * @brief Set the notification callback to be called when selection is completed
     * @param callback The callback function to call when selection is completed
     */
    void setNotificationCallback(NotificationCallback callback);

    /**
     * @brief Clear the notification callback
     */
    void clearNotificationCallback();

    /**
     * @brief Render polygon selection overlay using OpenGL
     * @param mvp_matrix Model-View-Projection matrix
     */
    void render(QMatrix4x4 const & mvp_matrix);

    void deactivate();

    /**
     * @brief Get the current active selection region (if any)
     * @return Pointer to selection region, or nullptr if none active
     */
    [[nodiscard]] std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const { return _active_selection_region; }


    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos);

    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {}

    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) {}

    void keyPressEvent(QKeyEvent * event);

private:
    NotificationCallback _notification_callback;

    QOpenGLShaderProgram * _line_shader_program;

    // OpenGL rendering resources
    QOpenGLBuffer _polygon_vertex_buffer;
    QOpenGLVertexArrayObject _polygon_vertex_array_object;
    QOpenGLBuffer _polygon_line_buffer;
    QOpenGLVertexArrayObject _polygon_line_array_object;

    // Polygon selection state
    bool _is_polygon_selecting;
    std::vector<Point2D<float>> _polygon_vertices;            // Current polygon vertices in world coordinates
    std::unique_ptr<SelectionRegion> _active_selection_region;// Current selection region

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
     * @brief Update polygon vertex and line buffers
     */
    void updatePolygonBuffers();

    /**
     * @brief Add point to current polygon selection
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void addPolygonVertex(int world_x, int world_y);


    /**
     * @brief Check if currently in polygon selection mode
     * @return True if actively selecting a polygon
     */
    [[nodiscard]] bool isPolygonSelecting() const { return _is_polygon_selecting; }

    /**
     * @brief Get the number of vertices in the current polygon
     * @return Number of vertices
     */
    [[nodiscard]] size_t getVertexCount() const { return _polygon_vertices.size(); }

    /**
     * @brief Start polygon selection at given world coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void startPolygonSelection(int world_x, int world_y);


    /**
     * @brief Complete polygon selection and create selection region
     */
    void completePolygonSelection();

    /**
     * @brief Cancel current polygon selection
     */
    void cancelPolygonSelection();
};

#endif// POLYGONSELECTIONHANDLER_HPP
