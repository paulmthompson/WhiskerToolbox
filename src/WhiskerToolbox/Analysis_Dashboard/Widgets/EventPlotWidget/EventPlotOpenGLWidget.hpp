#ifndef EVENTPLOTOPENGLWIDGET_HPP
#define EVENTPLOTOPENGLWIDGET_HPP

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <memory>
#include <unordered_map>
#include <vector>

/**
 * @brief OpenGL widget for rendering event data with high performance
 */
class EventPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
    Q_OBJECT

public:
    explicit EventPlotOpenGLWidget(QWidget * parent = nullptr);
    ~EventPlotOpenGLWidget() override;

    /**
     * @brief Set zoom level (1.0 = default, >1.0 = zoomed in, <1.0 = zoomed out)
     * @param zoom_level The zoom level
     */
    void setZoomLevel(float zoom_level);

    /**
     * @brief Get current zoom level
     */
    float getZoomLevel() const { return _zoom_level; }

    /**
     * @brief Set pan offset
     * @param offset_x X offset in normalized coordinates
     * @param offset_y Y offset in normalized coordinates
     */
    void setPanOffset(float offset_x, float offset_y);

    /**
     * @brief Get current pan offset
     */
    QVector2D getPanOffset() const { return QVector2D(_pan_offset_x, _pan_offset_y); }

    /**
     * @brief Enable or disable tooltips
     * @param enabled Whether tooltips should be enabled
     */
    void setTooltipsEnabled(bool enabled);

    /**
     * @brief Get current tooltip enabled state
     */
    bool getTooltipsEnabled() const { return _tooltips_enabled; }

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return World coordinates
     */
    QVector2D screenToWorld(int screen_x, int screen_y) const;

signals:
    /**
     * @brief Emitted when user double-clicks on an event to jump to that frame
     * @param time_frame_index The time frame index to jump to
     * @param data_key The data key of the event
     */
    void frameJumpRequested(int64_t time_frame_index, QString const & data_key);

    /**
     * @brief Emitted when zoom level changes
     * @param zoom_level The new zoom level
     */
    void zoomLevelChanged(float zoom_level);

    /**
     * @brief Emitted when pan offset changes
     * @param offset_x The new X pan offset
     * @param offset_y The new Y pan offset
     */
    void panOffsetChanged(float offset_x, float offset_y);

    /**
     * @brief Emitted when tooltip enabled state changes
     * @param enabled Whether tooltips are enabled
     */
    void tooltipsEnabledChanged(bool enabled);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private:
    // OpenGL resources
    QOpenGLShaderProgram * _shader_program;
    QOpenGLBuffer _vertex_buffer;
    QOpenGLVertexArrayObject _vertex_array_object;

    // View transformation
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _projection_matrix;
    float _zoom_level;
    float _pan_offset_x;
    float _pan_offset_y;

    // Interaction state
    bool _mouse_pressed;
    QPoint _last_mouse_pos;
    bool _tooltips_enabled;

    // Widget dimensions
    int _widget_width;
    int _widget_height;

    /**
     * @brief Initialize OpenGL shaders
     */
    void initializeShaders();

    /**
     * @brief Initialize OpenGL buffers
     */
    void initializeBuffers();

    /**
     * @brief Update view and projection matrices
     */
    void updateMatrices();

    /**
     * @brief Handle mouse panning
     * @param delta_x Mouse movement in X direction
     * @param delta_y Mouse movement in Y direction
     */
    void handlePanning(int delta_x, int delta_y);

    /**
     * @brief Handle mouse zooming
     * @param delta_y Mouse wheel delta
     */
    void handleZooming(int delta_y);
};

#endif// EVENTPLOTOPENGLWIDGET_HPP