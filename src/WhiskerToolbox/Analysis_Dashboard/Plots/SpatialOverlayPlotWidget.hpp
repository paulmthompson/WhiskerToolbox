#ifndef SPATIALOVERLAYPLOTWIDGET_HPP
#define SPATIALOVERLAYPLOTWIDGET_HPP

#include "AbstractPlotWidget.hpp"
#include "SpatialIndex/QuadTree.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QMatrix4x4>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QTimer>
#include <QToolTip>
#include <QVector3D>

#include <memory>
#include <unordered_map>

class PointData;
class QGraphicsProxyWidget;
class TimeFrameIndex;

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

/**
 * @brief OpenGL widget for rendering spatial data with high performance
 */
class SpatialOverlayOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
    Q_OBJECT

public:
    explicit SpatialOverlayOpenGLWidget(QWidget * parent = nullptr);
    ~SpatialOverlayOpenGLWidget() override;

    /**
     * @brief Set the point data to display
     * @param point_data_map Map of data key to PointData objects
     */
    void setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map);

    /**
     * @brief Set zoom level (1.0 = default, >1.0 = zoomed in, <1.0 = zoomed out)
     * @param zoom_level The zoom level
     */
    void setZoomLevel(float zoom_level);

    /**
     * @brief Set pan offset
     * @param offset_x X offset in normalized coordinates
     * @param offset_y Y offset in normalized coordinates
     */
    void setPanOffset(float offset_x, float offset_y);

    /**
     * @brief Set the point size for rendering
     * @param point_size Point size in pixels
     */
    void setPointSize(float point_size);

    /**
     * @brief Get current zoom level
     */
    float getZoomLevel() const { return _zoom_level; }

    /**
     * @brief Get current pan offset
     */
    QVector2D getPanOffset() const { return QVector2D(_pan_offset_x, _pan_offset_y); }

    /**
     * @brief Get current point size
     */
    float getPointSize() const { return _point_size; }

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
     * @brief Emitted when user double-clicks on a point to jump to that frame
     * @param time_frame_index The time frame index to jump to
     */
    void frameJumpRequested(int64_t time_frame_index);

    /**
     * @brief Emitted when point size changes
     * @param point_size The new point size in pixels
     */
    void pointSizeChanged(float point_size);

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
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void leaveEvent(QEvent * event) override;

private slots:
    /**
     * @brief Handle tooltip timer timeout
     */
    void handleTooltipTimer();

private:
    // Rendering data
    std::vector<SpatialPointData> _all_points;
    std::unique_ptr<QuadTree<size_t>> _spatial_index;// Index into _all_points

    // Modern OpenGL rendering resources
    QOpenGLShaderProgram * _shader_program;
    QOpenGLBuffer _vertex_buffer;
    QOpenGLVertexArrayObject _vertex_array_object;
    bool _opengl_resources_initialized;

    // View parameters
    float _zoom_level;
    float _pan_offset_x, _pan_offset_y;
    float _point_size;
    QMatrix4x4 _projection_matrix;
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _model_matrix;

    // Interaction state
    bool _is_panning;
    QPoint _last_mouse_pos;
    QPoint _current_mouse_pos;
    QTimer * _tooltip_timer;
    bool _tooltips_enabled;
    SpatialPointData const * _current_hover_point;

    // Data bounds
    float _data_min_x, _data_max_x, _data_min_y, _data_max_y;
    bool _data_bounds_valid;

    /**
     * @brief Update the spatial index with current point data
     */
    void updateSpatialIndex();

    /**
     * @brief Calculate data bounds from all points
     */
    void calculateDataBounds();

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return World coordinates
     */
    QVector2D screenToWorld(int screen_x, int screen_y) const;

    /**
     * @brief Convert world coordinates to screen coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Screen coordinates
     */
    QPoint worldToScreen(float world_x, float world_y) const;

    /**
     * @brief Find point near screen coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param tolerance_pixels Tolerance in pixels
     * @return Pointer to point data, or nullptr if none found
     */
    SpatialPointData const * findPointNear(int screen_x, int screen_y, float tolerance_pixels = 10.0f) const;

    /**
     * @brief Update view matrices based on current zoom and pan
     */
    void updateViewMatrices();

    /**
     * @brief Render all points using OpenGL
     */
    void renderPoints();

    /**
     * @brief Setup OpenGL for point rendering
     */
    void setupPointRendering();

    /**
     * @brief Initialize OpenGL shaders and resources
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources
     */
    void cleanupOpenGLResources();

    /**
     * @brief Update vertex buffer with current point data
     */
    void updateVertexBuffer();
};

/**
 * @brief Spatial overlay plot widget for visualizing PointData across all time frames
 * 
 * This widget displays all points from selected PointData objects overlaid in a single
 * spatial view, with efficient rendering using OpenGL and spatial indexing for interactions.
 */
class SpatialOverlayPlotWidget : public AbstractPlotWidget {
    Q_OBJECT

public:
    explicit SpatialOverlayPlotWidget(QGraphicsItem * parent = nullptr);
    ~SpatialOverlayPlotWidget() override = default;

    QString getPlotType() const override;

    /**
     * @brief Set which PointData keys to display
     * @param point_data_keys List of PointData keys to visualize
     */
    void setPointDataKeys(QStringList const & point_data_keys);

    /**
     * @brief Get currently displayed PointData keys
     */
    QStringList getPointDataKeys() const { return _point_data_keys; }

    /**
     * @brief Get access to the OpenGL widget for advanced configuration
     * @return Pointer to the OpenGL widget
     */
    SpatialOverlayOpenGLWidget * getOpenGLWidget() const { return _opengl_widget; }

signals:
    /**
     * @brief Emitted when user requests to jump to a specific frame
     * @param time_frame_index The time frame index to jump to
     */
    void frameJumpRequested(int64_t time_frame_index);

    /**
     * @brief Emitted when rendering properties change (point size, zoom, pan)
     */
    void renderingPropertiesChanged();

protected:
    void paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget = nullptr) override;
    void resizeEvent(QGraphicsSceneResizeEvent * event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;

private slots:
    /**
     * @brief Update the visualization when data changes
     */
    void updateVisualization();

    /**
     * @brief Handle frame jump request from OpenGL widget
     * @param time_frame_index The time frame index to jump to
     */
    void handleFrameJumpRequest(int64_t time_frame_index);

private:
    SpatialOverlayOpenGLWidget * _opengl_widget;
    QGraphicsProxyWidget * _proxy_widget;
    QStringList _point_data_keys;

    /**
     * @brief Load point data from DataManager
     */
    void loadPointData();

    /**
     * @brief Setup the OpenGL widget and proxy
     */
    void setupOpenGLWidget();
};

#endif// SPATIALOVERLAYPLOTWIDGET_HPP