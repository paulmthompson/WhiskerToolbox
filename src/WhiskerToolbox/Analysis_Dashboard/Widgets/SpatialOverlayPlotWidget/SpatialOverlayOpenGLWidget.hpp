#ifndef SPATIALOVERLAYOPENGLWIDGET_HPP
#define SPATIALOVERLAYOPENGLWIDGET_HPP

#include "Visualizers/RenderingContext.hpp"
#include "Selection/SelectionHandlers.hpp"
#include "Selection/SelectionModes.hpp"
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
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class PointData;
class PointDataVisualization;
class MaskData;
class MaskDataVisualization;
class LineData;
class LineDataVisualization;
class PolygonSelectionHandler;
class LineSelectionHandler;
class NoneSelectionHandler;
class GroupManager;

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

    //========== Visibility Management ==========

    /**
     * @brief Hide selected items from view
     */
    void hideSelectedItems();

    /**
     * @brief Show all hidden items in the current active dataset
     */
    void showAllItemsCurrentDataset();

    /**
     * @brief Show all hidden items across all datasets
     */
    void showAllItemsAllDatasets();

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
     * @brief Set the line width for rendering
     * @param line_width Line width in pixels
     */
    void setLineWidth(float line_width);

    /**
     * @brief Get current line width
     */
    float getLineWidth() const { return _line_width; }

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

    /**
     * @brief Get total number of selected lines across all LineData visualizations
     * @return Total selected line count
     */
    size_t getTotalSelectedLines() const;

    void makeSelection();

    void applyTimeRangeFilter(int start_frame, int end_frame);


    void setGroupManager(GroupManager * group_manager);


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
     * @brief Emitted when line width changes
     * @param line_width The new line width in pixels
     */
    void lineWidthChanged(float line_width);

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

    /**
     * @brief Throttled update method to limit FPS
     */
    void requestThrottledUpdate();

private:
    std::unordered_map<QString, std::unique_ptr<PointDataVisualization>> _point_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<MaskDataVisualization>> _mask_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<LineDataVisualization>> _line_data_visualizations;

    GroupManager * _group_manager = nullptr;
    bool _opengl_resources_initialized;

    // View parameters
    float _zoom_level;
    float _pan_offset_x, _pan_offset_y;
    float _point_size;
    float _line_width;
    QMatrix4x4 _projection_matrix;
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _model_matrix;

    // Interaction state
    bool _is_panning;
    QPoint _last_mouse_pos;
    QPoint _current_mouse_pos;
    QTimer * _tooltip_timer;
    QTimer * _tooltip_refresh_timer;
    QTimer * _fps_limiter_timer;   // fps limiting
    QTimer * _hover_debounce_timer;// Debounce timer for hover processing
    bool _tooltips_enabled;
    bool _pending_update;         //fps limiting
    bool _hover_processing_active;// Flag to track if hover processing is active
    QPoint _pending_hover_pos;    // Store the latest hover position for processing
    SelectionMode _selection_mode;// Current selection mode

    SelectionVariant _selection_handler;

    QVector2D _current_mouse_world_pos;///< Current mouse position in world coordinates


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
     * @brief Render all lines using OpenGL
     */
    void renderLines();

    /**
     * @brief Creates a rendering context for the current frame
     * @return A RenderingContext object populated with the current state.
     */
    RenderingContext createRenderingContext() const;


    /**
     * @brief Render common overlay elements (tooltips, selection indicators, etc.)
     */
    void renderCommonOverlay();

    /**
     * @brief Update mouse position in world coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     */
    void updateMouseWorldPosition(int screen_x, int screen_y);

    /**
     * @brief Show context menu at the given position
     * @param pos The position to show the menu at
     */
    void showContextMenu(QPoint const & pos);

    /**
     * @brief Assign selected points to a new group
     */
    void assignSelectedPointsToNewGroup();

    /**
     * @brief Assign selected points to an existing group
     * @param group_id The ID of the group to assign to
     */
    void assignSelectedPointsToGroup(int group_id);

    /**
     * @brief Remove selected points from their groups
     */
    void ungroupSelectedPoints();
};

#endif// SPATIALOVERLAYOPENGLWIDGET_HPP