#ifndef POINTDATAVISUALIZATION_HPP
#define POINTDATAVISUALIZATION_HPP

#include "../Selection/SelectionHandlers.hpp"
#include "SpatialIndex/QuadTree.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class PointData;
class QOpenGLShaderProgram;
class PolygonSelectionHandler;
class LineSelectionHandler;
class NoneSelectionHandler;
class PointSelectionHandler;
class GroupManager;


/**
 * @brief Visualization for Point Data
 * 
 * Each Point will have state for:
 * - Selection state (selected or not)
 * - X/Y coordinates in world space
 * - Visibility (hidden or not)

 * 
 * 
 * Data is stored in a QuadTree for efficient spatial queries
 */
struct PointDataVisualization : protected QOpenGLFunctions_4_1_Core {
    std::unique_ptr<QuadTree<int64_t>> m_spatial_index;
    std::vector<float> m_vertex_data;// Format: x, y, group_id per vertex (3 floats per point)
    QOpenGLBuffer m_vertex_buffer;
    QOpenGLVertexArrayObject m_vertex_array_object;
    QString m_key;
    QVector4D m_color;
    bool m_visible = true;

    // Selection state for this PointData
    std::unordered_set<QuadTreePoint<int64_t> const *> m_selected_points;
    std::vector<float> m_selection_vertex_data;
    QOpenGLBuffer m_selection_vertex_buffer;
    QOpenGLVertexArrayObject m_selection_vertex_array_object;

    // Hover state for this PointData
    QuadTreePoint<int64_t> const * m_current_hover_point = nullptr;
    QOpenGLBuffer m_highlight_vertex_buffer;
    QOpenGLVertexArrayObject m_highlight_vertex_array_object;

    // Visibility management for points
    std::unordered_set<QuadTreePoint<int64_t> const *> m_hidden_points;

    // Statistics tracking
    size_t m_total_point_count = 0;
    size_t m_hidden_point_count = 0;
    size_t m_visible_vertex_count = 0;// Number of vertices currently in the vertex buffer

    // Time range filtering
    int m_time_range_start = 0;
    int m_time_range_end = 999999;
    bool m_time_range_enabled = false;

    // Group management
    GroupManager * m_group_manager = nullptr;
    bool m_group_data_needs_update = false;

    PointDataVisualization(QString const & data_key,
                           std::shared_ptr<PointData> const & point_data,
                           GroupManager * group_manager = nullptr);
    ~PointDataVisualization();

    /**
     * @brief Initialize OpenGL resources for this visualization
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources for this visualization
     */
    void cleanupOpenGLResources();

    /**
     * @brief Update selection vertex buffer with current selection
     */
    void updateSelectionVertexBuffer();

    /**
     * @brief Clear all selected points
     */
    void clearSelection();

    /**
     * @brief Clear hover point
     */
    void clearHover();

    /**
     * @brief Toggle selection of a point
     * @param point_ptr Pointer to the point to toggle
     * @return True if point was selected, false if deselected
     */
    bool togglePointSelection(QuadTreePoint<int64_t> const * point_ptr);

    /**
     * @brief Remove a specific point from selection if it's currently selected
     * @param point_ptr Pointer to the point to remove from selection
     * @return True if point was removed from selection, false if it wasn't selected
     */
    bool removePointFromSelection(QuadTreePoint<int64_t> const * point_ptr);

    /**
     * @brief Render all points, selections, and highlights for this visualization
     * @param mvp_matrix The model-view-projection matrix
     * @param point_size The size of the points to render
     */
    void render(QMatrix4x4 const & mvp_matrix, float point_size);


    //========== Selection Handlers ==========

    /**
     * @brief Apply selection to this PointDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(SelectionVariant & selection_handler);

    /**
     * @brief Apply selection to this PointDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(PolygonSelectionHandler const & selection_handler);

    /**
     * @brief Apply selection to this PointDataVisualization
     * @param selection_handler The PointSelectionHandler to apply
     */
    void applySelection(PointSelectionHandler const & selection_handler);

    /**
     * @brief Get tooltip text for the current hover state
     * @return QString with tooltip information, or empty if no hover
     */
    QString getTooltipText() const;

    /**
     * @brief Handle hover events for this visualization
     * @param world_pos The mouse position in world coordinates
     * @param tolerance The tolerance in world coordinates for finding a near point
     * @return True if the hover state changed, false otherwise
     */
    bool handleHover(QVector2D const & world_pos, float tolerance);

    /**
     * @brief Handle double-click events for this visualization
     * @param world_pos The mouse position in world coordinates
     * @param tolerance The tolerance in world coordinates for finding a near point
     * @return The time frame index of the double-clicked point, or std::nullopt if none
     */
    std::optional<int64_t> handleDoubleClick(QVector2D const & world_pos, float tolerance);

    //========== Visibility Management ==========

    /**
     * @brief Hide selected points from view
     * @return Number of points that were hidden
     */
    size_t hideSelectedPoints();

    /**
     * @brief Show all hidden points in this visualization
     * @return Number of points that were shown
     */
    size_t showAllPoints();

    /**
     * @brief Get visibility statistics
     * @return Pair of (total_points, hidden_points)
     */
    std::pair<size_t, size_t> getVisibilityStats() const;

    void setTimeRangeEnabled(bool enabled);
    void setTimeRange(int start_frame, int end_frame);

    //========== Group Management ==========

    /**
     * @brief Set the group manager for this visualization
     * @param group_manager Pointer to the group manager
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Get selected point time stamp IDs for group assignment
     * @return Set of time stamp IDs of currently selected points
     */
    std::unordered_set<int64_t> getSelectedPointIds() const;

    /**
     * @brief Refresh group-based rendering data (call when group assignments change)
     */
    void refreshGroupRenderData();

private:
    /**
     * @brief Render points for this PointData
     */
    void _renderPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render selected points for this PointData
     */
    void _renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render hover point for this PointData
     */
    void _renderHoverPoint(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Update vertex buffer to exclude hidden points
     */
    void _updateVisibleVertexBuffer();

    /**
     * @brief Update group IDs in vertex data
     */
    void _updateGroupVertexData();
};


#endif// POINTDATAVISUALIZATION_HPP