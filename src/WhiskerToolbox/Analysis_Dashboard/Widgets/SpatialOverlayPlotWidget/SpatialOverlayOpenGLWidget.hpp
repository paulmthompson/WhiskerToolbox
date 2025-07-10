#ifndef SPATIALOVERLAYOPENGLWIDGET_HPP
#define SPATIALOVERLAYOPENGLWIDGET_HPP

#include "SelectionModes.hpp"
#include "SpatialIndex/QuadTree.hpp"

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
#include <vector>

class PointData;

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
     * @brief Get the currently selected point indices
     * @return Set of selected point indices
     */
    std::set<size_t> const & getSelectedPointIndices() const { return _selected_point_indices; }

    /**
     * @brief Get the number of currently selected points
     * @return Number of selected points
     */
    size_t getSelectedPointCount() const { return _selected_point_indices.size(); }

    /**
     * @brief Programmatically clear all selected points
     */
    void clearSelection();

    /**
     * @brief Get the spatial point data for all selected points
     * @return Vector of pointers to selected point data
     */
    std::vector<SpatialPointData const *> getSelectedPointData() const {
        std::vector<SpatialPointData const *> selected_points;
        selected_points.reserve(_selected_point_indices.size());
        
        for (size_t index : _selected_point_indices) {
            if (index < _all_points.size()) {
                selected_points.push_back(&_all_points[index]);
            }
        }
        
        return selected_points;
    }

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
     * @param selected_indices Set of selected point indices
     */
    void selectionChanged(size_t selected_count, std::set<size_t> const & selected_indices);

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
    std::vector<SpatialPointData> _all_points;
    std::unique_ptr<QuadTree<size_t>> _spatial_index;// Index into _all_points

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
    
    // Polygon rendering resources
    QOpenGLBuffer _polygon_vertex_buffer;
    QOpenGLVertexArrayObject _polygon_vertex_array_object;
    QOpenGLBuffer _polygon_line_buffer;
    QOpenGLVertexArrayObject _polygon_line_array_object;
    
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
    QTimer * _fps_limiter_timer;
    bool _tooltips_enabled;
    bool _pending_update;
    SpatialPointData const * _current_hover_point;

    // Selection state
    std::set<size_t> _selected_point_indices;  // Set of selected point indices
    std::vector<float> _selection_vertex_data; // Cached vertex data for selected points
    SelectionMode _selection_mode;             // Current selection mode
    
    // Polygon selection state
    bool _is_polygon_selecting;                // True when actively drawing polygon
    std::vector<QVector2D> _polygon_vertices;  // Current polygon vertices in world coordinates
    std::vector<QPoint> _polygon_screen_points; // Polygon vertices in screen coordinates for rendering
    std::unique_ptr<SelectionRegion> _active_selection_region; // Current selection region

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
     */
    void updateSpatialIndex();

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
    SpatialPointData const * findPointNear(int screen_x, int screen_y, float tolerance_pixels = 10.0f) const;

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
     * @brief Toggle selection of a point by index
     * @param point_index Index of point in _all_points vector
     */
    void togglePointSelection(size_t point_index);

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
     * @brief Complete polygon selection and select enclosed points
     */
    void completePolygonSelection();

    /**
     * @brief Cancel current polygon selection
     */
    void cancelPolygonSelection();

    /**
     * @brief Render polygon selection overlay using OpenGL
     */
    void renderPolygonOverlay();
    
    /**
     * @brief Update polygon vertex and line buffers
     */
    void updatePolygonBuffers();

    /**
     * @brief Update vertex buffer with current point data
     */
    void updateVertexBuffer();

};

#endif // SPATIALOVERLAYOPENGLWIDGET_HPP