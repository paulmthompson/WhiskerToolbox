#ifndef SCATTERPLOTOPENGLWIDGET_HPP
#define SCATTERPLOTOPENGLWIDGET_HPP

#include <QMatrix4x4>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLWidget>
#include <QTimer>

#include <memory>
#include <vector>


class ScatterPlotVisualization;
class GroupManager;

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
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _projection_matrix;
    float _zoom_level;
    float _pan_offset_x;
    float _pan_offset_y;

    // Mouse interaction
    bool _dragging;
    QPoint _last_mouse_pos;
    QPoint _current_mouse_pos;
    bool _tooltips_enabled;

    // Tooltip system
    QTimer * _tooltip_timer;
    QPoint _tooltip_mouse_pos;
    static constexpr int TOOLTIP_DELAY_MS = 500;

    /**
     * @brief Update the view matrix based on current zoom and pan
     */
    void updateViewMatrix();

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

    /**
     * @brief Update the projection matrix based on current data bounds and zoom/pan
     */
    void updateProjectionMatrix();
};

#endif// SCATTERPLOTOPENGLWIDGET_HPP
