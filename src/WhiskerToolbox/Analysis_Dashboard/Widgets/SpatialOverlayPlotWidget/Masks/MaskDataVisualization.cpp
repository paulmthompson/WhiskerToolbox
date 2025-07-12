#include "MaskDataVisualization.hpp"

#include "DataManager/Masks/Mask_Data.hpp"

#include <QOpenGLShaderProgram>

MaskDataVisualization::MaskDataVisualization(QString const & data_key,
                                             std::shared_ptr<MaskData> const & mask_data)
    : key(data_key),
      vertex_buffer(QOpenGLBuffer::VertexBuffer),
      selection_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      color(1.0f, 0.0f, 0.0f, 1.0f) {
    BoundingBox bounds = calculateBoundsForMaskData(mask_data.get());
    spatial_index = std::make_unique<RTree<int64_t>>(bounds);
    /*
    vertex_data.reserve(mask_data->size() * 2); // Reserve space for x and y coordinates
    for (auto const & mask : mask_data->GetAllMasksAsRange()) {
        vertex_data.push_back(mask.x);
        vertex_data.push_back(mask.y);
    
        spatial_index->insert(point.x, point.y, time_points_pair.time.getValue());
        }
    */
    initializeOpenGLResources();
}

MaskDataVisualization::~MaskDataVisualization() {
    cleanupOpenGLResources();
}

void MaskDataVisualization::initializeOpenGLResources() {

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

void MaskDataVisualization::cleanupOpenGLResources() {
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

void MaskDataVisualization::updateSelectionVertexBuffer() {
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

void MaskDataVisualization::clearSelection() {
    if (!selected_points.empty()) {
        selected_points.clear();
        updateSelectionVertexBuffer();
    }
}

bool MaskDataVisualization::toggleMaskSelection(RTreeEntry<int64_t> const * mask_ptr) {
    auto it = selected_masks.find(mask_ptr);

    if (it != selected_masks.end()) {
        // Mask is selected, remove it
        selected_masks.erase(it);
        updateSelectionVertexBuffer();
        return false;// Mask was deselected
    } else {
        // Mask is not selected, add it
        selected_masks.insert(mask_ptr);
        updateSelectionVertexBuffer();
        return true;// Mask was selected
    }
}

void MaskDataVisualization::renderMasks(QOpenGLShaderProgram * shader_program) {
    if (!visible || vertex_data.empty()) return;

    vertex_array_object.bind();
    vertex_buffer.bind();

    // Set color for this PointData
    shader_program->setUniformValue("u_color", color);

    // Draw points for this PointData
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertex_data.size() / 2));

    vertex_buffer.release();
    vertex_array_object.release();
}

void MaskDataVisualization::renderSelectedMasks(QOpenGLShaderProgram * shader_program) {
    if (selected_masks.empty()) return;

    selection_vertex_array_object.bind();
    selection_vertex_buffer.bind();

    // Set uniforms for selection rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black

    // Draw the selected points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(selected_masks.size()));

    selection_vertex_buffer.release();
    selection_vertex_array_object.release();
}

void MaskDataVisualization::renderHoverMask(QOpenGLShaderProgram * shader_program,
                                              QOpenGLBuffer & highlight_buffer,
                                              QOpenGLVertexArrayObject & highlight_vao) {
    if (!current_hover_point) return;

    highlight_vao.bind();
    highlight_buffer.bind();

    // Set the highlight point data
    float highlight_data[2] = {current_hover_point->x, current_hover_point->y};
    glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), highlight_data);

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for highlight rendering
    shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black

    // Draw the highlighted point
    glDrawArrays(GL_POINTS, 0, 1);

    highlight_buffer.release();
    highlight_vao.release();
}

BoundingBox MaskDataVisualization::calculateBoundsForMaskData(MaskData const * mask_data) const {

    return BoundingBox(0, 0, 0, 0);

}
