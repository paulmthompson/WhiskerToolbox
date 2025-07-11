#ifndef SPATIALOVERLAYOPENGLWIDGET_HPP
#define SPATIALOVERLAYOPENGLWIDGET_HPP

#include "SelectionModes.hpp"
#include "SpatialIndex/QuadTree.hpp"
#include "PolygonSelectionHandler.hpp"

#include <QMatrix4x4>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class PointData;

/**
 * @brief Data structure for storing frame and key information in the QuadTree
 */
struct QuadTreePointData {
    int64_t time_frame_index;
    std::string point_data_key;
    
    QuadTreePointData(int64_t frame_index, std::string const & key)
        : time_frame_index(frame_index), point_data_key(key) {}
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
     * @brief Set the current selection mode
     * @param mode The selection mode to activate
     */
    void setSelectionMode(SelectionMode mode);
    
    /**
     * @brief Get the current selection mode
     */
    SelectionMode getSelectionMode() const { return _selection_mode; }

    /**
     * @brief Enable or disable tooltips
     * @param enabled Whether tooltips should be enabled
     */
    void setTooltipsEnabled(bool enabled);

    /**
     * @brief Get current tooltip enabled state
     */
    bool getTooltipsEnabled() const { return _tooltips_enabled; }

    // ========== Point Data ==========

    /**
     * @brief Set the point data to display
     * @param point_data_map Map of data key to PointData objects
     */
    void setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map);

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
     * @brief Get the currently selected points
     * @return Set of pointers to selected points
     */
    std::unordered_set<QuadTreePoint<QuadTreePointData> const *> const & getSelectedPoints() const { return _selected_points; }

    /**
     * @brief Get the number of currently selected points
     * @return Number of selected points
     */
    size_t getSelectedPointCount() const { return _selected_points.size(); }

    /**
     * @brief Programmatically clear all selected points
     */
    void clearSelection();

    /**
     * @brief Get the selected point data for all selected points
     * @return Vector of pointers to selected QuadTreePoint objects
     */
    std::vector<QuadTreePoint<QuadTreePointData> const *> getSelectedPointData() const;

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return World coordinates
     */
    QVector2D screenToWorld(int screen_x, int screen_y) const;

signals:
    /**
     * @brief Emitted when user double-clicks on a point to jump to that frame
     * @param time_frame_index The time frame index to jump to
     */
    void frameJumpRequested(int64_t time_frame_index, std::string const & data_key);

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

    /**
     * @brief Emitted when the selection changes
     * @param selected_count Number of currently selected points
     * @param selected_points Set of pointers to selected points
     */
    void selectionChanged(size_t selected_count, std::unordered_set<QuadTreePoint<QuadTreePointData> const *> const & selected_points);

    /**
     * @brief Emitted when the selection mode changes
     * @param mode The new selection mode
     */
    void selectionModeChanged(SelectionMode mode);

    /**
     * @brief Emitted when the highlight state changes, requiring scene graph update
     */
    void highlightStateChanged();
protected:
    void initializeGL() override;
    void paintGL() override;
    void paintEvent(QPaintEvent * event) override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void leaveEvent(QEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;

private slots:
    /**
     * @brief Handle tooltip timer timeout
     */
    void _handleTooltipTimer();

    /**
     * @brief Handle tooltip refresh timer timeout
     */
    void handleTooltipRefresh();

private slots:
    /**
     * @brief Throttled update method to limit FPS
     */
    void requestThrottledUpdate();

private:
    // Rendering data
    std::vector<float> _vertex_data; // Flattened vertex data for OpenGL (x,y pairs)
    std::unique_ptr<QuadTree<QuadTreePointData>> _spatial_index; // Contains frame ID and data key

    // Modern OpenGL rendering resources
    QOpenGLShaderProgram * _shader_program;
    QOpenGLShaderProgram * _line_shader_program;
    QOpenGLBuffer _vertex_buffer;
    QOpenGLVertexArrayObject _vertex_array_object;
    
    // Highlight rendering resources
    QOpenGLBuffer _highlight_vertex_buffer;
    QOpenGLVertexArrayObject _highlight_vertex_array_object;
    
    // Selection rendering resources
    QOpenGLBuffer _selection_vertex_buffer;
    QOpenGLVertexArrayObject _selection_vertex_array_object;
    
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
    QTimer * _tooltip_refresh_timer;
    QTimer * _fps_limiter_timer; // fps limiting
    bool _tooltips_enabled;
    bool _pending_update; //fps limiting
    QuadTreePoint<QuadTreePointData> const * _current_hover_point;

    // Selection state
    std::unordered_set<QuadTreePoint<QuadTreePointData> const *> _selected_points; // Pointers to selected points in QuadTree
    std::vector<float> _selection_vertex_data; // Cached vertex data for selected points
    SelectionMode _selection_mode;             // Current selection mode
    
    // Polygon selection handler
    std::unique_ptr<PolygonSelectionHandler> _polygon_selection_handler;

    // Data bounds
    float _data_min_x, _data_max_x, _data_min_y, _data_max_y;
    bool _data_bounds_valid;

    /**
     * @brief Initialize OpenGL shaders and resources
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources
     */
    void cleanupOpenGLResources();

    /**
     * @brief Convert world coordinates to screen coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Screen coordinates
     */
    QPoint worldToScreen(float world_x, float world_y) const;

    /**
     * @brief Calculate the current orthographic projection bounds (helper for coordinate transformations)
     * @param left Output: left bound
     * @param right Output: right bound  
     * @param bottom Output: bottom bound
     * @param top Output: top bound
     */
    void calculateProjectionBounds(float &left, float &right, float &bottom, float &top) const;

    /**
     * @brief Update view matrices based on current zoom and pan
     */
    void updateViewMatrices();

    /**
     * @brief Update the spatial index with current point data
     * @param point_data_map Map of data key to PointData objects
     */
    void updateSpatialIndex(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map);

    /**
     * @brief Calculate data bounds from all points
     */
    void calculateDataBounds();

    /**
     * @brief Find point near screen coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param tolerance_pixels Tolerance in pixels
     * @return Pointer to point data, or nullptr if none found
     */
    QuadTreePoint<QuadTreePointData> const * findPointNear(int screen_x, int screen_y, float tolerance_pixels = 10.0f) const;

    /**
     * @brief Render all points using OpenGL
     */
    void renderPoints();

    /**
     * @brief Render highlighted point with hollow circle
     */
    void renderHighlightedPoint();

    /**
     * @brief Render selected points
     */
    void renderSelectedPoints();

    /**
     * @brief Toggle selection of a point
     * @param point The point to toggle selection for
     */
    void togglePointSelection(QuadTreePoint<QuadTreePointData> const & point);

    /**
     * @brief Update selection vertex buffer with current selection
     */
    void updateSelectionVertexBuffer();

    /**
     * @brief Apply a selection region to find all points within it
     * @param region The selection region to apply
     * @param add_to_selection If true, add to existing selection; if false, replace selection
     */
    void applySelectionRegion(SelectionRegion const& region, bool add_to_selection = false);

    /**
     * @brief Update vertex buffer with current point data
     */
    void updateVertexBuffer();

};

QString create_tooltipText(QuadTreePoint<QuadTreePointData> const * point);

#endif // SPATIALOVERLAYOPENGLWIDGET_HPP