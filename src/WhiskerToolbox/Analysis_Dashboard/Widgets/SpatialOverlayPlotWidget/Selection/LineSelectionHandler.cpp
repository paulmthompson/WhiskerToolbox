#include "LineSelectionHandler.hpp"

#include "ShaderManager/ShaderManager.hpp"

#include <QDebug>
#include <QKeyEvent>
#include <QOpenGLShaderProgram>
#include <QMouseEvent>

LineSelectionHandler::LineSelectionHandler()
    : _line_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      _is_drawing_line(false) {

    initializeOpenGLFunctions();
        
    initializeOpenGLResources();
}

LineSelectionHandler::~LineSelectionHandler() {
    cleanupOpenGLResources();
}

void LineSelectionHandler::setNotificationCallback(NotificationCallback callback) {
    _notification_callback = callback;
}

void LineSelectionHandler::clearNotificationCallback() {
    _notification_callback = nullptr;
}

void LineSelectionHandler::initializeOpenGLResources() {

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

    // Create line vertex array object and buffer
    _line_vertex_array_object.create();
    _line_vertex_array_object.bind();

    _line_vertex_buffer.create();
    _line_vertex_buffer.bind();
    _line_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Pre-allocate line vertex buffer for two points (4 floats: start_x, start_y, end_x, end_y)
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Set up vertex attributes for line vertices
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _line_vertex_array_object.release();
    _line_vertex_buffer.release();

    qDebug() << "LineSelectionHandler: OpenGL resources initialized successfully";
}

void LineSelectionHandler::cleanupOpenGLResources() {
    _line_vertex_buffer.destroy();
    _line_vertex_array_object.destroy();
}

void LineSelectionHandler::startLineSelection(float world_x, float world_y) {

    qDebug() << "LineSelectionHandler: Starting line selection at" << world_x << "," << world_y;

    _is_drawing_line = true;
    _line_start_point = Point2D<float>(world_x, world_y);
    _line_end_point = _line_start_point; // Initially end point is same as start point

    qDebug() << "LineSelectionHandler: Added first line point at world:" << world_x << "," << world_y;

    // Update line buffer
    updateLineBuffer();
}

void LineSelectionHandler::updateLineEndPoint(float world_x, float world_y) {
    if (!_is_drawing_line) {
        return;
    }

    _line_end_point = Point2D<float>(world_x, world_y);

    qDebug() << "LineSelectionHandler: Updated line end point to" << world_x << "," << world_y;

    // Update line buffer
    updateLineBuffer();
}

void LineSelectionHandler::completeLineSelection() {
    if (!_is_drawing_line) {
        qDebug() << "LineSelectionHandler: Cannot complete line selection - not currently drawing";
        cancelLineSelection();
        return;
    }

    qDebug() << "LineSelectionHandler: Completing line selection from" 
             << _line_start_point.x << "," << _line_start_point.y 
             << "to" << _line_end_point.x << "," << _line_end_point.y;

    // Create selection region and apply it
    auto line_region = std::make_unique<LineSelectionRegion>(_line_start_point, _line_end_point);
    _active_selection_region = std::move(line_region);

    // Call notification callback if set
    if (_notification_callback) {
        _notification_callback();
    }

    // Clean up line selection state
    _is_drawing_line = false;
}

void LineSelectionHandler::cancelLineSelection() {
    qDebug() << "LineSelectionHandler: Cancelling line selection";

    _is_drawing_line = false;

    // Clear line buffer
    _line_vertex_buffer.bind();
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    _line_vertex_buffer.release();
}

void LineSelectionHandler::render(QMatrix4x4 const & mvp_matrix) {
    if (!_is_drawing_line) {
        return;
    }

    ShaderManager & shader_manager = ShaderManager::instance();
    _line_shader_program = shader_manager.getProgram("line")->getNativeProgram();

    qDebug() << "LineSelectionHandler: Rendering line overlay from" 
             << _line_start_point.x << "," << _line_start_point.y 
             << "to" << _line_end_point.x << "," << _line_end_point.y;

    // Use line shader program
    if (!_line_shader_program->bind()) {
        qDebug() << "LineSelectionHandler: Failed to bind line shader program";
        return;
    }

    // Set uniform matrices
    _line_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);

    // Enable line smoothing
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Set line width
    glLineWidth(2.0f);

    // === DRAW CALL: Render line ===
    _line_vertex_array_object.bind();
    _line_vertex_buffer.bind();

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for line (black solid line)
    _line_shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f)); // Black

    // Disable blending for solid black line
    glDisable(GL_BLEND);

    // Draw the line
    glDrawArrays(GL_LINES, 0, 2);

    // Re-enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Reset line width
    glLineWidth(1.0f);
    glDisable(GL_LINE_SMOOTH);

    _line_vertex_buffer.release();
    _line_vertex_array_object.release();
    _line_shader_program->release();

    qDebug() << "LineSelectionHandler: Finished rendering line overlay";
}

void LineSelectionHandler::mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (event->button() == Qt::LeftButton) {
        if (!_is_drawing_line) {
            startLineSelection(world_pos.x(), world_pos.y());
        }
    } else if (event->button() == Qt::RightButton) {
        if (_is_drawing_line) {
            completeLineSelection();
        }
    }
}

void LineSelectionHandler::mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (_is_drawing_line && (event->buttons() & Qt::LeftButton)) {
        qDebug() << "LineSelectionHandler: Updating line end point to" << world_pos.x() << "," << world_pos.y();
        updateLineEndPoint(world_pos.x(), world_pos.y());
    }
}

void LineSelectionHandler::mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (_is_drawing_line && (event->button() == Qt::LeftButton)) {
        qDebug() << "LineSelectionHandler: Completing line selection";
        completeLineSelection();
    }
}

void LineSelectionHandler::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
        cancelLineSelection();
    }
}

void LineSelectionHandler::deactivate() {
    cancelLineSelection();
}

void LineSelectionHandler::updateLineBuffer() {
    if (!_is_drawing_line) {
        return;
    }

    // Update line buffer with current line data
    std::vector<float> line_data = {
        _line_start_point.x, _line_start_point.y,
        _line_end_point.x, _line_end_point.y
    };

    _line_vertex_array_object.bind();
    _line_vertex_buffer.bind();
    _line_vertex_buffer.allocate(line_data.data(), static_cast<int>(line_data.size() * sizeof(float)));

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _line_vertex_buffer.release();
    _line_vertex_array_object.release();
}

// LineSelectionRegion implementation

LineSelectionRegion::LineSelectionRegion(Point2D<float> const & start_point, Point2D<float> const & end_point)
    : _start_point(start_point), _end_point(end_point) {
}

bool LineSelectionRegion::containsPoint(Point2D<float> point) const {
    // For line selection, we'll use a simple distance-based approach
    // A point is "contained" if it's within a certain distance of the line
    
    const float tolerance = 5.0f; // 5 pixel tolerance
    
    // Calculate distance from point to line segment
    float dx = _end_point.x - _start_point.x;
    float dy = _end_point.y - _start_point.y;
    
    if (dx == 0.0f && dy == 0.0f) {
        // Line is actually a point, check distance to that point
        float distance = std::sqrt(std::pow(point.x - _start_point.x, 2) + 
                                   std::pow(point.y - _start_point.y, 2));
        return distance <= tolerance;
    }
    
    // Calculate the closest point on the line segment to the given point
    float t = ((point.x - _start_point.x) * dx + (point.y - _start_point.y) * dy) / (dx * dx + dy * dy);
    
    // Clamp t to [0, 1] to stay within the line segment
    t = std::max(0.0f, std::min(1.0f, t));
    
    // Calculate the closest point on the line segment
    float closest_x = _start_point.x + t * dx;
    float closest_y = _start_point.y + t * dy;
    
    // Calculate distance from point to closest point on line
    float distance = std::sqrt(std::pow(point.x - closest_x, 2) + 
                               std::pow(point.y - closest_y, 2));
    
    return distance <= tolerance;
}

void LineSelectionRegion::getBoundingBox(float & min_x, float & min_y, float & max_x, float & max_y) const {
    min_x = std::min(_start_point.x, _end_point.x);
    min_y = std::min(_start_point.y, _end_point.y);
    max_x = std::max(_start_point.x, _end_point.x);
    max_y = std::max(_start_point.y, _end_point.y);
} 