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

/**
 * @brief Handles polygon selection functionality for spatial overlay widgets
 * 
 * This class encapsulates all the logic and OpenGL resources needed for polygon selection,
 * including vertex management, rendering, and selection region creation.
 */
class PolygonSelectionHandler : protected QOpenGLFunctions_4_1_Core {
public:
    // Callback function types
    using RequestUpdateCallback = std::function<void()>;
    using ScreenToWorldCallback = std::function<QVector2D(int, int)>;
    using ApplySelectionRegionCallback = std::function<void(SelectionRegion const &, bool)>;

    explicit PolygonSelectionHandler(
            RequestUpdateCallback request_update_callback = nullptr,
            ScreenToWorldCallback screen_to_world_callback = nullptr,
            ApplySelectionRegionCallback apply_selection_region_callback = nullptr);
    ~PolygonSelectionHandler();

    /**
     * @brief Set the callbacks for communicating with the parent widget
     * @param request_update_callback Callback for requesting display updates
     * @param screen_to_world_callback Callback for screen to world coordinate conversion
     * @param apply_selection_region_callback Callback for applying selection regions
     */
    void setCallbacks(
            RequestUpdateCallback request_update_callback,
            ScreenToWorldCallback screen_to_world_callback,
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
     * @brief Start polygon selection at given screen coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     */
    void startPolygonSelection(int screen_x, int screen_y);

    /**
     * @brief Add point to current polygon selection
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     */
    void addPolygonVertex(int screen_x, int screen_y);

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

private:
    // Callback functions
    RequestUpdateCallback _request_update_callback;
    ScreenToWorldCallback _screen_to_world_callback;
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
    std::vector<QPoint> _polygon_screen_points;               // Polygon vertices in screen coordinates for rendering
    std::unique_ptr<SelectionRegion> _active_selection_region;// Current selection region

    /**
     * @brief Update polygon vertex and line buffers
     */
    void updatePolygonBuffers();
};

#endif// POLYGONSELECTIONHANDLER_HPP
