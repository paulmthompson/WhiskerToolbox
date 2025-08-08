#ifndef SCATTERPLOTOPENGLWIDGET_HPP
#define SCATTERPLOTOPENGLWIDGET_HPP

#include <QMatrix4x4>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLWidget>
#include <QTimer>
#include <QRubberBand>
#include "Analysis_Dashboard/Widgets/Common/PlotInteractionController.hpp"

#include <memory>
#include <vector>


class ScatterPlotVisualization;
class GroupManager;
class ScatterPlotViewAdapter; // adapter (friend)

/**
 * @brief OpenGL widget for rendering scatter plot data with high performance
 */
class ScatterPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
    Q_OBJECT

public:
    explicit ScatterPlotOpenGLWidget(QWidget * parent = nullptr);
    ~ScatterPlotOpenGLWidget() override;

    /**
     * @brief Set the scatter plot data
     * @param x_data Vector of X coordinates
     * @param y_data Vector of Y coordinates
     */
    void setScatterData(std::vector<float> const & x_data, 
                       std::vector<float> const & y_data);

    /**
     * @brief Set axis labels for display
     * @param x_label Label for X axis
     * @param y_label Label for Y axis
     */
    void setAxisLabels(QString const & x_label, QString const & y_label);

    /**
     * @brief Set the group manager for color coding points
     * @param group_manager The group manager instance
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set the point size for rendering
     * @param point_size Point size in pixels
     */
    void setPointSize(float point_size);

    /**
     * @brief Get current point size
     */
    float getPointSize() const { return _point_size; }

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

signals:
    /**
     * @brief Emitted when a point is clicked
     * @param point_index The index of the clicked point
     */
    void pointClicked(size_t point_index);

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
     * @brief Emitted when the current world view bounds change (after zoom/pan/resize/box-zoom)
     */
    void viewBoundsChanged(float left, float right, float bottom, float top);

    /**
     * @brief Emitted when the mouse moves, reporting world coordinates under the cursor
     */
    void mouseWorldMoved(float world_x, float world_y);

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

private:
  // Grant adapter access to private state for interaction
  friend class ScatterPlotViewAdapter;
    // Point visualization
    std::unique_ptr<ScatterPlotVisualization> _scatter_visualization;
    GroupManager * _group_manager;
    float _point_size;

    // Data storage for deferred initialization
    std::vector<float> _x_data;
    std::vector<float> _y_data;

    // Data bounds for projection calculation
    float _data_min_x, _data_max_x, _data_min_y, _data_max_y;
    bool _data_bounds_valid;

    // OpenGL state
    bool _opengl_resources_initialized;

    // View transformation
    QMatrix4x4 _projection_matrix;
    float _zoom_level;
    float _zoom_level_x;
    float _zoom_level_y;
    float _pan_offset_x;
    float _pan_offset_y;
    float _padding_factor;

    // Mouse interaction
    bool _dragging;
    QPoint _last_mouse_pos;
    QPoint _current_mouse_pos;
    bool _tooltips_enabled;
    bool _is_panning;  // Track panning state

    // Tooltip system
    QTimer * _tooltip_timer;
    QPoint _tooltip_mouse_pos;
    static constexpr int TOOLTIP_DELAY_MS = 500;

    // FPS limiter timer (30 FPS = ~33ms interval)
    QTimer * _fps_limiter_timer;
    bool _pending_update;         // fps limiting

  // Interaction controller (composition)
  std::unique_ptr<PlotInteractionController> _interaction;

    // Box-zoom state
    bool _box_zoom_active = false;
    QRubberBand * _rubber_band = nullptr;
    QPoint _rubber_origin;

    /**
     * @brief Update the projection matrix based on current data bounds and zoom/pan
     */
    void updateProjectionMatrix();

    /**
     * @brief Throttled update method to limit FPS
     */
    void requestThrottledUpdate();

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_pos Screen position
     * @return World position
     */
    QVector2D screenToWorld(QPoint const & screen_pos) const;

    /**
     * @brief Handle mouse hover for tooltips
     * @param pos Mouse position
     */
    void handleMouseHover(QPoint const & pos);

    /**
     * @brief Calculate the current orthographic projection bounds based on data
     * @param left Output: left bound
     * @param right Output: right bound  
     * @param bottom Output: bottom bound
     * @param top Output: top bound
     */
    void calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const;

    /**
     * @brief Calculate data bounds from the stored data
     */
  void calculateDataBounds();
};

#endif// SCATTERPLOTOPENGLWIDGET_HPP
