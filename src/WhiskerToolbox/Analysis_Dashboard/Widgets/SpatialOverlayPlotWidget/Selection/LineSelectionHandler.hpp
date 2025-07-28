#ifndef LINESELECTIONHANDLER_HPP
#define LINESELECTIONHANDLER_HPP

#include "SelectionModes.hpp"
#include "CoreGeometry/lines.hpp"
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

enum class LineSelectionBehavior {
    Replace,
    Append,
    Remove
};

/**
 * @brief Line selection region for line-based selection
 */
class LineSelectionRegion : public SelectionRegion {
public:
    explicit LineSelectionRegion(Point2D<float> const & start_point, Point2D<float> const & end_point);

    bool containsPoint(Point2D<float> point) const override;
    void getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const override;

    /**
     * @brief Get the start point of the line
     */
    Point2D<float> const & getStartPoint() const { return _start_point; }

    /**
     * @brief Get the end point of the line
     */
    Point2D<float> const & getEndPoint() const { return _end_point; }

    LineSelectionBehavior getBehavior() const { return _behavior; }
    void setBehavior(LineSelectionBehavior behavior) { _behavior = behavior; }


private:
    Point2D<float> _start_point;
    Point2D<float> _end_point;
    LineSelectionBehavior _behavior = LineSelectionBehavior::Replace;
};

/**
 * @brief Handles line selection functionality for spatial overlay widgets
 * 
 * This class encapsulates all the logic and OpenGL resources needed for line selection,
 * including line drawing, rendering, and selection region creation.
 */
class LineSelectionHandler : protected QOpenGLFunctions_4_1_Core {
public:
    using NotificationCallback = std::function<void()>;

    explicit LineSelectionHandler();
    ~LineSelectionHandler();

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
     * @brief Render line selection overlay using OpenGL
     * @param mvp_matrix Model-View-Projection matrix
     */
    void render(QMatrix4x4 const & mvp_matrix);

    void deactivate();

    /**
     * @brief Get the current active selection region (if any)
     * @return Pointer to selection region, or nullptr if none active
     */
    std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const { return _active_selection_region; }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos);

    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos);

    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos);

    void keyPressEvent(QKeyEvent * event);

    /**
     * @brief Update line end point during drawing
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void updateLineEndPoint(float world_x, float world_y);

private:
    NotificationCallback _notification_callback;

    QOpenGLShaderProgram * _line_shader_program;

    // OpenGL rendering resources
    QOpenGLBuffer _line_vertex_buffer;
    QOpenGLVertexArrayObject _line_vertex_array_object;

    // Line selection state
    bool _is_drawing_line;
    Point2D<float> _line_start_point;  // Line start point in world coordinates
    Point2D<float> _line_end_point;    // Line end point in world coordinates
    std::unique_ptr<SelectionRegion> _active_selection_region; // Current selection region
    LineSelectionBehavior _current_behavior;

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
     * @brief Update line vertex buffer
     */
    void updateLineBuffer();

    /**
     * @brief Start line selection at given world coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     */
    void startLineSelection(float world_x, float world_y);



    /**
     * @brief Complete line selection and create selection region
     */
    void completeLineSelection();

    /**
     * @brief Cancel current line selection
     */
    void cancelLineSelection();
};

#endif// LINESELECTIONHANDLER_HPP 