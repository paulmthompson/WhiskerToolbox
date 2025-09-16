#ifndef GENERICPOINTVISUALIZATION_HPP
#define GENERICPOINTVISUALIZATION_HPP

#include "Selection/SelectionHandlers.hpp"
#include "Selection/PointSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"

#include "GroupManagementWidget/GroupManager.hpp"
#include "SpatialIndex/QuadTree.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "DataManager/Entity/EntityTypes.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class QOpenGLShaderProgram;
class LineSelectionHandler;
class NoneSelectionHandler;
class GroupManager;

/**
 * @brief Generic visualization for point data with templated data types
 * 
 * This class provides a generic implementation for visualizing points with
 * any coordinate type (float/double) and any row indicator type.
 * 
 * @tparam CoordType The coordinate type (float or double)
 * @tparam RowIndicatorType The type used for row indicators (e.g., int, TimeFrameIndex)
 */
template<typename CoordType, typename RowIndicatorType>
struct GenericPointVisualization : protected QOpenGLFunctions_4_1_Core {
public:
    std::unique_ptr<QuadTree<RowIndicatorType>> m_spatial_index;
    std::vector<float> m_vertex_data;// Format: x, y, group_id per vertex (3 floats per point)
    std::vector<EntityId> m_entity_ids; // 1:1 with points when available
    QOpenGLBuffer m_vertex_buffer;
    QOpenGLVertexArrayObject m_vertex_array_object;
    QString m_key;
    QVector4D m_color;
    bool m_visible = true;

    // Selection state for this visualization
    std::unordered_set<QuadTreePoint<RowIndicatorType> const *> m_selected_points;
    std::vector<float> m_selection_vertex_data;
    QOpenGLBuffer m_selection_vertex_buffer;
    QOpenGLVertexArrayObject m_selection_vertex_array_object;

    // Hover state for this visualization
    QuadTreePoint<RowIndicatorType> const * m_current_hover_point = nullptr;
    QOpenGLBuffer m_highlight_vertex_buffer;
    QOpenGLVertexArrayObject m_highlight_vertex_array_object;

    // Visibility management for points
    std::unordered_set<QuadTreePoint<RowIndicatorType> const *> m_hidden_points;

    // Statistics tracking
    size_t m_total_point_count = 0;
    size_t m_hidden_point_count = 0;
    size_t m_visible_vertex_count = 0;// Number of vertices currently in the vertex buffer

    // Data range filtering
    RowIndicatorType m_data_range_start = RowIndicatorType{};
    RowIndicatorType m_data_range_end = RowIndicatorType{};
    bool m_data_range_enabled = false;

    // Group management
    GroupManager * m_group_manager = nullptr;
    bool m_group_data_needs_update = false;

    /**
     * @brief Constructor for generic point visualization
     * @param data_key The key identifier for this visualization
     * @param group_manager Optional group manager for color coding
     */
    /**
     * @brief Constructor for generic point visualization
     * @param data_key The key identifier for this visualization
     * @param group_manager Optional group manager for color coding
     */
    GenericPointVisualization(QString const & data_key, GroupManager * group_manager = nullptr);
    
    /**
     * @brief Constructor with deferred OpenGL initialization option
     * @param data_key The key identifier for this visualization
     * @param group_manager Optional group manager for color coding
     * @param defer_opengl_init If true, skip OpenGL initialization in constructor
     */
    GenericPointVisualization(QString const & data_key, GroupManager * group_manager, bool defer_opengl_init);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~GenericPointVisualization();

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
    bool togglePointSelection(QuadTreePoint<RowIndicatorType> const * point_ptr);

    /**
     * @brief Remove a specific point from selection if it's currently selected
     * @param point_ptr Pointer to the point to remove from selection
     * @return True if point was removed from selection, false if it wasn't selected
     */
    bool removePointFromSelection(QuadTreePoint<RowIndicatorType> const * point_ptr);

    /**
     * @brief Render all points, selections, and highlights for this visualization
     * @param mvp_matrix The model-view-projection matrix
     * @param point_size The size of the points to render
     */
    void render(QMatrix4x4 const & mvp_matrix, float point_size);

    /**
     * @brief Set per-point group indices (palette indices) used by the shader for coloring
     * @pre ids.size() == m_total_point_count
     */
    void setPerPointGroupIds(std::vector<uint32_t> const & ids) {
        if (ids.size() != m_total_point_count) return;
        if (m_vertex_data.size() != m_total_point_count * 3) return;
        for (size_t i = 0; i < m_total_point_count; ++i) {
            m_vertex_data[i * 3 + 2] = static_cast<float>(ids[i]);
        }
        if (m_vertex_buffer.isCreated()) {
            m_vertex_buffer.bind();
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_vertex_data.size() * sizeof(float)), m_vertex_data.data());
            m_vertex_buffer.release();
        }
    }

    /**
     * @brief Set per-point EntityIds aligned to the current vertex order
     * @pre entity_ids.size() == m_total_point_count
     */
    void setPerPointEntityIds(std::vector<EntityId> entity_ids) {
        if (entity_ids.size() != m_total_point_count) return;
        m_entity_ids = std::move(entity_ids);
        m_group_data_needs_update = true;
    }

    //========== Selection Handlers ==========

    /**
     * @brief Apply selection to this visualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(SelectionVariant & selection_handler);

    /**
     * @brief Apply selection to this visualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(PolygonSelectionHandler const & selection_handler);

    /**
     * @brief Apply selection to this visualization
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
     * @return The row indicator of the double-clicked point, or std::nullopt if none
     */
    std::optional<RowIndicatorType> handleDoubleClick(QVector2D const & world_pos, float tolerance);

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

    /**
     * @brief Set data range filtering enabled state
     * @param enabled Whether data range filtering should be enabled
     */
    void setDataRangeEnabled(bool enabled);

    /**
     * @brief Set data range for filtering
     * @param start_value Start value for the range
     * @param end_value End value for the range
     */
    void setDataRange(RowIndicatorType start_value, RowIndicatorType end_value);

    //========== Group Management ==========

    /**
     * @brief Set the group manager for this visualization
     * @param group_manager Pointer to the group manager
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Get selected point row indicators for group assignment
     * @return Set of row indicators of currently selected points
     */
    std::unordered_set<RowIndicatorType> getSelectedPointIds() const;

    /**
     * @brief Refresh group-based rendering data (call when group assignments change)
     */
    void refreshGroupRenderData();

    /**
     * @brief Get selected EntityIds if available; fallback to using m_entity_ids mapping
     */
    std::unordered_set<EntityId> getSelectedEntityIds() const {
        std::unordered_set<EntityId> out;
        
        if constexpr (std::is_same_v<RowIndicatorType, EntityId>) {
            // Direct case: RowIndicatorType is EntityId
            for (auto const * p : m_selected_points) {
                out.insert(p->data);
            }
        } else if (m_entity_ids.size() == m_total_point_count && m_spatial_index) {
            // Build a pointer->index map once to avoid O(S*N) behavior
            std::vector<QuadTreePoint<RowIndicatorType> const *> all_points;
            all_points.reserve(m_total_point_count);
            m_spatial_index->queryPointers(m_spatial_index->getBounds(), all_points);

            std::unordered_map<QuadTreePoint<RowIndicatorType> const *, size_t> pointer_to_index;
            pointer_to_index.reserve(all_points.size());
            for (size_t i = 0; i < all_points.size(); ++i) {
                pointer_to_index.emplace(all_points[i], i);
            }

            for (auto const * p : m_selected_points) {
                auto it_idx = pointer_to_index.find(p);
                if (it_idx != pointer_to_index.end()) {
                    size_t const index = it_idx->second;
                    if (index < m_entity_ids.size()) {
                        out.insert(m_entity_ids[index]);
                    }
                }
            }
        }
        // If neither case applies, return empty set
        
        return out;
    }

protected:
    /**
     * @brief Pure virtual function to populate data from the specific data source
     * This must be implemented by derived classes to populate the spatial index and vertex data
     */
    virtual void populateData() = 0;

    /**
     * @brief Get the bounding box for the data
     * @return BoundingBox containing all data points
     */
    virtual BoundingBox getDataBounds() const = 0;

private:
    /**
     * @brief Render points for this visualization
     */
    void _renderPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render selected points for this visualization
     */
    void _renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render hover point for this visualization
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

// Template implementation
template<typename CoordType, typename RowIndicatorType>
GenericPointVisualization<CoordType, RowIndicatorType>::GenericPointVisualization(
    QString const & data_key, GroupManager * group_manager)
    : m_key(data_key),
      m_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_selection_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_highlight_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_color(1.0f, 0.0f, 0.0f, 1.0f),
      m_group_manager(group_manager),
      m_group_data_needs_update(false) {
    
    // Initialize bounds - will be set by derived class in populateData()
    BoundingBox initial_bounds(0.0f, 0.0f, 1.0f, 1.0f);
    m_spatial_index = std::make_unique<QuadTree<RowIndicatorType>>(initial_bounds);
    
    initializeOpenGLResources();
}

template<typename CoordType, typename RowIndicatorType>
GenericPointVisualization<CoordType, RowIndicatorType>::GenericPointVisualization(
    QString const & data_key, GroupManager * group_manager, bool defer_opengl_init)
    : m_key(data_key),
      m_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_selection_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_highlight_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_color(1.0f, 0.0f, 0.0f, 1.0f),
      m_group_manager(group_manager),
      m_group_data_needs_update(false) {
    
    // Initialize bounds - will be set by derived class in populateData()
    BoundingBox initial_bounds(0.0f, 0.0f, 1.0f, 1.0f);
    m_spatial_index = std::make_unique<QuadTree<RowIndicatorType>>(initial_bounds);
    
    // Only initialize OpenGL resources if not deferred
    if (!defer_opengl_init) {
        initializeOpenGLResources();
    }
}

template<typename CoordType, typename RowIndicatorType>
GenericPointVisualization<CoordType, RowIndicatorType>::~GenericPointVisualization() {
    cleanupOpenGLResources();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::initializeOpenGLResources() {
    qDebug() << "GenericPointVisualization::initializeOpenGLResources: Starting initialization for" << m_key;
    
    if (!initializeOpenGLFunctions()) {
        qDebug() << "GenericPointVisualization::initializeOpenGLResources: Failed to initialize OpenGL functions";
        return;
    }

    // Load point shader program from ShaderManager
    if (!ShaderManager::instance().loadProgram("point",
                                               ":/shaders/point.vert",
                                               ":/shaders/point.frag",
                                               "",
                                               ShaderSourceType::Resource)) {
        qDebug() << "GenericPointVisualization: Failed to load point shader program";
        return;
    }

    qDebug() << "GenericPointVisualization::initializeOpenGLResources: Shader program loaded successfully";

    m_vertex_array_object.create();
    m_vertex_array_object.bind();

    m_vertex_buffer.create();
    m_vertex_buffer.bind();
    m_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));

    qDebug() << "GenericPointVisualization::initializeOpenGLResources: Vertex buffer created with" 
             << m_vertex_data.size() << "components";

    // Position attribute (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // Group ID attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) (2 * sizeof(float)));

    m_vertex_buffer.release();
    m_vertex_array_object.release();

    qDebug() << "GenericPointVisualization::initializeOpenGLResources: Vertex array object setup complete";

    m_selection_vertex_array_object.create();
    m_selection_vertex_array_object.bind();

    m_selection_vertex_buffer.create();
    m_selection_vertex_buffer.bind();
    m_selection_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_selection_vertex_buffer.release();
    m_selection_vertex_array_object.release();

    m_highlight_vertex_array_object.create();
    m_highlight_vertex_array_object.bind();

    m_highlight_vertex_buffer.create();
    m_highlight_vertex_buffer.bind();
    m_highlight_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Pre-allocate highlight buffer for one point (2 floats)
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Set up vertex attributes for highlight
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_highlight_vertex_array_object.release();
    m_highlight_vertex_buffer.release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::cleanupOpenGLResources() {
    if (m_vertex_buffer.isCreated()) {
        m_vertex_buffer.destroy();
    }
    if (m_vertex_array_object.isCreated()) {
        m_vertex_array_object.destroy();
    }
    if (m_selection_vertex_buffer.isCreated()) {
        m_selection_vertex_buffer.destroy();
    }
    if (m_selection_vertex_array_object.isCreated()) {
        m_selection_vertex_array_object.destroy();
    }
    if (m_highlight_vertex_buffer.isCreated()) {
        m_highlight_vertex_buffer.destroy();
    }
    if (m_highlight_vertex_array_object.isCreated()) {
        m_highlight_vertex_array_object.destroy();
    }
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::updateSelectionVertexBuffer() {
    m_selection_vertex_data.clear();

    if (m_selected_points.empty()) {
        // Clear the buffer if no selection
        m_selection_vertex_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        m_selection_vertex_buffer.release();
        return;
    }

    // Prepare vertex data for selected points
    m_selection_vertex_data.reserve(m_selected_points.size() * 2);

    for (auto const * point_ptr: m_selected_points) {
        // Store original coordinates in selection vertex data
        m_selection_vertex_data.push_back(point_ptr->x);
        m_selection_vertex_data.push_back(point_ptr->y);
    }

    // Update the buffer
    m_selection_vertex_array_object.bind();
    m_selection_vertex_buffer.bind();
    m_selection_vertex_buffer.allocate(m_selection_vertex_data.data(),
                                       static_cast<int>(m_selection_vertex_data.size() * sizeof(float)));

    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_selection_vertex_buffer.release();
    m_selection_vertex_array_object.release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::clearSelection() {
    if (!m_selected_points.empty()) {
        m_selected_points.clear();
        updateSelectionVertexBuffer();
    }
}

template<typename CoordType, typename RowIndicatorType>
bool GenericPointVisualization<CoordType, RowIndicatorType>::togglePointSelection(QuadTreePoint<RowIndicatorType> const * point_ptr) {
    auto it = m_selected_points.find(point_ptr);

    if (it != m_selected_points.end()) {
        return false;// Point already selected
    } else {
        // Point is not selected, add it
        m_selected_points.insert(point_ptr);
        updateSelectionVertexBuffer();
        return true;// Point was selected
    }
}

template<typename CoordType, typename RowIndicatorType>
bool GenericPointVisualization<CoordType, RowIndicatorType>::removePointFromSelection(QuadTreePoint<RowIndicatorType> const * point_ptr) {
    auto it = m_selected_points.find(point_ptr);

    if (it != m_selected_points.end()) {
        // Point is selected, remove it
        m_selected_points.erase(it);
        updateSelectionVertexBuffer();
        return true;// Point was removed
    }

    return false;// Point wasn't selected
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::render(QMatrix4x4 const & mvp_matrix, float point_size) {
    auto pointProgram = ShaderManager::instance().getProgram("point");
    if (!pointProgram || !pointProgram->getNativeProgram()->bind()) {
        qDebug() << "GenericPointVisualization: Failed to bind point shader program";
        return;
    }

    pointProgram->getNativeProgram()->setUniformValue("u_mvp_matrix", mvp_matrix);

    // Update group vertex data if needed
    if (m_group_data_needs_update) {
        _updateGroupVertexData();
    }

    // Render regular points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    _renderPoints(pointProgram->getNativeProgram(), point_size);

    // Render selected points
    glDisable(GL_BLEND);
    _renderSelectedPoints(pointProgram->getNativeProgram(), point_size);

    // Render hover point
    if (m_current_hover_point) {
        _renderHoverPoint(pointProgram->getNativeProgram(), point_size);
    }

    glEnable(GL_BLEND);

    pointProgram->getNativeProgram()->release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::_renderPoints(QOpenGLShaderProgram * shader_program, float point_size) {
    if (!m_visible || m_vertex_data.empty()) {
        qDebug() << "GenericPointVisualization::_renderPoints: Skipping render - visible:" << m_visible 
                 << "vertex_data_empty:" << m_vertex_data.empty() << "total_points:" << m_total_point_count;
        return;
    }

    //qDebug() << "GenericPointVisualization::_renderPoints: Rendering" << m_total_point_count << "points";

    m_vertex_array_object.bind();
    m_vertex_buffer.bind();

    // Set up group colors if we have a group manager
    if (m_group_manager) {
        auto const & groups = m_group_manager->getGroups();

        // Prepare color array - index 0 is for ungrouped points
        std::vector<QVector4D> group_colors(256, m_color);
        group_colors[0] = m_color;// Index 0 = ungrouped color

        // Map group colors to consecutive indices starting from 1
        int color_index = 1;
        for (auto it = groups.begin(); it != groups.end() && color_index < 256; ++it) {
            auto const & group = it.value();
            group_colors[color_index] = QVector4D(group.color.redF(), group.color.greenF(),
                                                  group.color.blueF(), group.color.alphaF());
            color_index++;
        }

        // Pass color array to shader
        shader_program->setUniformValueArray("u_group_colors", group_colors.data(), 256);
        shader_program->setUniformValue("u_num_groups", 256);
    } else {
        // No groups, all points use default color
        std::vector<QVector4D> group_colors(256, m_color);
        shader_program->setUniformValueArray("u_group_colors", group_colors.data(), 256);
        shader_program->setUniformValue("u_num_groups", 256);
    }

    shader_program->setUniformValue("u_color", m_color);
    shader_program->setUniformValue("u_point_size", point_size);

    // Draw all points in a single call
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_total_point_count));

    m_vertex_buffer.release();
    m_vertex_array_object.release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::_renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size) {
    if (m_selected_points.empty()) return;

    m_selection_vertex_array_object.bind();
    m_selection_vertex_buffer.bind();

    // Set uniforms for selection rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    shader_program->setUniformValue("u_point_size", point_size * 1.5f);           // Slightly larger

    // Draw the selected points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_selected_points.size()));

    m_selection_vertex_buffer.release();
    m_selection_vertex_array_object.release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::_renderHoverPoint(QOpenGLShaderProgram * shader_program, float point_size) {
    if (!m_current_hover_point) return;

    m_highlight_vertex_array_object.bind();
    m_highlight_vertex_buffer.bind();

    // Store original coordinates in hover point data
    float highlight_data[2] = {m_current_hover_point->x, m_current_hover_point->y};
    glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), highlight_data);

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for highlight rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    shader_program->setUniformValue("u_point_size", point_size * 2.5f);           // Larger size

    // Draw the highlighted point
    glDrawArrays(GL_POINTS, 0, 1);

    m_highlight_vertex_buffer.release();
    m_highlight_vertex_array_object.release();
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::applySelection(SelectionVariant & selection_handler) {
    if (std::holds_alternative<std::unique_ptr<PolygonSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PolygonSelectionHandler>>(selection_handler));
    } else if (std::holds_alternative<std::unique_ptr<PointSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PointSelectionHandler>>(selection_handler));
    } else {
        std::cout << "GenericPointVisualization::applySelection: selection_handler is not a PolygonSelectionHandler" << std::endl;
    }
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::applySelection(PolygonSelectionHandler const & selection_handler) {
    if (!m_selected_points.empty()) {
        clearSelection();
    }

    auto const & region = selection_handler.getActiveSelectionRegion();

    float min_x, min_y, max_x, max_y;
    region->getBoundingBox(min_x, min_y, max_x, max_y);
    BoundingBox query_bounds(min_x, min_y, max_x, max_y);

    size_t total_points_added = 0;
    QString last_modified_key;

    // Query each visualization's QuadTree
    if (!m_visible || !m_spatial_index) {
        return;
    }

    std::vector<QuadTreePoint<RowIndicatorType> const *> candidate_points;
    m_spatial_index->queryPointers(query_bounds, candidate_points);

    size_t points_added_this_data = 0;
    for (auto const * point_ptr: candidate_points) {
        if (region->containsPoint(Point2D<float>(point_ptr->x, point_ptr->y))) {
            if (m_selected_points.find(point_ptr) == m_selected_points.end()) {
                m_selected_points.insert(point_ptr);
                points_added_this_data++;
            }
        }
    }

    if (points_added_this_data > 0) {
        updateSelectionVertexBuffer();
        total_points_added += points_added_this_data;
    }
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::applySelection(PointSelectionHandler const & selection_handler) {
    float tolerance = selection_handler.getWorldTolerance();
    QVector2D world_pos = selection_handler.getWorldPos();
    Qt::KeyboardModifiers modifiers = selection_handler.getModifiers();

    auto const * candidate = m_spatial_index->findNearest(world_pos.x(), world_pos.y(), tolerance);

    if (candidate) {
        if (modifiers & Qt::ControlModifier) {
            togglePointSelection(candidate);
        } else if (modifiers & Qt::ShiftModifier) {
            removePointFromSelection(candidate);
        }
    }
}

template<typename CoordType, typename RowIndicatorType>
QString GenericPointVisualization<CoordType, RowIndicatorType>::getTooltipText() const {
    if (!m_current_hover_point) {
        return QString();
    }

    return QString("Dataset: %1\nRow: %2\nPosition: (%3, %4)")
            .arg(m_key)
            .arg(QString::number(m_current_hover_point->data))
            .arg(m_current_hover_point->x, 0, 'f', 2)
            .arg(m_current_hover_point->y, 0, 'f', 2);
}

template<typename CoordType, typename RowIndicatorType>
bool GenericPointVisualization<CoordType, RowIndicatorType>::handleHover(QVector2D const & world_pos, float tolerance) {
    auto const * nearest_point = m_spatial_index->findNearest(world_pos.x(), world_pos.y(), tolerance);

    // Check if the nearest point is hidden
    if (nearest_point && m_hidden_points.find(nearest_point) != m_hidden_points.end()) {
        nearest_point = nullptr;// Treat hidden points as if they don't exist for hover
    }

    bool hover_changed = (m_current_hover_point != nearest_point);
    m_current_hover_point = nearest_point;
    return hover_changed;
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::clearHover() {
    m_current_hover_point = nullptr;
}

template<typename CoordType, typename RowIndicatorType>
std::optional<RowIndicatorType> GenericPointVisualization<CoordType, RowIndicatorType>::handleDoubleClick(QVector2D const & world_pos, float tolerance) {
    auto const * nearest_point = m_spatial_index->findNearest(world_pos.x(), world_pos.y(), tolerance);

    // Check if the nearest point is hidden
    if (nearest_point && m_hidden_points.find(nearest_point) != m_hidden_points.end()) {
        return std::nullopt;// Hidden points can't be double-clicked
    }

    if (nearest_point) {
        return nearest_point->data;
    }
    return std::nullopt;
}

//========== Visibility Management ==========

template<typename CoordType, typename RowIndicatorType>
size_t GenericPointVisualization<CoordType, RowIndicatorType>::hideSelectedPoints() {
    if (m_selected_points.empty()) {
        qDebug() << "GenericPointVisualization: No points selected for hiding";
        return 0;
    }

    size_t hidden_count = 0;

    // Add selected points to hidden set
    for (auto const * point_ptr: m_selected_points) {
        if (m_hidden_points.find(point_ptr) == m_hidden_points.end()) {
            m_hidden_points.insert(point_ptr);
            hidden_count++;
        }
    }

    // Clear selection since hidden points should not be selected
    m_selected_points.clear();

    // Update statistics
    m_hidden_point_count = m_hidden_points.size();

    // Update GPU buffers
    updateSelectionVertexBuffer();
    _updateVisibleVertexBuffer();

    qDebug() << "GenericPointVisualization: Hidden" << hidden_count
             << "points, total hidden:" << m_hidden_point_count;

    return hidden_count;
}

template<typename CoordType, typename RowIndicatorType>
size_t GenericPointVisualization<CoordType, RowIndicatorType>::showAllPoints() {
    size_t shown_count = m_hidden_points.size();

    m_hidden_points.clear();
    m_hidden_point_count = 0;

    // Update GPU vertex buffer to show all points
    _updateVisibleVertexBuffer();

    qDebug() << "GenericPointVisualization: Showed" << shown_count << "points, all points now visible";

    return shown_count;
}

template<typename CoordType, typename RowIndicatorType>
std::pair<size_t, size_t> GenericPointVisualization<CoordType, RowIndicatorType>::getVisibilityStats() const {
    return {m_total_point_count, m_hidden_point_count};
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::_updateVisibleVertexBuffer() {
    if (!m_spatial_index) {
        return;
    }

    // Rebuild vertex data with current visibility and group assignments
    m_vertex_data.clear();

    // Get all points from the spatial index
    std::vector<QuadTreePoint<RowIndicatorType> const *> all_points;
    BoundingBox full_bounds = m_spatial_index->getBounds();
    m_spatial_index->queryPointers(full_bounds, all_points);

    m_total_point_count = all_points.size();
    m_hidden_point_count = 0;

    for (auto const * point_ptr: all_points) {
        // Check if point is hidden or outside data range
        bool is_hidden = (m_hidden_points.find(point_ptr) != m_hidden_points.end());
        bool outside_data_range = (m_data_range_enabled &&
                                   (point_ptr->data < m_data_range_start || point_ptr->data > m_data_range_end));

        if (is_hidden || outside_data_range) {
            m_hidden_point_count++;
        }

        // Add all points to vertex data (hidden points will be filtered in shader if needed)
        m_vertex_data.push_back(point_ptr->x);
        m_vertex_data.push_back(point_ptr->y);

        // Get group ID for this point - use EntityId-based method if RowIndicatorType is EntityId
        int group_id = -1;
        if (m_group_manager) {
            if constexpr (std::is_same_v<RowIndicatorType, EntityId>) {
                group_id = m_group_manager->getEntityGroup(point_ptr->data);
            } else {
                // For non-EntityId types, we can't directly get group membership
                // This would need to be handled differently based on your specific use case
                group_id = -1;
            }
        }
        m_vertex_data.push_back(static_cast<float>(group_id == -1 ? 0 : group_id));
    }

    m_visible_vertex_count = m_vertex_data.size();

    // Update the OpenGL vertex buffer
    m_vertex_buffer.bind();
    m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
    m_vertex_buffer.release();

    qDebug() << "GenericPointVisualization: Updated vertex buffer with" << m_total_point_count
             << "total points (" << m_hidden_point_count << "hidden)";
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::setDataRangeEnabled(bool enabled) {
    qDebug() << "GenericPointVisualization::setDataRangeEnabled(" << enabled << ")";

    if (m_data_range_enabled != enabled) {
        m_data_range_enabled = enabled;

        // Update visibility mask to apply or remove data range filtering
        _updateVisibleVertexBuffer();

        qDebug() << "Data range filtering" << (enabled ? "enabled" : "disabled");
    }
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::setDataRange(RowIndicatorType start_value, RowIndicatorType end_value) {
    qDebug() << "GenericPointVisualization::setDataRange(" << start_value << "," << end_value << ")";

    m_data_range_start = start_value;
    m_data_range_end = end_value;

    // Update visibility mask to apply data range filtering
    _updateVisibleVertexBuffer();

    qDebug() << "Data range updated and visibility mask refreshed";
}

//========== Group Management ==========

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::setGroupManager(GroupManager * group_manager) {
    m_group_manager = group_manager;

    if (m_group_manager) {
        // Initial refresh of group data
        refreshGroupRenderData();
    }
}

template<typename CoordType, typename RowIndicatorType>
std::unordered_set<RowIndicatorType> GenericPointVisualization<CoordType, RowIndicatorType>::getSelectedPointIds() const {
    std::unordered_set<RowIndicatorType> point_ids;

    for (auto const * point_ptr: m_selected_points) {
        point_ids.insert(point_ptr->data);// data field contains the row indicator
    }

    return point_ids;
}

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::refreshGroupRenderData() {
    if (!m_group_manager) {
        return;
    }

    m_group_data_needs_update = true;
    _updateGroupVertexData();
}

//========== Private Methods ==========

template<typename CoordType, typename RowIndicatorType>
void GenericPointVisualization<CoordType, RowIndicatorType>::_updateGroupVertexData() {
    if (!m_group_manager) {
        return;
    }

    // Create mapping from group ID to shader color index
    auto const & groups = m_group_manager->getGroups();
    std::unordered_map<int, int> group_id_to_color_index;
    int color_index = 1;// Start from index 1 (0 is for ungrouped)
    for (auto it = groups.begin(); it != groups.end() && color_index < 256; ++it) {
        group_id_to_color_index[it.key()] = color_index;
        color_index++;
    }

    bool can_use_entity_ids = (m_entity_ids.size() == m_total_point_count);

    if (can_use_entity_ids) {
        // Fast path: use per-vertex EntityIds to compute group palette indices in O(N)
        for (size_t i = 0; i < m_total_point_count; ++i) {
            int gid = m_group_manager->getEntityGroup(m_entity_ids[i]);
            float shader_group_id = 0.0f;
            if (gid != -1) {
                auto it = group_id_to_color_index.find(gid);
                if (it != group_id_to_color_index.end()) shader_group_id = static_cast<float>(it->second);
            }
            m_vertex_data[i * 3 + 2] = shader_group_id;
        }
    } else if (m_spatial_index) {
        // Fallback path: derive using spatial index (legacy behavior)
        for (size_t i = 0; i < m_vertex_data.size(); i += 3) {
            float x = m_vertex_data[i];
            float y = m_vertex_data[i + 1];
            auto const * point_ptr = m_spatial_index->findNearest(x, y, 0.0001f);
            int group_id = -1;
            if (point_ptr && m_group_manager) {
                if constexpr (std::is_same_v<RowIndicatorType, EntityId>) {
                    group_id = m_group_manager->getEntityGroup(point_ptr->data);
                } else {
                    // For non-EntityId types, we can't directly get group membership
                    group_id = -1;
                }
            }
            float shader_group_id = 0.0f;
            if (group_id != -1) {
                auto it = group_id_to_color_index.find(group_id);
                if (it != group_id_to_color_index.end()) shader_group_id = static_cast<float>(it->second);
            }
            m_vertex_data[i + 2] = shader_group_id;
        }
    }

    // Update OpenGL buffer
    if (m_vertex_buffer.isCreated()) {
        m_vertex_buffer.bind();
        m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
        m_vertex_buffer.release();
    }

    m_group_data_needs_update = false;
}

#endif// GENERICPOINTVISUALIZATION_HPP 
