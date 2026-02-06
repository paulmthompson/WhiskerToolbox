#include "ThreeDPlotOpenGLWidget.hpp"

#include "Core/3DPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

ThreeDPlotOpenGLWidget::ThreeDPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
{
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling
    setFormat(format);
}

ThreeDPlotOpenGLWidget::~ThreeDPlotOpenGLWidget()
{
    makeCurrent();
    
    // Clean up OpenGL resources
    _vao.destroy();
    _vbo.destroy();
    delete _shader_program;
    _grid_vao.destroy();
    _grid_vbo.destroy();
    delete _grid_shader_program;
    
    doneCurrent();
}

void ThreeDPlotOpenGLWidget::setState(std::shared_ptr<ThreeDPlotState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &ThreeDPlotState::stateChanged,
                this, &ThreeDPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &ThreeDPlotState::plotDataKeyAdded,
                this, &ThreeDPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &ThreeDPlotState::plotDataKeyRemoved,
                this, &ThreeDPlotOpenGLWidget::onStateChanged);

        // Initial update
        update();
    }
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void ThreeDPlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (white background)
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize shaders and buffers
    _initializeShaders();
    _initializeBuffers();
    _initializeGridBuffers();
    _updateProjectionMatrix();
    _updateViewMatrix();
}

void ThreeDPlotOpenGLWidget::paintGL()
{
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set up matrices
    QMatrix4x4 mvp = _projection_matrix * _view_matrix;

    // Render grid first
    _renderGrid();

    // Render points if we have data
    if (_shader_program && _point_count > 0) {
        // Use shader program
        if (!_shader_program->bind()) {
            return;
        }

        _shader_program->setUniformValue("u_mvp_matrix", mvp);
        _shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));  // Black points on white background
        _shader_program->setUniformValue("u_point_size", 5.0f);

        // Bind VAO and draw points
        _vao.bind();
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDrawArrays(GL_POINTS, 0, _point_count);
        glDisable(GL_PROGRAM_POINT_SIZE);
        _vao.release();

        _shader_program->release();
    }
}

void ThreeDPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = w;
    _widget_height = h;

    glViewport(0, 0, w, h);
    _updateProjectionMatrix();
    _updateViewMatrix();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void ThreeDPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    _last_mouse_pos = event->pos();

    if (event->button() == Qt::LeftButton) {
        _is_rotating = true;
        _active_button = Qt::LeftButton;
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::RightButton) {
        _is_panning = true;
        _active_button = Qt::RightButton;
        setCursor(Qt::SizeAllCursor);
    } else if (event->button() == Qt::MiddleButton) {
        _is_panning = true;
        _active_button = Qt::MiddleButton;
        setCursor(Qt::SizeAllCursor);
    }

    event->accept();
}

void ThreeDPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    QPoint current_pos = event->pos();
    int delta_x = current_pos.x() - _last_mouse_pos.x();
    int delta_y = current_pos.y() - _last_mouse_pos.y();

    if (_is_rotating && _active_button == Qt::LeftButton) {
        // Rotate camera: horizontal movement rotates azimuth, vertical rotates elevation
        _camera_azimuth += static_cast<float>(delta_x) * 0.5f;
        _camera_elevation += static_cast<float>(delta_y) * 0.5f;
        
        // Clamp elevation to avoid gimbal lock
        _camera_elevation = qBound(-89.0f, _camera_elevation, 89.0f);
        
        _updateViewMatrix();
        emit viewBoundsChanged();
        update();
    } else if (_is_panning && (_active_button == Qt::RightButton || _active_button == Qt::MiddleButton)) {
        // Pan camera: translate in the XY plane
        // Scale panning based on camera distance to keep panning speed consistent
        float pan_scale = _camera_distance * 0.001f;
        _camera_pan.setX(_camera_pan.x() + static_cast<float>(delta_x) * pan_scale);
        _camera_pan.setY(_camera_pan.y() - static_cast<float>(delta_y) * pan_scale);  // Invert Y for intuitive panning
        
        _updateViewMatrix();
        emit viewBoundsChanged();
        update();
    }

    _last_mouse_pos = current_pos;
    event->accept();
}

void ThreeDPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == _active_button) {
        _is_rotating = false;
        _is_panning = false;
        _active_button = Qt::NoButton;
        setCursor(Qt::ArrowCursor);
    }

    event->accept();
}

void ThreeDPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    // Zoom camera: adjust distance from origin
    float zoom_factor = 1.0f + (static_cast<float>(event->angleDelta().y()) * 0.001f);
    _camera_distance *= zoom_factor;
    
    // Clamp distance to reasonable bounds
    _camera_distance = qBound(10.0f, _camera_distance, 5000.0f);
    
    _updateViewMatrix();
    emit viewBoundsChanged();
    update();

    event->accept();
}

// =============================================================================
// Private Slots
// =============================================================================

void ThreeDPlotOpenGLWidget::onStateChanged()
{
    // Reload data if we have a valid time position and data manager
    if (_last_data_manager && _last_time_position.isValid()) {
        updateTime(_last_data_manager, _last_time_position);
    } else {
        // Just repaint (will show empty or current state)
        update();
    }
}

void ThreeDPlotOpenGLWidget::updateTime(std::shared_ptr<DataManager> data_manager,
                                         TimePosition position)
{
    // Cache the data manager and time position for reloading when keys change
    _last_data_manager = data_manager;
    _last_time_position = position;

    if (!data_manager || !position.isValid() || !_state) {
        _point_count = 0;
        update();
        return;
    }

    // Get all plot data keys from state
    auto data_keys = _state->getPlotDataKeys();
    
    if (data_keys.empty()) {
        _point_count = 0;
        update();
        return;
    }

    // Collect points from all data keys
    std::vector<Point2D<float>> all_points;
    
    for (QString const & key_qstr : data_keys) {
        std::string key = key_qstr.toStdString();
        
        // Get the PointData for this key
        auto point_data = data_manager->getData<PointData>(key);
        if (!point_data) {
            continue;
        }

        // Get points at the current time frame (returns a view)
        auto points_view = point_data->getAtTime(position.index);
        
        // Add points to collection
        for (auto const & point: points_view) {
            all_points.push_back(point);
        }
    }
    
    if (all_points.empty()) {
        _point_count = 0;
        update();
        return;
    }

    // Convert Point2D to 3D (add z=0) and store in float array
    _point_data.clear();
    _point_data.reserve(all_points.size() * 3);
    
    for (auto const & point: all_points) {
        _point_data.push_back(point.x);
        _point_data.push_back(point.y);
        _point_data.push_back(0.0f);  // z = 0 for now
    }

    _point_count = static_cast<int>(all_points.size());

    // Upload to GPU
    makeCurrent();
    if (_vbo.isCreated()) {
        _vbo.bind();
        _vbo.allocate(_point_data.data(), static_cast<int>(_point_data.size() * sizeof(float)));
        _vbo.release();
    }
    doneCurrent();

    qDebug() << "ThreeDPlotOpenGLWidget::updateTime: frame=" << position.index.getValue()
             << ", keys=" << data_keys.size()
             << ", total_points=" << _point_count;

    update();
}

void ThreeDPlotOpenGLWidget::_initializeShaders()
{
    _shader_program = new QOpenGLShaderProgram();

    // Vertex shader for 3D points
    char const * vertex_shader_source = R"(
        #version 410 core
        layout(location = 0) in vec3 position;
        
        uniform mat4 u_mvp_matrix;
        uniform vec4 u_color;
        uniform float u_point_size;
        
        out vec4 frag_color;
        
        void main() {
            gl_Position = u_mvp_matrix * vec4(position, 1.0);
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
            // Smooth edge
            float alpha = 1.0 - smoothstep(0.4, 0.5, dist);
            final_color = vec4(frag_color.rgb, frag_color.a * alpha);
        }
    )";

    // Compile and link shaders
    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_source)) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to compile vertex shader");
        qDebug() << "Vertex shader compilation error:" << _shader_program->log();
        return;
    }

    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader_source)) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to compile fragment shader");
        qDebug() << "Fragment shader compilation error:" << _shader_program->log();
        return;
    }

    if (!_shader_program->link()) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to link shader program");
        qDebug() << "Shader program linking error:" << _shader_program->log();
        return;
    }
}

void ThreeDPlotOpenGLWidget::_initializeBuffers()
{
    // Create vertex buffer
    _vbo.create();
    _vbo.bind();
    _vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    _vbo.allocate(0);
    _vbo.release();

    // Create vertex array object
    _vao.create();
    _vao.bind();
    _vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    _vbo.release();
    _vao.release();
}

void ThreeDPlotOpenGLWidget::_initializeGridBuffers()
{
    // Create grid shader program
    _grid_shader_program = new QOpenGLShaderProgram();

    // Vertex shader for grid lines
    char const * grid_vertex_shader_source = R"(
        #version 410 core
        layout(location = 0) in vec3 position;
        
        uniform mat4 u_mvp_matrix;
        uniform vec4 u_color;
        
        out vec4 frag_color;
        
        void main() {
            gl_Position = u_mvp_matrix * vec4(position, 1.0);
            frag_color = u_color;
        }
    )";

    // Fragment shader for grid lines
    char const * grid_fragment_shader_source = R"(
        #version 410 core
        in vec4 frag_color;
        out vec4 final_color;
        
        void main() {
            final_color = frag_color;
        }
    )";

    // Compile and link grid shaders
    if (!_grid_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, grid_vertex_shader_source)) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to compile grid vertex shader");
        qDebug() << "Grid vertex shader compilation error:" << _grid_shader_program->log();
        return;
    }

    if (!_grid_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, grid_fragment_shader_source)) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to compile grid fragment shader");
        qDebug() << "Grid fragment shader compilation error:" << _grid_shader_program->log();
        return;
    }

    if (!_grid_shader_program->link()) {
        qWarning("ThreeDPlotOpenGLWidget: Failed to link grid shader program");
        qDebug() << "Grid shader program linking error:" << _grid_shader_program->log();
        return;
    }

    // Create grid lines at z=0 with 200 pixel spacing
    // Create a grid from -2000 to 2000 in both x and y directions
    const float grid_spacing = 200.0f;
    const float grid_min = -2000.0f;
    const float grid_max = 2000.0f;
    const float z_pos = 0.0f;

    _grid_data.clear();

    // Vertical lines (constant x)
    for (float x = grid_min; x <= grid_max; x += grid_spacing) {
        _grid_data.push_back(x);
        _grid_data.push_back(grid_min);
        _grid_data.push_back(z_pos);
        _grid_data.push_back(x);
        _grid_data.push_back(grid_max);
        _grid_data.push_back(z_pos);
    }

    // Horizontal lines (constant y)
    for (float y = grid_min; y <= grid_max; y += grid_spacing) {
        _grid_data.push_back(grid_min);
        _grid_data.push_back(y);
        _grid_data.push_back(z_pos);
        _grid_data.push_back(grid_max);
        _grid_data.push_back(y);
        _grid_data.push_back(z_pos);
    }

    _grid_vertex_count = static_cast<int>(_grid_data.size() / 3);

    // Create grid vertex buffer
    _grid_vbo.create();
    _grid_vbo.bind();
    _grid_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    _grid_vbo.allocate(_grid_data.data(), static_cast<int>(_grid_data.size() * sizeof(float)));
    _grid_vbo.release();

    // Create grid vertex array object
    _grid_vao.create();
    _grid_vao.bind();
    _grid_vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    _grid_vbo.release();
    _grid_vao.release();
}

void ThreeDPlotOpenGLWidget::_renderGrid()
{
    if (!_grid_shader_program || _grid_vertex_count == 0) {
        return;
    }

    // Use grid shader program
    if (!_grid_shader_program->bind()) {
        return;
    }

    // Set up matrices
    QMatrix4x4 mvp = _projection_matrix * _view_matrix;
    _grid_shader_program->setUniformValue("u_mvp_matrix", mvp);
    _grid_shader_program->setUniformValue("u_color", QVector4D(0.7f, 0.7f, 0.7f, 1.0f));  // Light gray grid lines

    // Bind grid VAO and draw lines
    _grid_vao.bind();
    glDrawArrays(GL_LINES, 0, _grid_vertex_count);
    _grid_vao.release();

    _grid_shader_program->release();
}

void ThreeDPlotOpenGLWidget::_updateProjectionMatrix()
{
    if (_widget_width <= 0 || _widget_height <= 0) {
        return;
    }

    // Perspective projection
    float aspect = static_cast<float>(_widget_width) / static_cast<float>(_widget_height);
    float fov = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 10000.0f;

    _projection_matrix.setToIdentity();
    _projection_matrix.perspective(fov, aspect, near_plane, far_plane);
}

void ThreeDPlotOpenGLWidget::_updateViewMatrix()
{
    _view_matrix.setToIdentity();
    
    // Apply pan offset first
    _view_matrix.translate(-_camera_pan);
    
    // Rotate around origin: first elevation (around X), then azimuth (around Y)
    _view_matrix.rotate(_camera_elevation, 1.0f, 0.0f, 0.0f);
    _view_matrix.rotate(_camera_azimuth, 0.0f, 1.0f, 0.0f);
    
    // Move camera back by distance
    _view_matrix.translate(0.0f, 0.0f, -_camera_distance);
}
