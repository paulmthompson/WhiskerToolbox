#include "EventPlotOpenGLWidget.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QToolTip>
#include <QVector3D>
#include <QWheelEvent>

EventPlotOpenGLWidget::EventPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _shader_program(nullptr),
      _line_shader_program(nullptr),
      _zoom_level(1.0f),
      _y_zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _mouse_pressed(false),
      _tooltips_enabled(true),
      _widget_width(1),
      _widget_height(1),
      _negative_range(30000),
      _positive_range(30000),
      _total_events(0),
      _data_bounds_valid(false),
      _opengl_resources_initialized(false),
      _tooltip_timer(nullptr),
      _hover_debounce_timer(nullptr),
      _tooltip_refresh_timer(nullptr),
      _hover_processing_active(false),
      _pending_hover_pos(0, 0) {
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);

    // Request OpenGL 4.1 Core Profile (same as SpatialOverlayOpenGLWidget)
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for smooth points
    setFormat(format);

    // Initialize tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay
    connect(_tooltip_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::handleTooltipTimer);

    // Initialize hover processing timers (similar to SpatialOverlayOpenGLWidget)
    _hover_debounce_timer = new QTimer(this);
    _hover_debounce_timer->setSingleShot(true);
    _hover_debounce_timer->setInterval(16); // ~60 FPS debounce
    connect(_hover_debounce_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::processHoverDebounce);

    _tooltip_refresh_timer = new QTimer(this);
    _tooltip_refresh_timer->setSingleShot(false);
    _tooltip_refresh_timer->setInterval(100); // Refresh every 100ms
    connect(_tooltip_refresh_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::handleTooltipRefresh);

    // Initialize hover processing state
    _hover_processing_active = false;
    
}


EventPlotOpenGLWidget::~EventPlotOpenGLWidget() {
    // OpenGL context must be current for cleanup
    makeCurrent();

    if (_shader_program) {
        delete _shader_program;
    }

    if (_line_shader_program) {
        delete _line_shader_program;
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

void EventPlotOpenGLWidget::setYZoomLevel(float y_zoom_level) {
    if (_y_zoom_level != y_zoom_level) {
        _y_zoom_level = y_zoom_level;
        updateMatrices();
        update();
        emit zoomLevelChanged(_y_zoom_level);
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
    
    updateVertexData();
    updateMatrices();
    update();
}

void EventPlotOpenGLWidget::initializeGL() {
    qDebug() << "EventPlotOpenGLWidget::initializeGL called";
    
    // Check if OpenGL functions can be initialized
    if (!initializeOpenGLFunctions()) {
        qWarning() << "EventPlotOpenGLWidget::initializeGL - Failed to initialize OpenGL functions";
        return;
    }

    // Set clear color
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);// Light gray background

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Enable programmable point size
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // Enable blending for smoother points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize shaders and buffers
    initializeShaders();
    initializeBuffers();
    
    // Mark OpenGL resources as initialized
    _opengl_resources_initialized = true;
    
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

    if (!_opengl_resources_initialized) {
        qDebug() << "EventPlotOpenGLWidget::paintGL - OpenGL resources not initialized";
        return;
    }

    // Always render center line first (uses its own shader)
    renderCenterLine();

    // Bind point shader program for events
    _shader_program->bind();

    // Set transformation matrices for point rendering
    _shader_program->setUniformValue("view_matrix", _view_matrix);
    _shader_program->setUniformValue("projection_matrix", _projection_matrix);

    // Render events if we have data
    if (!_vertex_data.empty()) {
        renderEvents();
    }

    // Render hovered event if any
    if (_hovered_event.has_value()) {
        renderHoveredEvent();
    }

    // Unbind point shader
    _shader_program->release();
    
    qDebug() << "EventPlotOpenGLWidget::paintGL completed";
}

void EventPlotOpenGLWidget::resizeGL(int width, int height) {
    _widget_width = width;
    _widget_height = height;

    // Set the viewport
    glViewport(0, 0, width, height);

    // Update projection matrix for new aspect ratio
    updateMatrices();
}

void EventPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    _mouse_pressed = true;
    _last_mouse_pos = event->pos();

    // Handle double-click for frame jumping
    if (event->type() == QEvent::MouseButtonDblClick) {
        float world_x;
        float world_y;
        screenToWorld(event->pos().x(), event->pos().y(), world_x, world_y);
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
        // Handle tooltip logic if tooltips are enabled (improved version)
        if (_tooltips_enabled) {
            // Store the current mouse position for debounced processing
            _pending_hover_pos = event->pos();
            
            // If hover processing is currently active, skip this event
            if (_hover_processing_active) {
                return;
            }
            
            // Start or restart the debounce timer
            _hover_debounce_timer->stop();
            _hover_debounce_timer->start();
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

void EventPlotOpenGLWidget::handleTooltipRefresh() {
    if (_hovered_event.has_value() && _tooltips_enabled) {
        QString tooltip_text = QString("Trial %1, Event %2\nTime: %3 ms")
                                       .arg(_hovered_event->trial_index + 1)
                                       .arg(_hovered_event->event_index + 1)
                                       .arg(_hovered_event->x, 0, 'f', 1);

        QToolTip::showText(mapToGlobal(_pending_hover_pos), tooltip_text, this);
    }
}

void EventPlotOpenGLWidget::initializeShaders() {
    // Create shader program for points (events)
    _shader_program = new QOpenGLShaderProgram();

    // Vertex shader for points
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

    // Fragment shader for points
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

    // Compile and link point shaders
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
    
    qDebug() << "EventPlotOpenGLWidget::initializeShaders - point shader program linked successfully";

    // Create separate shader program for lines (center line)
    _line_shader_program = new QOpenGLShaderProgram();

    // Vertex shader for lines
    char const * line_vertex_shader_source = R"(
        #version 410 core
        layout(location = 0) in vec2 position;
        
        uniform vec4 u_color;
        uniform float u_world_x;  // World X coordinate for the line
        uniform mat4 view_matrix;
        uniform mat4 projection_matrix;
        
        out vec4 frag_color;
        
        void main() {
            // Use the world X coordinate and the canvas Y coordinate
            vec4 world_pos = vec4(u_world_x, position.y, 0.0, 1.0);
            gl_Position = projection_matrix * view_matrix * world_pos;
            frag_color = u_color;
        }
    )";

    // Fragment shader for lines
    char const * line_fragment_shader_source = R"(
        #version 410 core
        in vec4 frag_color;
        out vec4 final_color;
        
        void main() {
            final_color = frag_color;
        }
    )";

    // Compile and link line shaders
    if (!_line_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, line_vertex_shader_source)) {
        qWarning("Failed to compile line vertex shader");
        qDebug() << "Line vertex shader compilation error:" << _line_shader_program->log();
        return;
    }

    if (!_line_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, line_fragment_shader_source)) {
        qWarning("Failed to compile line fragment shader");
        qDebug() << "Line fragment shader compilation error:" << _line_shader_program->log();
        return;
    }

    if (!_line_shader_program->link()) {
        qWarning("Failed to link line shader program");
        qDebug() << "Line shader program linking error:" << _line_shader_program->log();
        return;
    }
    
    qDebug() << "EventPlotOpenGLWidget::initializeShaders - line shader program linked successfully";
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
    _center_line_buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);  // Static since we'll just use full canvas height
    
    // Initialize with line data that spans from bottom to top of canvas
    // We'll use the line shader to position this at world X=0
    std::vector<float> center_line_data = {
        0.0f, -1.0f,  // Bottom of canvas
        0.0f,  1.0f   // Top of canvas
    };
    _center_line_buffer.allocate(center_line_data.data(), static_cast<int>(center_line_data.size() * sizeof(float)));
    _center_line_buffer.release();

    _center_line_vertex_array_object.create();
    _center_line_vertex_array_object.bind();
    _center_line_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    _center_line_buffer.release();
    _center_line_vertex_array_object.release();
    
    qDebug() << "EventPlotOpenGLWidget::initializeBuffers - center line buffer created (static line)";
}

void EventPlotOpenGLWidget::updateMatrices() {
    // Update view matrix (pan and Y zoom only)
    _view_matrix.setToIdentity();
    _view_matrix.translate(_pan_offset_x, _pan_offset_y, 0.0f);
    _view_matrix.scale(1.0f, _y_zoom_level, 1.0f);  // Only scale Y axis

    // Update projection matrix (orthographic)
    _projection_matrix.setToIdentity();
    
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    // Always ensure valid bounds
    if (left >= right) {
        left = -static_cast<float>(_negative_range);
        right = static_cast<float>(_positive_range);
    }
    if (bottom >= top) {
        bottom = -1.0f;
        top = 1.0f;
    }
    
    _projection_matrix.ortho(left, right, bottom, top, -1.0f, 1.0f);
    
    qDebug() << "EventPlotOpenGLWidget::updateMatrices - projection bounds:" 
             << "left:" << left << "right:" << right 
             << "bottom:" << bottom << "top:" << top
             << "y_zoom:" << _y_zoom_level;
    
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
    float new_y_zoom = _y_zoom_level * zoom_factor;

    // Clamp Y zoom level
    new_y_zoom = qBound(0.1f, new_y_zoom, 10.0f);

    if (_y_zoom_level != new_y_zoom) {
        _y_zoom_level = new_y_zoom;
        updateMatrices();
        update();
        emit zoomLevelChanged(_y_zoom_level);  // Emit Y zoom level change
    }
}

void EventPlotOpenGLWidget::setXAxisRange(int negative_range, int positive_range) {
    if (_negative_range != negative_range || _positive_range != positive_range) {
        _negative_range = negative_range;
        _positive_range = positive_range;
        
        updateMatrices();
        update();
        
        qDebug() << "EventPlotOpenGLWidget::setXAxisRange - updated to:" << -negative_range << "to" << positive_range;
    
    }
}

void EventPlotOpenGLWidget::getXAxisRange(int & negative_range, int & positive_range) const {
    negative_range = _negative_range;
    positive_range = _positive_range;
}

void EventPlotOpenGLWidget::getVisibleBounds(float & left_bound, float & right_bound) const {
    // Calculate the actual visible bounds including pan offset
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    left_bound = left;
    right_bound = right;
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

std::optional<EventPlotOpenGLWidget::HoveredEvent> EventPlotOpenGLWidget::findEventNear(int screen_x, int screen_y, float tolerance_pixels) {
    if (_event_data.empty() || _vertex_data.empty()) {
        return std::nullopt;
        }

    float world_x, world_y;
    screenToWorld(screen_x, screen_y, world_x, world_y);
    float world_tolerance = calculateWorldTolerance(tolerance_pixels);

    // Convert trial index to y-coordinate
    float y_scale = 2.0f / static_cast<float>(_event_data.size());

    for (size_t trial_index = 0; trial_index < _event_data.size(); ++trial_index) {
        float trial_y = -1.0f + (static_cast<float>(trial_index) + 0.5f) * y_scale;

        // Check if mouse is near this trial's y-coordinate
        if (std::abs(world_y - trial_y) <= world_tolerance) {
            for (size_t event_index = 0; event_index < _event_data[trial_index].size(); ++event_index) {
                float event_x = _event_data[trial_index][event_index];

                // Check if mouse is near this event's x-coordinate
                if (std::abs(world_x - event_x) <= world_tolerance) {
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
    _shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black
    _shader_program->setUniformValue("u_point_size", 12.0f);                       // Larger size

    // Draw the highlighted event
    glDrawArrays(GL_POINTS, 0, 1);

    _highlight_vertex_buffer.release();
    _highlight_vertex_array_object.release();
}

void EventPlotOpenGLWidget::renderCenterLine() {
    qDebug() << "EventPlotOpenGLWidget::renderCenterLine called";
    
    if (!_center_line_vertex_array_object.isCreated() || !_center_line_buffer.isCreated()) {
        qDebug() << "EventPlotOpenGLWidget::renderCenterLine - buffers not created";
        return;
    }
    
    if (!_line_shader_program || !_line_shader_program->isLinked()) {
        qDebug() << "EventPlotOpenGLWidget::renderCenterLine - line shader program not ready";
        return;
    }
    
    // Use the line shader program for rendering the center line
    _line_shader_program->bind();

    // Set transformation matrices
    _line_shader_program->setUniformValue("view_matrix", _view_matrix);
    _line_shader_program->setUniformValue("projection_matrix", _projection_matrix);
    
    // Set the world X coordinate for the center line (always at X=0)
    _line_shader_program->setUniformValue("u_world_x", 0.0f);
    
    // Set line color (red)
    _line_shader_program->setUniformValue("u_color", QVector4D(0.8f, 0.2f, 0.2f, 1.0f));
    
    // Bind vertex array object
    _center_line_vertex_array_object.bind();

    // Draw the center line as GL_LINES (2 vertices: bottom to top)
    glDrawArrays(GL_LINES, 0, 2);

    _center_line_vertex_array_object.release();
    _line_shader_program->release();
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "EventPlotOpenGLWidget::renderCenterLine - OpenGL error:" << error;
    } else {
        qDebug() << "EventPlotOpenGLWidget::renderCenterLine completed successfully with line from canvas bottom to top";
    }
}

void EventPlotOpenGLWidget::calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const {
    if (_widget_width <= 0 || _widget_height <= 0) {
        // Use safe default bounds
        left = -static_cast<float>(_negative_range);
        right = static_cast<float>(_positive_range);
        bottom = -1.0f;
        top = 1.0f;
        qDebug() << "EventPlotOpenGLWidget::calculateProjectionBounds - using default bounds with ranges";
        return;
    }

    // Use the user-specified X-axis range (no X zoom, controlled by spinboxes)
    float x_range_width = static_cast<float>(_negative_range + _positive_range);
    float center_x = 0.0f;  // Always center at 0
    
    // Y-axis is always normalized to [-1, 1] for trials, but apply Y zoom
    float y_range_height = 2.0f;
    float center_y = 0.0f;

    // Apply zoom: X uses no zoom (controlled by spinboxes), Y uses Y zoom
    float x_zoom_factor = 1.0f;  // No X zoom
    float y_zoom_factor = 1.0f / _y_zoom_level;  // Y zoom only
    
    float half_width = (x_range_width * x_zoom_factor) / 2.0f;
    float half_height = (y_range_height * y_zoom_factor) / 2.0f;

    // Apply aspect ratio correction
    float aspect_ratio = static_cast<float>(_widget_width) / _widget_height;
    if (aspect_ratio > 1.0f) {
        half_width *= aspect_ratio;
    } else {
        half_height /= aspect_ratio;
    }

    // Apply pan offset (scale by the range for intuitive panning)
    float pan_x = _pan_offset_x * x_range_width * x_zoom_factor;
    float pan_y = _pan_offset_y * y_range_height * y_zoom_factor;

    left = center_x - half_width + pan_x;
    right = center_x + half_width + pan_x;
    bottom = center_y - half_height + pan_y;
    top = center_y + half_height + pan_y;
    
    qDebug() << "EventPlotOpenGLWidget::calculateProjectionBounds - user-specified bounds:" 
             << "left:" << left << "right:" << right 
             << "bottom:" << bottom << "top:" << top
             << "ranges: -" << _negative_range << "to +" << _positive_range
             << "y_zoom:" << _y_zoom_level;
}

void EventPlotOpenGLWidget::processHoverDebounce() {
    if (!_tooltips_enabled || _hover_processing_active) {
        return;
    }
    
    _hover_processing_active = true;
    
    // Get the screen coordinates
    QPoint screen_pos = _pending_hover_pos;
    
    // Find the nearest event using spatial index
    auto hovered_event = findEventNear(screen_pos.x(), screen_pos.y());
    
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
        
        if (_hovered_event.has_value()) {
            // Start tooltip timer
            _tooltip_refresh_timer->start();
        } else {
            // Hide tooltip immediately
            _tooltip_refresh_timer->stop();
            QToolTip::hideText();
        }
        
        update();
    }
    
    _hover_processing_active = false;
}

float EventPlotOpenGLWidget::calculateWorldTolerance(float screen_tolerance) const {
    // Calculate projection bounds
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    // Calculate world units per pixel
    float world_width = right - left;
    float world_height = top - bottom;
    float world_per_pixel_x = world_width / _widget_width;
    float world_per_pixel_y = world_height / _widget_height;
    
    // Use the smaller of the two to ensure we don't miss events
    float world_per_pixel = std::min(world_per_pixel_x, world_per_pixel_y);
    
    return screen_tolerance * world_per_pixel;
}

void EventPlotOpenGLWidget::screenToWorld(int screen_x, int screen_y, float& world_x, float& world_y) {
    // Calculate projection bounds
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    // Convert screen coordinates to normalized coordinates [0, 1]
    float norm_x = static_cast<float>(screen_x) / _widget_width;
    float norm_y = 1.0f - static_cast<float>(screen_y) / _widget_height; // Flip Y axis
    
    // Convert to world coordinates
    world_x = left + norm_x * (right - left);
    world_y = bottom + norm_y * (top - bottom);
}