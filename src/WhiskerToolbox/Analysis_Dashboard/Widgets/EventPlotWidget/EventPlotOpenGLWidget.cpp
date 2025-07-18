#include "EventPlotOpenGLWidget.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QToolTip>
#include <QVector2D>
#include <QVector3D>
#include <QWheelEvent>

EventPlotOpenGLWidget::EventPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _shader_program(nullptr),
      _zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _mouse_pressed(false),
      _tooltips_enabled(true),
      _widget_width(1),
      _widget_height(1),
      _negative_range(30000),
      _positive_range(30000),
      _total_events(0),
      _tooltip_timer(nullptr) {
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);

    // Initialize tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay
    connect(_tooltip_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::handleTooltipTimer);
}

EventPlotOpenGLWidget::~EventPlotOpenGLWidget() {
    // OpenGL context must be current for cleanup
    makeCurrent();

    if (_shader_program) {
        delete _shader_program;
    }

    _vertex_buffer.destroy();
    _vertex_array_object.destroy();
    _highlight_vertex_buffer.destroy();
    _highlight_vertex_array_object.destroy();
    _center_line_buffer.destroy();
    _center_line_vertex_array_object.destroy();

    doneCurrent();
}

void EventPlotOpenGLWidget::setZoomLevel(float zoom_level) {
    if (_zoom_level != zoom_level) {
        _zoom_level = zoom_level;
        updateMatrices();
        update();
        emit zoomLevelChanged(_zoom_level);
    }
}

void EventPlotOpenGLWidget::setPanOffset(float offset_x, float offset_y) {
    if (_pan_offset_x != offset_x || _pan_offset_y != offset_y) {
        _pan_offset_x = offset_x;
        _pan_offset_y = offset_y;
        updateMatrices();
        update();
        emit panOffsetChanged(_pan_offset_x, _pan_offset_y);
    }
}

void EventPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    if (_tooltips_enabled != enabled) {
        _tooltips_enabled = enabled;
        emit tooltipsEnabledChanged(_tooltips_enabled);
    }
}

void EventPlotOpenGLWidget::setEventData(std::vector<std::vector<float>> const & event_data) {
    _event_data = event_data;
    qDebug() << "EventPlotOpenGLWidget::setEventData called with" << event_data.size() << "trials";
    for (size_t i = 0; i < event_data.size(); ++i) {
        qDebug() << "Trial" << i << "has" << event_data[i].size() << "events";
    }
    
    // TEMPORARY: Add a test event if no events exist
    if (_event_data.empty()) {
        qDebug() << "EventPlotOpenGLWidget::setEventData - adding test data";
        _event_data = {{100.0f, -50.0f, 200.0f}}; // Test events at known positions
    }
    
    updateVertexData();
    update();
}

QVector2D EventPlotOpenGLWidget::screenToWorld(int screen_x, int screen_y) const {
    // Convert screen coordinates to normalized device coordinates
    float ndc_x = (2.0f * screen_x) / _widget_width - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y) / _widget_height;

    // Apply inverse view transformation
    QMatrix4x4 inv_view = _view_matrix.inverted();
    QVector4D world_pos = inv_view * QVector4D(ndc_x, ndc_y, 0.0f, 1.0f);

    return QVector2D(world_pos.x(), world_pos.y());
}

void EventPlotOpenGLWidget::initializeGL() {
    qDebug() << "EventPlotOpenGLWidget::initializeGL called";
    
    initializeOpenGLFunctions();

    // Set clear color
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);// Light gray background

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Initialize shaders and buffers
    initializeShaders();
    initializeBuffers();
    updateMatrices();
    
    qDebug() << "EventPlotOpenGLWidget::initializeGL completed";
}

void EventPlotOpenGLWidget::paintGL() {
    qDebug() << "EventPlotOpenGLWidget::paintGL called";
    
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_shader_program || !_shader_program->isLinked()) {
        qDebug() << "EventPlotOpenGLWidget::paintGL - shader program not ready";
        return;
    }

    // Bind shader program
    _shader_program->bind();

    // Set transformation matrices
    _shader_program->setUniformValue("view_matrix", _view_matrix);
    _shader_program->setUniformValue("projection_matrix", _projection_matrix);

    // Render center line
    renderCenterLine();

    // Render events
    renderEvents();

    // Render hovered event if any
    if (_hovered_event.has_value()) {
        renderHoveredEvent();
    }

    // Unbind
    _shader_program->release();
}

void EventPlotOpenGLWidget::resizeGL(int width, int height) {
    _widget_width = width;
    _widget_height = height;

    // Update projection matrix for new aspect ratio
    updateMatrices();
}

void EventPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    _mouse_pressed = true;
    _last_mouse_pos = event->pos();

    // Handle double-click for frame jumping
    if (event->type() == QEvent::MouseButtonDblClick) {
        QVector2D world_pos = screenToWorld(event->pos().x(), event->pos().y());
        // TODO: Find event at world position and emit frameJumpRequested
        // This will be implemented in subsequent steps
    }
}

void EventPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    if (_mouse_pressed) {
        QPoint current_pos = event->pos();
        int delta_x = current_pos.x() - _last_mouse_pos.x();
        int delta_y = current_pos.y() - _last_mouse_pos.y();

        handlePanning(delta_x, delta_y);

        _last_mouse_pos = current_pos;
    } else {
                // Handle hover detection
        auto hovered_event = findEventNear(event->pos().x(), event->pos().y());
        
        // Compare the optional values properly
        bool hover_changed = false;
        if (hovered_event.has_value() != _hovered_event.has_value()) {
            hover_changed = true;
        } else if (hovered_event.has_value() && _hovered_event.has_value()) {
            // Both have values, compare the actual event data
            hover_changed = (hovered_event->trial_index != _hovered_event->trial_index ||
                           hovered_event->event_index != _hovered_event->event_index);
        }
        
        if (hover_changed) {
            _hovered_event = hovered_event;
            
            if (_hovered_event.has_value() && _tooltips_enabled) {
                // Start tooltip timer
                _tooltip_timer->start();
            } else {
                // Hide tooltip immediately
                _tooltip_timer->stop();
                QToolTip::hideText();
            }
            
            update();
        }
    }
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    Q_UNUSED(event)
    _mouse_pressed = false;
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    handleZooming(event->angleDelta().y());
}

void EventPlotOpenGLWidget::leaveEvent(QEvent * event) {
    Q_UNUSED(event)
    _hovered_event.reset();
    _tooltip_timer->stop();
    QToolTip::hideText();
    update();
}

void EventPlotOpenGLWidget::handleTooltipTimer() {
    if (_hovered_event.has_value() && _tooltips_enabled) {
        QString tooltip_text = QString("Trial %1, Event %2\nTime: %3 ms")
                                       .arg(_hovered_event->trial_index + 1)
                                       .arg(_hovered_event->event_index + 1)
                                       .arg(_hovered_event->x, 0, 'f', 1);

        QToolTip::showText(mapToGlobal(QPoint(_last_mouse_pos.x(), _last_mouse_pos.y())),
                           tooltip_text, this);
    }
}

void EventPlotOpenGLWidget::initializeShaders() {
    // Create shader program
    _shader_program = new QOpenGLShaderProgram();

    // Vertex shader
    char const * vertex_shader_source = R"(
        #version 410 core
        layout(location = 0) in vec2 position;
        
        uniform mat4 view_matrix;
        uniform mat4 projection_matrix;
        uniform vec4 u_color;
        uniform float u_point_size;
        
        out vec4 frag_color;
        
        void main() {
            gl_Position = projection_matrix * view_matrix * vec4(position, 0.0, 1.0);
            gl_PointSize = u_point_size;
            frag_color = u_color;
        }
    )";

    // Fragment shader
    char const * fragment_shader_source = R"(
        #version 410 core
        in vec4 frag_color;
        out vec4 final_color;
        
        void main() {
            // Create a circular point
            vec2 center = gl_PointCoord - vec2(0.5);
            float dist = length(center);
            if (dist > 0.5) {
                discard;
            }
            final_color = frag_color;
        }
    )";

    // Compile and link shaders
    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_source)) {
        qWarning("Failed to compile vertex shader");
        qDebug() << "Vertex shader compilation error:" << _shader_program->log();
        return;
    }

    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader_source)) {
        qWarning("Failed to compile fragment shader");
        qDebug() << "Fragment shader compilation error:" << _shader_program->log();
        return;
    }

    if (!_shader_program->link()) {
        qWarning("Failed to link shader program");
        qDebug() << "Shader program linking error:" << _shader_program->log();
        return;
    }
    
    qDebug() << "EventPlotOpenGLWidget::initializeShaders - shader program linked successfully";
}

void EventPlotOpenGLWidget::initializeBuffers() {
    // Create vertex buffer for regular events
    _vertex_buffer.create();
    _vertex_buffer.bind();
    _vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    _vertex_buffer.allocate(0);
    _vertex_buffer.release();

    // Create vertex array object for regular events
    _vertex_array_object.create();
    _vertex_array_object.bind();
    _vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    _vertex_buffer.release();
    _vertex_array_object.release();

    // Create highlight buffer for hovered events
    _highlight_vertex_buffer.create();
    _highlight_vertex_buffer.bind();
    _highlight_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    _highlight_vertex_buffer.allocate(0);
    _highlight_vertex_buffer.release();

    // Create vertex array object for highlight events
    _highlight_vertex_array_object.create();
    _highlight_vertex_array_object.bind();
    _highlight_vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    _highlight_vertex_buffer.release();
    _highlight_vertex_array_object.release();

    // Create center line buffer and vertex array object
    _center_line_buffer.create();
    _center_line_buffer.bind();
    _center_line_buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    
    // Create a vertical line at x=0 from y=-1 to y=1
    float center_line_data[4] = {0.0f, -1.0f, 0.0f, 1.0f}; // Two points: (0,-1) and (0,1)
    _center_line_buffer.allocate(center_line_data, 4 * sizeof(float));
    _center_line_buffer.release();

    _center_line_vertex_array_object.create();
    _center_line_vertex_array_object.bind();
    _center_line_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    _center_line_buffer.release();
    _center_line_vertex_array_object.release();
}

void EventPlotOpenGLWidget::updateMatrices() {
    // Update view matrix (pan and zoom)
    _view_matrix.setToIdentity();
    _view_matrix.translate(_pan_offset_x, _pan_offset_y, 0.0f);
    _view_matrix.scale(_zoom_level, _zoom_level, 1.0f);

    // Update projection matrix (orthographic)
    // Use X-axis range to set the horizontal bounds
    // Events are normalized to 0, so range is from -negative_range to +positive_range
    _projection_matrix.setToIdentity();
    
    // TEMPORARY: Use a smaller range for testing to see if events are visible
    // float left = -static_cast<float>(_negative_range);
    // float right = static_cast<float>(_positive_range);
    float left = -1000.0f;  // Smaller range for testing
    float right = 1000.0f;   // Smaller range for testing
    
    float bottom = -1.0f;
    float top = 1.0f;
    _projection_matrix.ortho(left, right, bottom, top, -1.0f, 1.0f);
    
    qDebug() << "EventPlotOpenGLWidget::updateMatrices - projection bounds:" << left << "to" << right << "Y:" << bottom << "to" << top;
}

void EventPlotOpenGLWidget::handlePanning(int delta_x, int delta_y) {
    float pan_sensitivity = 0.01f;
    float new_pan_x = _pan_offset_x - delta_x * pan_sensitivity;
    float new_pan_y = _pan_offset_y + delta_y * pan_sensitivity;

    setPanOffset(new_pan_x, new_pan_y);
}

void EventPlotOpenGLWidget::handleZooming(int delta_y) {
    float zoom_sensitivity = 0.001f;
    float zoom_factor = 1.0f + delta_y * zoom_sensitivity;
    float new_zoom = _zoom_level * zoom_factor;

    // Clamp zoom level
    new_zoom = qBound(0.1f, new_zoom, 10.0f);

    setZoomLevel(new_zoom);
}

void EventPlotOpenGLWidget::setXAxisRange(int negative_range, int positive_range) {
    if (_negative_range != negative_range || _positive_range != positive_range) {
        _negative_range = negative_range;
        _positive_range = positive_range;
        updateMatrices();
        update();
    }
}

void EventPlotOpenGLWidget::getXAxisRange(int & negative_range, int & positive_range) const {
    negative_range = _negative_range;
    positive_range = _positive_range;
}

void EventPlotOpenGLWidget::updateVertexData() {
    _vertex_data.clear();
    _total_events = 0;

    if (_event_data.empty()) {
        qDebug() << "EventPlotOpenGLWidget::updateVertexData - no event data";
        return;
    }

    // Calculate total number of events for pre-allocation
    for (auto const & trial: _event_data) {
        _total_events += trial.size();
    }

    qDebug() << "EventPlotOpenGLWidget::updateVertexData - total events:" << _total_events;

    _vertex_data.reserve(_total_events * 2);// 2 floats per vertex (x, y)

    // Convert trial index to y-coordinate (normalized to [-1, 1])
    float y_scale = 2.0f / static_cast<float>(_event_data.size());

    for (size_t trial_index = 0; trial_index < _event_data.size(); ++trial_index) {
        float y = -1.0f + (static_cast<float>(trial_index) + 0.5f) * y_scale;

        for (float event_time: _event_data[trial_index]) {
            // event_time is already normalized to center (0 = center)
            _vertex_data.push_back(event_time);
            _vertex_data.push_back(y);
            
                // Debug: print first few events
    if (_vertex_data.size() <= 10) {
        qDebug() << "Event" << _vertex_data.size()/2 << "at position:" << event_time << "," << y;
    }
    
    // Debug: print some sample events from different trials
    if (trial_index < 3 && !_event_data[trial_index].empty()) {
        qDebug() << "Trial" << trial_index << "sample events:";
        for (size_t i = 0; i < std::min(size_t(3), _event_data[trial_index].size()); ++i) {
            qDebug() << "  Event" << i << "time:" << _event_data[trial_index][i];
        }
    }
        }
    }

    qDebug() << "EventPlotOpenGLWidget::updateVertexData - vertex data size:" << _vertex_data.size();

    // Update OpenGL buffer
    if (_vertex_buffer.isCreated()) {
        _vertex_buffer.bind();
        _vertex_buffer.allocate(_vertex_data.data(), static_cast<int>(_vertex_data.size() * sizeof(float)));
        _vertex_buffer.release();
        qDebug() << "EventPlotOpenGLWidget::updateVertexData - buffer updated";
    } else {
        qDebug() << "EventPlotOpenGLWidget::updateVertexData - vertex buffer not created!";
    }
}

std::optional<EventPlotOpenGLWidget::HoveredEvent> EventPlotOpenGLWidget::findEventNear(int screen_x, int screen_y, float tolerance_pixels) const {
    if (_event_data.empty() || _vertex_data.empty()) {
        return std::nullopt;
    }

    QVector2D world_pos = screenToWorld(screen_x, screen_y);
    float world_tolerance = calculateWorldTolerance(tolerance_pixels);

    // Convert trial index to y-coordinate
    float y_scale = 2.0f / static_cast<float>(_event_data.size());

    for (size_t trial_index = 0; trial_index < _event_data.size(); ++trial_index) {
        float trial_y = -1.0f + (static_cast<float>(trial_index) + 0.5f) * y_scale;

        // Check if mouse is near this trial's y-coordinate
        if (std::abs(world_pos.y() - trial_y) <= world_tolerance) {
            for (size_t event_index = 0; event_index < _event_data[trial_index].size(); ++event_index) {
                float event_x = _event_data[trial_index][event_index];

                // Check if mouse is near this event's x-coordinate
                if (std::abs(world_pos.x() - event_x) <= world_tolerance) {
                    return HoveredEvent{
                            static_cast<int>(trial_index),
                            static_cast<int>(event_index),
                            event_x,
                            trial_y};
                }
            }
        }
    }

    return std::nullopt;
}

float EventPlotOpenGLWidget::calculateWorldTolerance(float screen_tolerance) const {
    // Convert screen tolerance to world coordinates
    // This is a simplified conversion - in a real implementation you might want more sophisticated coordinate transformation
    float world_tolerance = screen_tolerance * 0.01f;// Rough conversion factor
    return world_tolerance;
}

void EventPlotOpenGLWidget::renderEvents() {
    if (_vertex_data.empty()) {
        qDebug() << "EventPlotOpenGLWidget::renderEvents - no vertex data";
        return;
    }

    qDebug() << "EventPlotOpenGLWidget::renderEvents - rendering" << _total_events << "events";

    _vertex_array_object.bind();
    _vertex_buffer.bind();

    // Set uniforms for regular event rendering
    _shader_program->setUniformValue("u_color", QVector4D(0.2f, 0.4f, 0.8f, 1.0f));// Blue
    _shader_program->setUniformValue("u_point_size", 6.0f);

    // Draw all events
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(_total_events));

    _vertex_buffer.release();
    _vertex_array_object.release();
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "EventPlotOpenGLWidget::renderEvents - OpenGL error:" << error;
    }
}

void EventPlotOpenGLWidget::renderHoveredEvent() {
    if (!_hovered_event.has_value()) {
        return;
    }

    _highlight_vertex_array_object.bind();
    _highlight_vertex_buffer.bind();

    // Create vertex data for the hovered event
    float highlight_data[2] = {_hovered_event->x, _hovered_event->y};
    _highlight_vertex_buffer.allocate(highlight_data, 2 * sizeof(float));

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Set uniforms for highlight rendering
    _shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.0f, 0.0f, 1.0f));// Red
    _shader_program->setUniformValue("u_point_size", 12.0f);                       // Larger size

    // Draw the highlighted event
    glDrawArrays(GL_POINTS, 0, 1);

    _highlight_vertex_buffer.release();
    _highlight_vertex_array_object.release();
}

void EventPlotOpenGLWidget::renderCenterLine() {
    qDebug() << "EventPlotOpenGLWidget::renderCenterLine called";
    
    _center_line_vertex_array_object.bind();
    _center_line_buffer.bind();

    // Set uniforms for center line rendering
    _shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    _shader_program->setUniformValue("u_point_size", 2.0f); // Thin line

    // Draw the center line as GL_LINES
    glDrawArrays(GL_LINES, 0, 2);

    _center_line_buffer.release();
    _center_line_vertex_array_object.release();
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "EventPlotOpenGLWidget::renderCenterLine - OpenGL error:" << error;
    }
}