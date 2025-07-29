#include "PointDataVisualization.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"

#include "ShaderManager/ShaderManager.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/NoneSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PointSelectionHandler.hpp"

#include <QOpenGLShaderProgram>

PointDataVisualization::PointDataVisualization(QString const & data_key,
                                               std::shared_ptr<PointData> const & point_data)
    : m_key(data_key),
      m_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_selection_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_highlight_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      m_color(1.0f, 0.0f, 0.0f, 1.0f) {
    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = calculateBoundsForPointData(point_data.get());

    m_spatial_index = std::make_unique<QuadTree<int64_t>>(bounds);
    m_vertex_data.reserve(point_data->GetAllPointsAsRange().size() * 2);// Reserve space for x and y coordinates

    for (auto const & time_points_pair: point_data->GetAllPointsAsRange()) {
        for (auto const & point: time_points_pair.points) {
            // Store original coordinates in QuadTree (preserve data structure)
            m_spatial_index->insert(point.x, point.y, time_points_pair.time.getValue());

            // Store original coordinates in vertex data for OpenGL rendering
            m_vertex_data.push_back(point.x);
            m_vertex_data.push_back(point.y);
        }
    }

    // Initialize visibility statistics
    m_total_point_count = m_vertex_data.size() / 2;
    m_hidden_point_count = 0;
    m_visible_vertex_count = m_vertex_data.size();

    initializeOpenGLResources();
}

PointDataVisualization::~PointDataVisualization() {
    cleanupOpenGLResources();
}

void PointDataVisualization::initializeOpenGLResources() {

    if (!initializeOpenGLFunctions()) {
        return;
    }

    // Load point shader program from ShaderManager
    if (!ShaderManager::instance().loadProgram("point",
                                               ":/shaders/point.vert",
                                               ":/shaders/point.frag",
                                               "",
                                               ShaderSourceType::Resource)) {
        qDebug() << "PointDataVisualization: Failed to load point shader program";
        return;
    }


    m_vertex_array_object.create();
    m_vertex_array_object.bind();

    m_vertex_buffer.create();
    m_vertex_buffer.bind();
    m_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_vertex_buffer.release();
    m_vertex_array_object.release();

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

void PointDataVisualization::cleanupOpenGLResources() {
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

void PointDataVisualization::updateSelectionVertexBuffer() {
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

void PointDataVisualization::clearSelection() {
    if (!m_selected_points.empty()) {
        m_selected_points.clear();
        updateSelectionVertexBuffer();
    }
}

bool PointDataVisualization::togglePointSelection(QuadTreePoint<int64_t> const * point_ptr) {
    auto it = m_selected_points.find(point_ptr);

    if (it != m_selected_points.end()) {
        // Point is selected, remove it
        m_selected_points.erase(it);
        updateSelectionVertexBuffer();
        return false;// Point was deselected
    } else {
        // Point is not selected, add it
        m_selected_points.insert(point_ptr);
        updateSelectionVertexBuffer();
        return true;// Point was selected
    }
}

bool PointDataVisualization::removePointFromSelection(QuadTreePoint<int64_t> const * point_ptr) {
    auto it = m_selected_points.find(point_ptr);

    if (it != m_selected_points.end()) {
        // Point is selected, remove it
        m_selected_points.erase(it);
        updateSelectionVertexBuffer();
        return true;// Point was removed
    }

    return false;// Point wasn't selected
}

void PointDataVisualization::render(QMatrix4x4 const & mvp_matrix, float point_size) {
    auto pointProgram = ShaderManager::instance().getProgram("point");
    if (!pointProgram || !pointProgram->getNativeProgram()->bind()) {
        qDebug() << "PointDataVisualization: Failed to bind point shader program";
        return;
    }

    pointProgram->getNativeProgram()->setUniformValue("u_mvp_matrix", mvp_matrix);

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

void PointDataVisualization::_renderPoints(QOpenGLShaderProgram * shader_program, float point_size) {
    if (!m_visible || m_vertex_data.empty()) return;

    m_vertex_array_object.bind();
    m_vertex_buffer.bind();

    // Set color for this PointData
    shader_program->setUniformValue("u_color", m_color);
    shader_program->setUniformValue("u_point_size", point_size);

    // Draw only the visible points (those currently in the vertex buffer)
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_visible_vertex_count / 2));

    m_vertex_buffer.release();
    m_vertex_array_object.release();
}

void PointDataVisualization::_renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size) {
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

void PointDataVisualization::_renderHoverPoint(QOpenGLShaderProgram * shader_program,
                                              float point_size) {
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

void PointDataVisualization::applySelection(SelectionVariant & selection_handler) {
    if (std::holds_alternative<std::unique_ptr<PolygonSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PolygonSelectionHandler>>(selection_handler));
    } else if (std::holds_alternative<std::unique_ptr<PointSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PointSelectionHandler>>(selection_handler));
    }
    else {
        std::cout << "PointDataVisualization::applySelection: selection_handler is not a PolygonSelectionHandler" << std::endl;
    }
}

void PointDataVisualization::applySelection(PolygonSelectionHandler const & selection_handler) {
    if (!m_selected_points.empty()) {
        clearSelection();
    }

    auto const & region = selection_handler.getActiveSelectionRegion();

    float min_x, min_y, max_x, max_y;
    region->getBoundingBox(min_x, min_y, max_x, max_y);
    BoundingBox query_bounds(min_x, min_y, max_x, max_y);

    size_t total_points_added = 0;
    QString last_modified_key;

    // Query each PointData's QuadTree
    if (!m_visible || !m_spatial_index) {
        return;
    }

    std::vector<QuadTreePoint<int64_t> const *> candidate_points;
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

void PointDataVisualization::applySelection(PointSelectionHandler const & selection_handler) {
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

QString PointDataVisualization::getTooltipText() const {
    if (!m_current_hover_point) {
        return QString();
    }

    return QString("Dataset: %1\nInterval: %2\nPosition: (%3, %4)")
            .arg(m_key)
            .arg(m_current_hover_point->data)
            .arg(m_current_hover_point->x, 0, 'f', 2)
            .arg(m_current_hover_point->y, 0, 'f', 2);
}

bool PointDataVisualization::handleHover(const QVector2D & world_pos, float tolerance) {
    auto const * nearest_point = m_spatial_index->findNearest(world_pos.x(), world_pos.y(), tolerance);
    
    // Check if the nearest point is hidden
    if (nearest_point && m_hidden_points.find(nearest_point) != m_hidden_points.end()) {
        nearest_point = nullptr; // Treat hidden points as if they don't exist for hover
    }
    
    bool hover_changed = (m_current_hover_point != nearest_point);
    m_current_hover_point = nearest_point;
    return hover_changed;
}

void PointDataVisualization::clearHover() {
    m_current_hover_point = nullptr;
}

std::optional<int64_t> PointDataVisualization::handleDoubleClick(const QVector2D & world_pos, float tolerance) {
    auto const * nearest_point = m_spatial_index->findNearest(world_pos.x(), world_pos.y(), tolerance);
    
    // Check if the nearest point is hidden
    if (nearest_point && m_hidden_points.find(nearest_point) != m_hidden_points.end()) {
        return std::nullopt; // Hidden points can't be double-clicked
    }
    
    if (nearest_point) {
        return nearest_point->data;
    }
    return std::nullopt;
}

//========== Visibility Management ==========

size_t PointDataVisualization::hideSelectedPoints() {
    if (m_selected_points.empty()) {
        qDebug() << "PointDataVisualization: No points selected for hiding";
        return 0;
    }
    
    size_t hidden_count = 0;
    
    // Add selected points to hidden set
    for (const auto* point_ptr : m_selected_points) {
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
    
    qDebug() << "PointDataVisualization: Hidden" << hidden_count 
             << "points, total hidden:" << m_hidden_point_count;
    
    return hidden_count;
}

size_t PointDataVisualization::showAllPoints() {
    size_t shown_count = m_hidden_points.size();
    
    m_hidden_points.clear();
    m_hidden_point_count = 0;
    
    // Update GPU vertex buffer to show all points
    _updateVisibleVertexBuffer();
    
    qDebug() << "PointDataVisualization: Showed" << shown_count << "points, all points now visible";
    
    return shown_count;
}

std::pair<size_t, size_t> PointDataVisualization::getVisibilityStats() const {
    return {m_total_point_count, m_hidden_point_count};
}

void PointDataVisualization::_updateVisibleVertexBuffer() {
    if (!m_spatial_index) {
        return;
    }
    
    // Create new vertex data excluding hidden points
    std::vector<float> visible_vertex_data;
    
    // Get all points from the spatial index and filter out hidden ones
    std::vector<QuadTreePoint<int64_t> const *> all_points;
    BoundingBox full_bounds = m_spatial_index->getBounds();
    m_spatial_index->queryPointers(full_bounds, all_points);
    
    m_total_point_count = all_points.size();
    
    for (const auto* point_ptr : all_points) {
        // Skip hidden points
        if (m_hidden_points.find(point_ptr) == m_hidden_points.end()) {
            visible_vertex_data.push_back(point_ptr->x);
            visible_vertex_data.push_back(point_ptr->y);
        }
    }
    
    // Update the visible vertex count
    m_visible_vertex_count = visible_vertex_data.size();
    
    // Update the vertex buffer with only visible points
    m_vertex_buffer.bind();
    m_vertex_buffer.allocate(visible_vertex_data.data(), static_cast<int>(visible_vertex_data.size() * sizeof(float)));
    m_vertex_buffer.release();
    
    qDebug() << "PointDataVisualization: Updated vertex buffer with" << (m_visible_vertex_count / 2) 
             << "visible points out of" << m_total_point_count << "total points";
}