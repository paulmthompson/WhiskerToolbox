#include "EventPlotOpenGLWidget.hpp"

#include <QMouseEvent>
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
      _widget_height(1) {
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
}

EventPlotOpenGLWidget::~EventPlotOpenGLWidget() {
    // OpenGL context must be current for cleanup
    makeCurrent();

    if (_shader_program) {
        delete _shader_program;
    }

    _vertex_buffer.destroy();
    _vertex_array_object.destroy();

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
    initializeOpenGLFunctions();

    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Initialize shaders and buffers
    initializeShaders();
    initializeBuffers();
    updateMatrices();
}

void EventPlotOpenGLWidget::paintGL() {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_shader_program || !_shader_program->isLinked()) {
        return;
    }

    // Bind shader program
    _shader_program->bind();

    // Set transformation matrices
    _shader_program->setUniformValue("view_matrix", _view_matrix);
    _shader_program->setUniformValue("projection_matrix", _projection_matrix);

    // Bind vertex array object
    _vertex_array_object.bind();

    // TODO: Render event data here
    // This will be implemented in subsequent steps

    // Unbind
    _vertex_array_object.release();
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
    }
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    Q_UNUSED(event)
    _mouse_pressed = false;
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    handleZooming(event->angleDelta().y());
}

void EventPlotOpenGLWidget::initializeShaders() {
    // Create shader program
    _shader_program = new QOpenGLShaderProgram();

    // Vertex shader
    char const * vertex_shader_source = R"(
        #version 410 core
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec3 color;
        
        uniform mat4 view_matrix;
        uniform mat4 projection_matrix;
        
        out vec3 frag_color;
        
        void main() {
            gl_Position = projection_matrix * view_matrix * vec4(position, 1.0);
            frag_color = color;
        }
    )";

    // Fragment shader
    char const * fragment_shader_source = R"(
        #version 410 core
        in vec3 frag_color;
        out vec4 final_color;
        
        void main() {
            final_color = vec4(frag_color, 1.0);
        }
    )";

    // Compile and link shaders
    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_source)) {
        qWarning("Failed to compile vertex shader");
        return;
    }

    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader_source)) {
        qWarning("Failed to compile fragment shader");
        return;
    }

    if (!_shader_program->link()) {
        qWarning("Failed to link shader program");
        return;
    }
}

void EventPlotOpenGLWidget::initializeBuffers() {
    // Create vertex buffer
    _vertex_buffer.create();
    _vertex_buffer.bind();

    // TODO: Set up vertex data for events
    // This will be implemented in subsequent steps

    // For now, create empty buffer
    _vertex_buffer.allocate(0);
    _vertex_buffer.release();

    // Create vertex array object
    _vertex_array_object.create();
    _vertex_array_object.bind();

    // Set up vertex attributes
    _vertex_buffer.bind();
    glEnableVertexAttribArray(0);// position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);// color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    _vertex_buffer.release();

    _vertex_array_object.release();
}

void EventPlotOpenGLWidget::updateMatrices() {
    // Update view matrix (pan and zoom)
    _view_matrix.setToIdentity();
    _view_matrix.translate(_pan_offset_x, _pan_offset_y, 0.0f);
    _view_matrix.scale(_zoom_level, _zoom_level, 1.0f);

    // Update projection matrix (orthographic)
    _projection_matrix.setToIdentity();
    float aspect_ratio = static_cast<float>(_widget_width) / _widget_height;
    _projection_matrix.ortho(-aspect_ratio, aspect_ratio, -1.0f, 1.0f, -1.0f, 1.0f);
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