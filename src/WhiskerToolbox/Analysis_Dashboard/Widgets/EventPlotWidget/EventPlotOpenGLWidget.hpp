#ifndef EVENTPLOTOPENGLWIDGET_HPP
#define EVENTPLOTOPENGLWIDGET_HPP

#include "SpatialIndex/QuadTree.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "ShaderManager/ShaderManager.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>
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
     * @brief Set Y-axis zoom level (1.0 = default, >1.0 = zoomed in, <1.0 = zoomed out)
     * @param y_zoom_level The Y-axis zoom level
     */
    void setYZoomLevel(float y_zoom_level);

    /**
     * @brief Get current Y-axis zoom level
     */
    float getYZoomLevel() const { return _y_zoom_level; }

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
     * @brief Set the X-axis range for the plot
     * @param negative_range Negative range in milliseconds (from -negative_range to -1)
     * @param positive_range Positive range in milliseconds (from 1 to positive_range)
     */
    void setXAxisRange(int negative_range, int positive_range);

    /**
     * @brief Get the current X-axis range
     * @param negative_range Output parameter for negative range
     * @param positive_range Output parameter for positive range
     */
    void getXAxisRange(int & negative_range, int & positive_range) const;

    /**
     * @brief Get the current visible bounds (including pan offset)
     * @param left_bound Output parameter for left visible bound
     * @param right_bound Output parameter for right visible bound
     */
    void getVisibleBounds(float & left_bound, float & right_bound) const;

    /**
     * @brief Set the event data to display
     * @param event_data Vector of trials, each containing vector of event times
     */
    void setEventData(std::vector<std::vector<float>> const & event_data);

    enum class PlotTheme {
        Dark,
        Light
    };
    void setPlotTheme(PlotTheme theme) { _plot_theme = theme; update(); }
    PlotTheme getPlotTheme() const { return _plot_theme; }

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
    void leaveEvent(QEvent * event) override;

private slots:
    /**
     * @brief Handle tooltip timer timeout
     */
    void handleTooltipTimer();

    /**
     * @brief Handle tooltip refresh timer timeout
     */
    void handleTooltipRefresh();


private:
    // OpenGL resources
    QOpenGLShaderProgram * _shader_program;
    QOpenGLBuffer _vertex_buffer;
    QOpenGLVertexArrayObject _vertex_array_object;
    QOpenGLBuffer _highlight_vertex_buffer;
    QOpenGLVertexArrayObject _highlight_vertex_array_object;

    // View transformation
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _projection_matrix;
    float _zoom_level;      // Overall zoom level (kept for compatibility)
    float _y_zoom_level;    // Y-axis zoom level (for trial spacing)
    float _pan_offset_x;
    float _pan_offset_y;

    // Interaction state
    bool _mouse_pressed;
    QPoint _last_mouse_pos;
    bool _tooltips_enabled;
    QTimer * _tooltip_timer;

    // Widget dimensions
    int _widget_width;
    int _widget_height;

    // X-axis range settings
    int _negative_range;
    int _positive_range;

    bool _data_bounds_valid;
    bool _opengl_resources_initialized;

    // Event data
    std::vector<std::vector<float>> _event_data;
    std::vector<float> _vertex_data;
    size_t _total_events;

    // Hover state - improved version
    struct HoveredEvent {
        int trial_index;
        int event_index;
        float x;
        float y;
    };
    std::optional<HoveredEvent> _hovered_event;

    // Hover processing timers (similar to SpatialOverlayOpenGLWidget)
    QTimer * _hover_debounce_timer;
    QTimer * _tooltip_refresh_timer;
    bool _hover_processing_active;
    QPoint _pending_hover_pos;

    PlotTheme _plot_theme = PlotTheme::Dark;

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
     * @brief Calculate projection bounds for coordinate transformation
     * @param left Output: left bound
     * @param right Output: right bound  
     * @param bottom Output: bottom bound
     * @param top Output: top bound
     */
    void calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const;

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

    /**
     * @brief Update vertex data from event data
     */
    void updateVertexData();

    /**
     * @brief Process hover detection with debouncing (improved version)
     */
    void processHoverDebounce();

    /**
     * @brief Find event near screen coordinates (legacy method)
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param tolerance_pixels Tolerance in pixels
     * @return Optional hovered event
     */
    std::optional<HoveredEvent> findEventNear(int screen_x, int screen_y, float tolerance_pixels = 10.0f);

    /**
     * @brief Calculate world tolerance from screen tolerance
     * @param screen_tolerance Tolerance in screen pixels
     * @return World tolerance
     */
    float calculateWorldTolerance(float screen_tolerance) const;

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param world_x Output world X coordinate
     * @param world_y Output world Y coordinate
     */
    void screenToWorld(int screen_x, int screen_y, float& world_x, float& world_y);

    /**
     * @brief Render all events using OpenGL
     */
    void renderEvents();

    /**
     * @brief Render hovered event with larger size
     */
    void renderHoveredEvent();

    /**
     * @brief Render the center line at t=0
     */
    void renderCenterLine();
    
    

};

#endif// EVENTPLOTOPENGLWIDGET_HPP