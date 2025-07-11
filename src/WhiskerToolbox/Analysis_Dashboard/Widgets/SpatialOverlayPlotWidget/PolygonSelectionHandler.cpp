#include "PolygonSelectionHandler.hpp"

#include <QDebug>
#include <QOpenGLFunctions>

PolygonSelectionHandler::PolygonSelectionHandler(
        RequestUpdateCallback request_update_callback,
        ScreenToWorldCallback screen_to_world_callback,
        ApplySelectionRegionCallback apply_selection_region_callback)
    : _request_update_callback(request_update_callback),
      _screen_to_world_callback(screen_to_world_callback),
      _apply_selection_region_callback(apply_selection_region_callback),
      _polygon_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      _polygon_line_buffer(QOpenGLBuffer::VertexBuffer),
      _opengl_resources_initialized(false),
      _is_polygon_selecting(false) {
}

PolygonSelectionHandler::~PolygonSelectionHandler() {
    cleanupOpenGLResources();
}

void PolygonSelectionHandler::setCallbacks(
        RequestUpdateCallback request_update_callback,
        ScreenToWorldCallback screen_to_world_callback,
        ApplySelectionRegionCallback apply_selection_region_callback) {
    _request_update_callback = request_update_callback;
    _screen_to_world_callback = screen_to_world_callback;
    _apply_selection_region_callback = apply_selection_region_callback;
}

void PolygonSelectionHandler::initializeOpenGLResources() {
    if (_opengl_resources_initialized) {
        return;
    }

    // Initialize OpenGL functions
    if (!initializeOpenGLFunctions()) {
        qWarning() << "PolygonSelectionHandler: Failed to initialize OpenGL functions";
        return;
    }

    // Create polygon vertex array object and buffer
    _polygon_vertex_array_object.create();
    _polygon_vertex_array_object.bind();

    _polygon_vertex_buffer.create();
    _polygon_vertex_buffer.bind();
    _polygon_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Pre-allocate polygon vertex buffer (initially empty)
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Set up vertex attributes for polygon vertices
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _polygon_vertex_array_object.release();
    _polygon_vertex_buffer.release();

    // Create polygon line array object and buffer
    _polygon_line_array_object.create();
    _polygon_line_array_object.bind();

    _polygon_line_buffer.create();
    _polygon_line_buffer.bind();
    _polygon_line_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Pre-allocate polygon line buffer (initially empty)
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Set up vertex attributes for polygon lines
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _polygon_line_array_object.release();
    _polygon_line_buffer.release();

    _opengl_resources_initialized = true;
    qDebug() << "PolygonSelectionHandler: OpenGL resources initialized successfully";
}

void PolygonSelectionHandler::cleanupOpenGLResources() {
    if (_opengl_resources_initialized) {
        _polygon_vertex_buffer.destroy();
        _polygon_vertex_array_object.destroy();

        _polygon_line_buffer.destroy();
        _polygon_line_array_object.destroy();

        _opengl_resources_initialized = false;
    }
}

void PolygonSelectionHandler::startPolygonSelection(int screen_x, int screen_y) {
    if (!_screen_to_world_callback) {
        qWarning() << "PolygonSelectionHandler: No screen to world callback set";
        return;
    }

    qDebug() << "PolygonSelectionHandler: Starting polygon selection at" << screen_x << "," << screen_y;

    _is_polygon_selecting = true;
    _polygon_vertices.clear();
    _polygon_screen_points.clear();

    // Add first vertex
    QVector2D world_pos = _screen_to_world_callback(screen_x, screen_y);
    _polygon_vertices.push_back(world_pos);
    _polygon_screen_points.push_back(QPoint(screen_x, screen_y));

    qDebug() << "PolygonSelectionHandler: Added first polygon vertex at world:" << world_pos.x() << "," << world_pos.y()
             << "screen:" << screen_x << "," << screen_y;

    // Update polygon buffers
    updatePolygonBuffers();

    if (_request_update_callback) {
        _request_update_callback();
    }
}

void PolygonSelectionHandler::addPolygonVertex(int screen_x, int screen_y) {
    if (!_is_polygon_selecting || !_screen_to_world_callback) {
        return;
    }

    QVector2D world_pos = _screen_to_world_callback(screen_x, screen_y);
    _polygon_vertices.push_back(world_pos);
    _polygon_screen_points.push_back(QPoint(screen_x, screen_y));

    qDebug() << "PolygonSelectionHandler: Added polygon vertex" << _polygon_vertices.size()
             << "at" << world_pos.x() << "," << world_pos.y();

    // Update polygon buffers
    updatePolygonBuffers();

    if (_request_update_callback) {
        _request_update_callback();
    }
}

void PolygonSelectionHandler::completePolygonSelection() {
    if (!_is_polygon_selecting || _polygon_vertices.size() < 3) {
        qDebug() << "PolygonSelectionHandler: Cannot complete polygon selection - insufficient vertices";
        cancelPolygonSelection();
        return;
    }

    if (!_apply_selection_region_callback) {
        qWarning() << "PolygonSelectionHandler: No apply selection region callback set";
        return;
    }

    qDebug() << "PolygonSelectionHandler: Completing polygon selection with"
             << _polygon_vertices.size() << "vertices";

    // Create selection region and apply it
    auto polygon_region = std::make_unique<PolygonSelectionRegion>(_polygon_vertices);
    _apply_selection_region_callback(*polygon_region, false);// Replace existing selection

    // Store the active region for future use if needed
    _active_selection_region = std::move(polygon_region);

    // Clean up polygon selection state
    _is_polygon_selecting = false;
    _polygon_vertices.clear();
    _polygon_screen_points.clear();

    if (_request_update_callback) {
        _request_update_callback();
    }
}

void PolygonSelectionHandler::cancelPolygonSelection() {
    qDebug() << "PolygonSelectionHandler: Cancelling polygon selection";

    _is_polygon_selecting = false;
    _polygon_vertices.clear();
    _polygon_screen_points.clear();

    // Clear polygon buffers
    if (_opengl_resources_initialized) {
        _polygon_vertex_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        _polygon_vertex_buffer.release();

        _polygon_line_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        _polygon_line_buffer.release();
    }

    if (_request_update_callback) {
        _request_update_callback();
    }
}

void PolygonSelectionHandler::renderPolygonOverlay(QOpenGLShaderProgram * line_shader_program, QMatrix4x4 const & mvp_matrix) {
    if (!_is_polygon_selecting || _polygon_vertices.empty() || !line_shader_program) {
        return;
    }

    qDebug() << "PolygonSelectionHandler: Rendering polygon overlay with" << _polygon_vertices.size() << "vertices";

    // Use line shader program
    if (!line_shader_program->bind()) {
        qDebug() << "PolygonSelectionHandler: Failed to bind line shader program";
        return;
    }

    // Set uniform matrices
    line_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);

    // Enable line smoothing
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Set line width
    glLineWidth(2.0f);

    // === DRAW CALL 1: Render polygon vertices as points ===
    _polygon_vertex_array_object.bind();
    _polygon_vertex_buffer.bind();

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for vertices (red points)
    line_shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.0f, 0.0f, 1.0f));// Red
    line_shader_program->setUniformValue("u_point_size", 8.0f);

    // Draw vertices as points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(_polygon_vertices.size()));

    _polygon_vertex_buffer.release();
    _polygon_vertex_array_object.release();

    // === DRAW CALL 2: Render polygon lines ===
    if (_polygon_vertices.size() >= 2) {
        _polygon_line_array_object.bind();
        _polygon_line_buffer.bind();

        // Set vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        // Set uniforms for lines (blue dashed appearance)
        line_shader_program->setUniformValue("u_color", QVector4D(0.2f, 0.6f, 1.0f, 1.0f));// Blue

        // Draw lines
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>((_polygon_vertices.size() - 1) * 2));

        // Draw closure line if we have 3+ vertices
        if (_polygon_vertices.size() >= 3) {
            // Draw closure line with different color
            line_shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.6f, 0.2f, 1.0f));// Orange
            glDrawArrays(GL_LINES, static_cast<GLint>((_polygon_vertices.size() - 1) * 2), 2);
        }

        _polygon_line_buffer.release();
        _polygon_line_array_object.release();
    }

    // Reset line width
    glLineWidth(1.0f);
    glDisable(GL_LINE_SMOOTH);

    line_shader_program->release();

    qDebug() << "PolygonSelectionHandler: Finished rendering polygon overlay";
}

void PolygonSelectionHandler::updatePolygonBuffers() {
    if (!_opengl_resources_initialized || _polygon_vertices.empty()) {
        return;
    }

    // Update vertex buffer (for drawing vertices as points)
    std::vector<float> vertex_data;
    vertex_data.reserve(_polygon_vertices.size() * 2);

    for (auto const & vertex: _polygon_vertices) {
        vertex_data.push_back(vertex.x());
        vertex_data.push_back(vertex.y());
    }

    _polygon_vertex_array_object.bind();
    _polygon_vertex_buffer.bind();
    _polygon_vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _polygon_vertex_buffer.release();
    _polygon_vertex_array_object.release();

    // Update line buffer (for drawing lines between vertices)
    if (_polygon_vertices.size() >= 2) {
        std::vector<float> line_data;
        line_data.reserve((_polygon_vertices.size() * 2 + 2) * 2);// Extra space for closure line

        // Add lines between consecutive vertices
        for (size_t i = 1; i < _polygon_vertices.size(); ++i) {
            // Line from previous vertex to current vertex
            line_data.push_back(_polygon_vertices[i - 1].x());
            line_data.push_back(_polygon_vertices[i - 1].y());
            line_data.push_back(_polygon_vertices[i].x());
            line_data.push_back(_polygon_vertices[i].y());
        }

        // Add closure line if we have 3+ vertices
        if (_polygon_vertices.size() >= 3) {
            line_data.push_back(_polygon_vertices.back().x());
            line_data.push_back(_polygon_vertices.back().y());
            line_data.push_back(_polygon_vertices.front().x());
            line_data.push_back(_polygon_vertices.front().y());
        }

        _polygon_line_array_object.bind();
        _polygon_line_buffer.bind();
        _polygon_line_buffer.allocate(line_data.data(), static_cast<int>(line_data.size() * sizeof(float)));

        // Set vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        _polygon_line_buffer.release();
        _polygon_line_array_object.release();
    }
}
