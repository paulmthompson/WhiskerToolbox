#include "PointDataVisualization.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp"

#include <QOpenGLShaderProgram>

PointDataVisualization::PointDataVisualization(QString const & data_key,
                                               std::shared_ptr<PointData> const & point_data)
    : key(data_key),
      vertex_buffer(QOpenGLBuffer::VertexBuffer),
      selection_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      color(1.0f, 0.0f, 0.0f, 1.0f) {
    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = calculateBoundsForPointData(point_data.get());

    spatial_index = std::make_unique<QuadTree<int64_t>>(bounds);
    vertex_data.reserve(point_data->GetAllPointsAsRange().size() * 2);// Reserve space for x and y coordinates

    for (auto const & time_points_pair: point_data->GetAllPointsAsRange()) {
        for (auto const & point: time_points_pair.points) {
            // Store original coordinates in QuadTree (preserve data structure)
            spatial_index->insert(point.x, point.y, time_points_pair.time.getValue());

            // Store original coordinates in vertex data for OpenGL rendering
            vertex_data.push_back(point.x);
            vertex_data.push_back(point.y);
        }
    }

    initializeOpenGLResources();
}

PointDataVisualization::~PointDataVisualization() {
    cleanupOpenGLResources();
}

void PointDataVisualization::initializeOpenGLResources() {

    if (!initializeOpenGLFunctions()) {
        return;
    }


    vertex_array_object.create();
    vertex_array_object.bind();

    vertex_buffer.create();
    vertex_buffer.bind();
    vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    vertex_buffer.release();
    vertex_array_object.release();

    // Initialize selection vertex buffer
    selection_vertex_array_object.create();
    selection_vertex_array_object.bind();

    selection_vertex_buffer.create();
    selection_vertex_buffer.bind();
    selection_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    selection_vertex_buffer.release();
    selection_vertex_array_object.release();
}

void PointDataVisualization::cleanupOpenGLResources() {
    if (vertex_buffer.isCreated()) {
        vertex_buffer.destroy();
    }
    if (vertex_array_object.isCreated()) {
        vertex_array_object.destroy();
    }
    if (selection_vertex_buffer.isCreated()) {
        selection_vertex_buffer.destroy();
    }
    if (selection_vertex_array_object.isCreated()) {
        selection_vertex_array_object.destroy();
    }
}

void PointDataVisualization::updateSelectionVertexBuffer() {
    selection_vertex_data.clear();

    if (selected_points.empty()) {
        // Clear the buffer if no selection
        selection_vertex_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        selection_vertex_buffer.release();
        return;
    }

    // Prepare vertex data for selected points
    selection_vertex_data.reserve(selected_points.size() * 2);

    for (auto const * point_ptr: selected_points) {
        // Store original coordinates in selection vertex data
        selection_vertex_data.push_back(point_ptr->x);
        selection_vertex_data.push_back(point_ptr->y);
    }

    // Update the buffer
    selection_vertex_array_object.bind();
    selection_vertex_buffer.bind();
    selection_vertex_buffer.allocate(selection_vertex_data.data(),
                                     static_cast<int>(selection_vertex_data.size() * sizeof(float)));

    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    selection_vertex_buffer.release();
    selection_vertex_array_object.release();
}

void PointDataVisualization::clearSelection() {
    if (!selected_points.empty()) {
        selected_points.clear();
        updateSelectionVertexBuffer();
    }
}

bool PointDataVisualization::togglePointSelection(QuadTreePoint<int64_t> const * point_ptr) {
    auto it = selected_points.find(point_ptr);

    if (it != selected_points.end()) {
        // Point is selected, remove it
        selected_points.erase(it);
        updateSelectionVertexBuffer();
        return false;// Point was deselected
    } else {
        // Point is not selected, add it
        selected_points.insert(point_ptr);
        updateSelectionVertexBuffer();
        return true;// Point was selected
    }
}

bool PointDataVisualization::removePointFromSelection(QuadTreePoint<int64_t> const * point_ptr) {
    auto it = selected_points.find(point_ptr);

    if (it != selected_points.end()) {
        // Point is selected, remove it
        selected_points.erase(it);
        updateSelectionVertexBuffer();
        return true;// Point was removed
    }

    return false;// Point wasn't selected
}

void PointDataVisualization::renderPoints(QOpenGLShaderProgram * shader_program, float point_size) {
    if (!visible || vertex_data.empty()) return;

    vertex_array_object.bind();
    vertex_buffer.bind();

    // Set color for this PointData
    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_point_size", point_size);

    // Draw points for this PointData
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertex_data.size() / 2));

    vertex_buffer.release();
    vertex_array_object.release();
}

void PointDataVisualization::renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size) {
    if (selected_points.empty()) return;

    selection_vertex_array_object.bind();
    selection_vertex_buffer.bind();

    // Set uniforms for selection rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    shader_program->setUniformValue("u_point_size", point_size * 1.5f);           // Slightly larger

    // Draw the selected points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(selected_points.size()));

    selection_vertex_buffer.release();
    selection_vertex_array_object.release();
}

void PointDataVisualization::renderHoverPoint(QOpenGLShaderProgram * shader_program,
                                              float point_size,
                                              QOpenGLBuffer & highlight_buffer,
                                              QOpenGLVertexArrayObject & highlight_vao) {
    if (!current_hover_point) return;

    highlight_vao.bind();
    highlight_buffer.bind();

    // Store original coordinates in hover point data
    float highlight_data[2] = {current_hover_point->x, current_hover_point->y};
    glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), highlight_data);

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for highlight rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    shader_program->setUniformValue("u_point_size", point_size * 2.5f);           // Larger size

    // Draw the highlighted point
    glDrawArrays(GL_POINTS, 0, 1);

    highlight_buffer.release();
    highlight_vao.release();
}

BoundingBox PointDataVisualization::calculateBoundsForPointData(PointData const * point_data) const {
    if (!point_data) {
        return BoundingBox(0, 0, 0, 0);
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    bool has_points = false;

    for (auto const & time_points_pair: point_data->GetAllPointsAsRange()) {
        for (auto const & point: time_points_pair.points) {
            min_x = std::min(min_x, point.x);
            max_x = std::max(max_x, point.x);
            min_y = std::min(min_y, point.y);
            max_y = std::max(max_y, point.y);
            has_points = true;
        }
    }

    if (!has_points) {
        return BoundingBox(0, 0, 0, 0);
    }

    // Add some padding
    float padding_x = (max_x - min_x) * 0.1f;
    float padding_y = (max_y - min_y) * 0.1f;

    return BoundingBox(min_x - padding_x, min_y - padding_y,
                       max_x + padding_x, max_y + padding_y);
}

void PointDataVisualization::applySelection(std::variant<std::unique_ptr<PolygonSelectionHandler>> const & selection_handler) {
    if (std::holds_alternative<std::unique_ptr<PolygonSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PolygonSelectionHandler>>(selection_handler));
    } else {
        std::cout << "PointDataVisualization::applySelection: selection_handler is not a PolygonSelectionHandler" << std::endl;
    }
}

void PointDataVisualization::applySelection(PolygonSelectionHandler const & selection_handler) {
    if (!selected_points.empty()) {
        clearSelection();
    }

    auto const & region = selection_handler.getActiveSelectionRegion();

    float min_x, min_y, max_x, max_y;
    region->getBoundingBox(min_x, min_y, max_x, max_y);
    BoundingBox query_bounds(min_x, min_y, max_x, max_y);

    size_t total_points_added = 0;
    QString last_modified_key;

    // Query each PointData's QuadTree
    if (!visible || !spatial_index) {
        return;
    }

    std::vector<QuadTreePoint<int64_t> const *> candidate_points;
    spatial_index->queryPointers(query_bounds, candidate_points);

    size_t points_added_this_data = 0;
    for (auto const * point_ptr: candidate_points) {
        if (region->containsPoint(Point2D<float>(point_ptr->x, point_ptr->y))) {
            if (selected_points.find(point_ptr) == selected_points.end()) {
                selected_points.insert(point_ptr);
                points_added_this_data++;
            }
        }
    }

    if (points_added_this_data > 0) {
        updateSelectionVertexBuffer();
        total_points_added += points_added_this_data;
    }
}