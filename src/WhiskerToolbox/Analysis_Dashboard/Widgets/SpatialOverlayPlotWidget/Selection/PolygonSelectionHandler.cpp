#include "PolygonSelectionHandler.hpp"

#include "ShaderManager/ShaderManager.hpp"

#include <QDebug>
#include <QKeyEvent>
#include <QOpenGLShaderProgram>
#include <QMouseEvent>

PolygonSelectionHandler::PolygonSelectionHandler()
    : _polygon_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      _polygon_line_buffer(QOpenGLBuffer::VertexBuffer),
      _is_polygon_selecting(false) {

    initializeOpenGLFunctions();
        
    initializeOpenGLResources();

}

PolygonSelectionHandler::~PolygonSelectionHandler() {
    cleanupOpenGLResources();
}

void PolygonSelectionHandler::setNotificationCallback(NotificationCallback callback) {
    _notification_callback = callback;
}

void PolygonSelectionHandler::clearNotificationCallback() {
    _notification_callback = nullptr;
}

void PolygonSelectionHandler::initializeOpenGLResources() {

    ShaderManager & shader_manager = ShaderManager::instance();
    if (!shader_manager.getProgram("line")) {
        bool success = shader_manager.loadProgram("line",
                                                  ":/shaders/line.vert",
                                                  ":/shaders/line.frag",
                                                  "",
                                                  ShaderSourceType::Resource);
        if (!success) {
            qDebug() << "Failed to load line shader!";
        }
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

    qDebug() << "PolygonSelectionHandler: OpenGL resources initialized successfully";
}

void PolygonSelectionHandler::cleanupOpenGLResources() {
        _polygon_vertex_buffer.destroy();
        _polygon_vertex_array_object.destroy();

        _polygon_line_buffer.destroy();
        _polygon_line_array_object.destroy();
}

void PolygonSelectionHandler::startPolygonSelection(int world_x, int world_y) {

    qDebug() << "PolygonSelectionHandler: Starting polygon selection at" << world_x << "," << world_y;

    _is_polygon_selecting = true;
    _polygon_vertices.clear();

    // Add first vertex
    _polygon_vertices.push_back(Point2D<float>(world_x, world_y));

    qDebug() << "PolygonSelectionHandler: Added first polygon vertex at world:" << world_x << "," << world_y;

    // Update polygon buffers
    updatePolygonBuffers();

}

void PolygonSelectionHandler::addPolygonVertex(int world_x, int world_y) {
    if (!_is_polygon_selecting) {
        return;
    }

    _polygon_vertices.push_back(Point2D<float>(world_x, world_y));

    qDebug() << "PolygonSelectionHandler: Added polygon vertex" << _polygon_vertices.size()
             << "at" << world_x << "," << world_y;

    // Update polygon buffers
    updatePolygonBuffers();

}

void PolygonSelectionHandler::completePolygonSelection() {
    if (!_is_polygon_selecting || _polygon_vertices.size() < 3) {
        qDebug() << "PolygonSelectionHandler: Cannot complete polygon selection - insufficient vertices";
        cancelPolygonSelection();
        return;
    }

    qDebug() << "PolygonSelectionHandler: Completing polygon selection with"
             << _polygon_vertices.size() << "vertices";

    // Create selection region and apply it
    auto polygon_region = std::make_unique<PolygonSelectionRegion>(_polygon_vertices);
    _active_selection_region = std::move(polygon_region);

    // Call notification callback if set
    if (_notification_callback) {
        _notification_callback();
    }

    // Clean up polygon selection state
    _is_polygon_selecting = false;
    _polygon_vertices.clear();
}

void PolygonSelectionHandler::cancelPolygonSelection() {
    qDebug() << "PolygonSelectionHandler: Cancelling polygon selection";

    _is_polygon_selecting = false;
    _polygon_vertices.clear();

    // Clear polygon buffers

        _polygon_vertex_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        _polygon_vertex_buffer.release();

        _polygon_line_buffer.bind();
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        _polygon_line_buffer.release();
}

void PolygonSelectionHandler::render(QMatrix4x4 const & mvp_matrix) {
    if (!_is_polygon_selecting || _polygon_vertices.empty()) {
        return;
    }

    ShaderManager & shader_manager = ShaderManager::instance();
    _line_shader_program = shader_manager.getProgram("line")->getNativeProgram();

    qDebug() << "PolygonSelectionHandler: Rendering polygon overlay with" << _polygon_vertices.size() << "vertices";

    // Use line shader program
    if (!_line_shader_program->bind()) {
        qDebug() << "PolygonSelectionHandler: Failed to bind line shader program";
        return;
    }

    // Set uniform matrices
    _line_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);

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
    _line_shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.0f, 0.0f, 1.0f));// Red
    _line_shader_program->setUniformValue("u_point_size", 8.0f);

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
        _line_shader_program->setUniformValue("u_color", QVector4D(0.2f, 0.6f, 1.0f, 1.0f));// Blue

        // Draw lines
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>((_polygon_vertices.size() - 1) * 2));

        // Draw closure line if we have 3+ vertices
        if (_polygon_vertices.size() >= 3) {
            // Draw closure line with different color
            _line_shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.6f, 0.2f, 1.0f));// Orange
            glDrawArrays(GL_LINES, static_cast<GLint>((_polygon_vertices.size() - 1) * 2), 2);
        }

        _polygon_line_buffer.release();
        _polygon_line_array_object.release();
    }

    // Reset line width
    glLineWidth(1.0f);
    glDisable(GL_LINE_SMOOTH);

    _line_shader_program->release();

    qDebug() << "PolygonSelectionHandler: Finished rendering polygon overlay";
}

void PolygonSelectionHandler::mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (event->button() == Qt::LeftButton) {
        if (!isPolygonSelecting()) {
            startPolygonSelection(world_pos.x(), world_pos.y());
        } else {
            addPolygonVertex(world_pos.x(), world_pos.y());
        }
    }  else if (event->button() == Qt::RightButton) {
        if (isPolygonSelecting()) {
            completePolygonSelection();
        } 
    }
}

void PolygonSelectionHandler::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
        cancelPolygonSelection();
    }
}

void PolygonSelectionHandler::deactivate() {
    cancelPolygonSelection();
    //cleanupOpenGLResources();
}

void PolygonSelectionHandler::updatePolygonBuffers() {
    if (_polygon_vertices.empty()) {
        return;
    }

    // Update vertex buffer (for drawing vertices as points)
    std::vector<float> vertex_data;
    vertex_data.reserve(_polygon_vertices.size() * 2);

    for (auto const & vertex: _polygon_vertices) {
        vertex_data.push_back(vertex.x);
        vertex_data.push_back(vertex.y);
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
            line_data.push_back(_polygon_vertices[i - 1].x);
            line_data.push_back(_polygon_vertices[i - 1].y);
            line_data.push_back(_polygon_vertices[i].x);
            line_data.push_back(_polygon_vertices[i].y);
        }

        // Add closure line if we have 3+ vertices
        if (_polygon_vertices.size() >= 3) {
            line_data.push_back(_polygon_vertices.back().x);
            line_data.push_back(_polygon_vertices.back().y);
            line_data.push_back(_polygon_vertices.front().x);
            line_data.push_back(_polygon_vertices.front().y);
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

PolygonSelectionRegion::PolygonSelectionRegion(std::vector<Point2D<float>> const & vertices)
    : _polygon(vertices) {
}

bool PolygonSelectionRegion::containsPoint(Point2D<float> point) const {
    return _polygon.containsPoint(point);
}

void PolygonSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    auto bbox = _polygon.getBoundingBox();
    min_x = bbox.min_x;
    min_y = bbox.min_y;
    max_x = bbox.max_x;
    max_y = bbox.max_y;
}
