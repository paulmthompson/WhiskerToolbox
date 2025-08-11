#ifndef SPATIALOVERLAYOPENGLWIDGET_HPP
#define SPATIALOVERLAYOPENGLWIDGET_HPP

#include "Visualizers/RenderingContext.hpp"
#include "Selection/SelectionHandlers.hpp"
#include "Selection/SelectionModes.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "CoreGeometry/boundingbox.hpp"


#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>
#include "Analysis_Dashboard/Widgets/Common/PlotInteractionController.hpp"

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
class SpatialOverlayViewAdapter; // adapter (friend)
class QKeyEvent;

/**
 * @brief OpenGL widget for rendering spatial data with high performance
 */
class SpatialOverlayOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core {
    Q_OBJECT

public:
    explicit SpatialOverlayOpenGLWidget(QWidget * parent = nullptr);
    ~SpatialOverlayOpenGLWidget() override;

    /**
     * @brief Reset view to fit all data (zoom and pan to defaults)
     */
    void resetView();

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
     * @brief Public method to handle key press events from external sources
     * @param event The key event to handle
     */
    void handleKeyPress(QKeyEvent* event);

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

    /**
     * @brief Emitted when the current world view bounds change (after zoom/pan/resize/box-zoom)
     * @param left Left world bound
     * @param right Right world bound
     * @param bottom Bottom world bound
     * @param top Top world bound
     */
    void viewBoundsChanged(float left, float right, float bottom, float top);

    /**
     * @brief Emitted when the mouse moves, reporting world coordinates under the cursor
     * @param world_x X coordinate in world space
     * @param world_y Y coordinate in world space
     */
    void mouseWorldMoved(float world_x, float world_y);

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
    // Grant adapter access to private state for high-performance interaction
    friend class SpatialOverlayViewAdapter;

    std::unordered_map<QString, std::unique_ptr<PointDataVisualization>> _point_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<MaskDataVisualization>> _mask_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<LineDataVisualization>> _line_data_visualizations;

    GroupManager * _group_manager = nullptr;
    bool _opengl_resources_initialized;

    // View parameters
    float _zoom_level_x; // per-axis zoom (X)
    float _zoom_level_y; // per-axis zoom (Y)
    float _pan_offset_x, _pan_offset_y;
    float _center_x = 0.0f; // camera center (world units)
    float _center_y = 0.0f; // camera center (world units)
    float _point_size;
    float _line_width;
    QMatrix4x4 _projection_matrix;
    QMatrix4x4 _view_matrix;
    QMatrix4x4 _model_matrix;
    float _padding_factor; // view padding factor (default 1.1)

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

    // Composition-based interaction controller
    std::unique_ptr<PlotInteractionController> _interaction;

    // Data bounds
    BoundingBox _data_bounds;
    bool _data_bounds_valid;

    // Context menu
    QMenu * _contextMenu {nullptr};
    QMenu * _assignGroupMenu {nullptr};

    QAction * _actionCreateNewGroup {nullptr};
    QAction * _actionUngroupSelected {nullptr};
    QAction * _actionShowAllCurrent {nullptr};
    QAction * _actionShowAllDatasets {nullptr};
    QAction * _actionHideSelected {nullptr};

    QList<QAction*> _dynamicGroupActions;

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

    // ========== Context Menu ==========

    void _initializeContextMenu();

    /**
     * @brief Show context menu at the given position
     * @param pos The position to show the menu at
     */
    void _showContextMenu(QPoint const & pos);

    void _updateContextMenuState();

    void _updateDynamicGroupActions();

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

    /**
     * @brief Check if OpenGL context is properly initialized
     * @return true if OpenGL context is valid and ready for rendering
     */
    bool isOpenGLContextValid() const;

    /**
     * @brief Get OpenGL context error information
     * @return QString containing error information if context creation failed
     */
    QString getOpenGLErrorInfo() const;

    /**
     * @brief Force OpenGL context creation (for debugging)
     * @return true if context creation was successful
     */
    bool forceContextCreation();

private:
        /**
         * @brief Compute camera center and visible world extents for current view
         *
         * Derives the world-space camera center and the visible world width/height
         * from current data bounds, padding, per-axis zoom and normalized pan offsets.
         * Aspect ratio corrections are applied in the same way as rendering, ensuring
         * that the returned values exactly match the visible region.
         *
         * @pre _data_bounds_valid must be true and widget size positive
         * @post On success, outputs are populated with center and extents in world units
         */
        void computeCameraWorldView(float & center_x,
                                    float & center_y,
                                    float & world_width,
                                    float & world_height) const;
    /**
     * @brief Try to create OpenGL context with specified version
     * @param major Major version number
     * @param minor Minor version number
     * @return true if context creation was successful
     */
    bool tryCreateContextWithVersion(int major, int minor);
};

#endif// SPATIALOVERLAYOPENGLWIDGET_HPP