#ifndef SPATIALOVERLAYOPENGLWIDGET_HPP
#define SPATIALOVERLAYOPENGLWIDGET_HPP

#include "PolygonSelectionHandler.hpp"
#include "SelectionModes.hpp"
#include "SpatialIndex/QuadTree.hpp"
#include "Masks/MaskDataVisualization.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
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
class PointDataVisualization;
class MaskData;
class MaskDataVisualization;
class LineData;
class LineDataVisualization;

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
        /**
     * @brief Programmatically clear all selected points
     */
    void clearSelection();

    /**
     * @brief Convert screen coordinates to world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return World coordinates
     */
    QVector2D screenToWorld(int screen_x, int screen_y) const;

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
     * @brief Get the currently selected points from all PointData objects
     * @return Vector of pairs containing data key and selected points
     */
    std::vector<std::pair<QString, std::vector<QuadTreePoint<int64_t> const *>>> getSelectedPointData() const;

    /**
     * @brief Get total number of selected points across all PointData visualizations
     * @return Total selected point count
     */
    size_t getTotalSelectedPoints() const;

    // ========== Mask Data ==========

    /**
     * @brief Set the mask data to display
     * @param mask_data_map Map of data key to MaskData objects
     */
    void setMaskData(std::unordered_map<QString, std::shared_ptr<MaskData>> const & mask_data_map);


    /**
     * @brief Get the currently selected masks from all MaskData objects
     * @return Vector of pairs containing data key and selected mask identifiers
     */
    std::vector<std::pair<QString, std::vector<MaskIdentifier>>> getSelectedMaskData() const;

    /**
     * @brief Get total number of selected masks across all MaskData visualizations
     * @return Total selected mask count
     */
    size_t getTotalSelectedMasks() const;

    // ========== Line Data ==========

    /**
     * @brief Set the line data to display
     * @param line_data_map Map of data key to LineData objects
     */
    void setLineData(std::unordered_map<QString, std::shared_ptr<LineData>> const & line_data_map);

signals:
    /**
     * @brief Emitted when user double-clicks on a point to jump to that frame
     * @param time_frame_index The time frame index to jump to
     * @param data_key The data key of the point
     */
    void frameJumpRequested(int64_t time_frame_index, QString const & data_key);

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
     * @param total_count Total number of selected points across all datasets
     * @param data_key The data key of the most recently modified dataset
     * @param data_specific_count Number of selected points in the specific dataset
     */
    void selectionChanged(size_t total_count, QString const & data_key, size_t data_specific_count);

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

    /**
     * @brief Handle hover debounce timer timeout - processes expensive hover operations
     */
    void processHoverDebounce();

private slots:
    /**
     * @brief Throttled update method to limit FPS
     */
    void requestThrottledUpdate();

private:
    // PointData visualizations - each PointData has its own QuadTree and OpenGL resources
    std::unordered_map<QString, std::unique_ptr<PointDataVisualization>> _point_data_visualizations;
    
    // MaskData visualizations - each MaskData has its own RTree and OpenGL resources
    std::unordered_map<QString, std::unique_ptr<MaskDataVisualization>> _mask_data_visualizations;

    // LineData visualizations - each LineData has its own OpenGL resources
    //std::unordered_map<QString, std::unique_ptr<LineDataVisualization>> _line_data_visualizations;

    // Modern OpenGL rendering resources
    QOpenGLShaderProgram * _shader_program;
    QOpenGLShaderProgram * _line_shader_program;
    QOpenGLShaderProgram * _texture_shader_program;

    // Global highlight rendering resources (shared across all PointData)
    QOpenGLBuffer _highlight_vertex_buffer;
    QOpenGLVertexArrayObject _highlight_vertex_array_object;

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
    QTimer * _fps_limiter_timer;// fps limiting
    QTimer * _hover_debounce_timer;// Debounce timer for hover processing
    bool _tooltips_enabled;
    bool _pending_update;         //fps limiting
    bool _hover_processing_active;// Flag to track if hover processing is active
    QPoint _pending_hover_pos;    // Store the latest hover position for processing
    SelectionMode _selection_mode;// Current selection mode

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
    void calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const;

    /**
     * @brief Update view matrices based on current zoom and pan
     */
    void updateViewMatrices();

    /**
     * @brief Calculate data bounds from all PointData visualizations
     */
    void calculateDataBounds();

    /**
     * @brief Find point near screen coordinates across all PointData visualizations
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param tolerance_pixels Tolerance in pixels
     * @return Pair of PointDataVisualization and point, or {nullptr, nullptr} if none found
     */
    std::pair<PointDataVisualization *, QuadTreePoint<int64_t> const *> findPointNear(int screen_x, int screen_y, float tolerance_pixels = 10.0f) const;

    /**
     * @brief Find masks near screen coordinates across all MaskData visualizations
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return Vector of pairs containing MaskDataVisualization and R-tree entries with bounding boxes
     */
    std::vector<std::pair<MaskDataVisualization *, std::vector<RTreeEntry<MaskIdentifier>>>> findMasksNear(int screen_x, int screen_y) const;

    /**
     * @brief Get the PointDataVisualization that currently has a hover point
     * @return Pointer to visualization with hover point, or nullptr
     */
    PointDataVisualization * getCurrentHoverVisualization() const;

    /**
     * @brief Calculate world tolerance from screen tolerance
     * @param screen_tolerance Tolerance in screen pixels
     * @return World tolerance
     */
    float calculateWorldTolerance(float screen_tolerance) const;

    /**
     * @brief Render all points using OpenGL
     */
    void renderPoints();

    /**
     * @brief Render all masks using OpenGL
     */
    void renderMasks();

    /**
     * @brief Apply a selection region to find all points within it
     * @param region The selection region to apply
     * @param add_to_selection If true, add to existing selection; if false, replace selection
     */
    void applySelectionRegion(SelectionRegion const & region, bool add_to_selection = false);
};

QString create_tooltipText(QuadTreePoint<int64_t> const * point, QString const & data_key);

#endif// SPATIALOVERLAYOPENGLWIDGET_HPP