#include "EventPlotOpenGLWidget.hpp"
#include "EventPointVisualization.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QToolTip>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include "Analysis_Dashboard/Widgets/Common/ViewAdapter.hpp"
#include "Analysis_Dashboard/Widgets/Common/PlotInteractionController.hpp"
#include "EventPlotViewAdapter.hpp"

EventPlotOpenGLWidget::EventPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _event_visualization(nullptr),
      _group_manager(nullptr),
      _point_size(6.0f),
      _shader_program(nullptr),
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
    format.setSamples(4);// Enable multisampling for smooth points
    setFormat(format);

    // Initialize tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay
    connect(_tooltip_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::handleTooltipTimer);

    // Initialize hover processing timers (similar to SpatialOverlayOpenGLWidget)
    _hover_debounce_timer = new QTimer(this);
    _hover_debounce_timer->setSingleShot(true);
    _hover_debounce_timer->setInterval(16);// ~60 FPS debounce
    connect(_hover_debounce_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::processHoverDebounce);

    _tooltip_refresh_timer = new QTimer(this);
    _tooltip_refresh_timer->setSingleShot(false);
    _tooltip_refresh_timer->setInterval(100);// Refresh every 100ms
    connect(_tooltip_refresh_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::handleTooltipRefresh);

    // Initialize hover processing state
    _hover_processing_active = false;

    // Setup composition-based interaction controller
    _interaction = std::make_unique<PlotInteractionController>(this, std::make_unique<EventPlotViewAdapter>(this));
    connect(_interaction.get(), &PlotInteractionController::viewBoundsChanged, this, &EventPlotOpenGLWidget::viewBoundsChanged);
    connect(_interaction.get(), &PlotInteractionController::mouseWorldMoved, this, &EventPlotOpenGLWidget::mouseWorldMoved);

    // Standardized interaction additions
    _zoom_level_x = 1.0f; // default no X zoom
    _padding_factor = 1.1f;
}


EventPlotOpenGLWidget::~EventPlotOpenGLWidget() {
    // OpenGL context must be current for cleanup
    makeCurrent();

    // Clean up event visualization
    if (_event_visualization) {
        _event_visualization->cleanupOpenGLResources();
        _event_visualization.reset();
    }

    // Clean up center line shader (legacy)
    if (_shader_program) {
        delete _shader_program;
    }

    // Clean up legacy buffers (may be removed in future)
    _vertex_buffer.destroy();
    _vertex_array_object.destroy();
    _highlight_vertex_buffer.destroy();
    _highlight_vertex_array_object.destroy();

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

    // Create new event visualization with the data
    // Note: Don't initialize OpenGL resources here - will be done in initializeGL()
    _event_visualization = std::make_unique<EventPointVisualization>(
        "events", _event_data, _group_manager);

    // If OpenGL is already initialized, initialize the visualization resources
    if (_opengl_resources_initialized && context() && context()->isValid()) {
        makeCurrent();
        try {
            _event_visualization->initializeOpenGLResources();
            qDebug() << "EventPlotOpenGLWidget::setEventData - EventPointVisualization initialized successfully";
        } catch (...) {
            qWarning() << "EventPlotOpenGLWidget::setEventData - Failed to initialize EventPointVisualization";
        }
        doneCurrent();
    }

    // Update legacy vertex data for compatibility (may remove later)
    updateVertexData();
    applyRowColorIndicesIfReady();
    updateMatrices();
    update();
}

void EventPlotOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    
    // Update existing event visualization with group manager
    if (_event_visualization) {
        _event_visualization->setGroupManager(_group_manager);
        
        // Connect to group manager signals if available
        if (_group_manager) {
            connect(_group_manager, &GroupManager::groupModified,
                    this, [this]() {
                        if (_event_visualization) {
                            _event_visualization->refreshGroupRenderData();
                            update();
                        }
                    });
        }
        
        update();
    }
}

void EventPlotOpenGLWidget::setPointSize(float point_size) {
    float new_point_size = std::max(1.0f, std::min(50.0f, point_size)); // Clamp between 1 and 50 pixels
    if (new_point_size != _point_size) {
        _point_size = new_point_size;
        update();
    }
}

void EventPlotOpenGLWidget::initializeGL() {
    qDebug() << "EventPlotOpenGLWidget::initializeGL called";

    // Check if OpenGL functions can be initialized
    if (!initializeOpenGLFunctions()) {
        qWarning() << "EventPlotOpenGLWidget::initializeGL - Failed to initialize OpenGL functions";
        return;
    }

    // Set clear color based on theme
    if (_plot_theme == PlotTheme::Dark) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);// dark gray
    } else {
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);// light gray
    }

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

    // Initialize shaders and buffers (for center line only now)
    initializeShaders();
    initializeBuffers();

    // Mark OpenGL resources as initialized BEFORE initializing event visualization
    _opengl_resources_initialized = true;

    // Initialize event visualization if we have data (context is now ready)
    if (_event_visualization) {
        try {
            _event_visualization->initializeOpenGLResources();
            qDebug() << "EventPlotOpenGLWidget::initializeGL - EventPointVisualization initialized successfully";
        } catch (...) {
            qWarning() << "EventPlotOpenGLWidget::initializeGL - Failed to initialize EventPointVisualization";
        }
    }

    updateMatrices();

    qDebug() << "EventPlotOpenGLWidget::initializeGL completed";

    // Load axes shader via ShaderManager
    ShaderManager::instance().loadProgram(
            "axes",
            ":/shaders/colored_vertex.vert",
            ":/shaders/colored_vertex.frag");
}

void EventPlotOpenGLWidget::paintGL() {
    qDebug() << "EventPlotOpenGLWidget::paintGL called";

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_resources_initialized) {
        qDebug() << "EventPlotOpenGLWidget::paintGL - OpenGL resources not initialized";
        return;
    }

    // Always render center line first (uses its own shader)
    renderCenterLine();

    // Render events using the point visualization system
    if (_event_visualization) {
        QMatrix4x4 mvp = _projection_matrix * _view_matrix;
        _event_visualization->render(mvp, _point_size);
    }

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
        return;
    }
    if (_interaction && _interaction->handleMousePress(event)) return;
}

void EventPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    if (_interaction && _interaction->handleMouseMove(event)) return;
    if (_mouse_pressed) {
        QPoint current_pos = event->pos();
        int delta_x = current_pos.x() - _last_mouse_pos.x();
        int delta_y = current_pos.y() - _last_mouse_pos.y();

        handlePanning(delta_x, delta_y);

        _last_mouse_pos = current_pos;
    } else {
        // Handle tooltip logic if tooltips are enabled using new visualization system
        if (_tooltips_enabled && _event_visualization) {
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
    // Emit mouse world coordinates
    float world_x, world_y;
    screenToWorld(event->pos().x(), event->pos().y(), world_x, world_y);
    emit mouseWorldMoved(world_x, world_y);
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    _mouse_pressed = false;
    if (_interaction && _interaction->handleMouseRelease(event)) return;
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    // Zoom behavior with modifiers: Ctrl=X-only, Shift=Y-only, None=both
    float zoom_sensitivity = 0.001f;
    float zoom_factor = 1.0f + event->angleDelta().y() * zoom_sensitivity;
    zoom_factor = std::clamp(zoom_factor, 0.1f, 10.0f);
    Qt::KeyboardModifiers mods = event->modifiers();
    if (mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::ShiftModifier)) {
        _zoom_level_x = std::clamp(_zoom_level_x * zoom_factor, 0.1f, 10.0f);
    } else if (mods.testFlag(Qt::ShiftModifier) && !mods.testFlag(Qt::ControlModifier)) {
        float new_y_zoom = std::clamp(_y_zoom_level * zoom_factor, 0.1f, 10.0f);
        _y_zoom_level = new_y_zoom;
    } else {
        _zoom_level_x = std::clamp(_zoom_level_x * zoom_factor, 0.1f, 10.0f);
        float new_y_zoom = std::clamp(_y_zoom_level * zoom_factor, 0.1f, 10.0f);
        _y_zoom_level = new_y_zoom;
    }
    updateMatrices();
    update();
}

void EventPlotOpenGLWidget::leaveEvent(QEvent * event) {
    Q_UNUSED(event)
    
    // Clear hover state in event visualization
    if (_event_visualization) {
        _event_visualization->clearHover();
    }
    
    // Clear legacy hover state
    _hovered_event.reset();
    _tooltip_timer->stop();
    QToolTip::hideText();
    update();
}

void EventPlotOpenGLWidget::handleTooltipTimer() {
    if (_event_visualization && _tooltips_enabled) {
        QString tooltip_text = _event_visualization->getEventTooltipText();
        if (!tooltip_text.isEmpty()) {
            QToolTip::showText(mapToGlobal(QPoint(_last_mouse_pos.x(), _last_mouse_pos.y())),
                               tooltip_text, this);
        }
    }
}

void EventPlotOpenGLWidget::handleTooltipRefresh() {
    if (_event_visualization && _tooltips_enabled) {
        QString tooltip_text = _event_visualization->getEventTooltipText();
        if (!tooltip_text.isEmpty()) {
            QToolTip::showText(mapToGlobal(_pending_hover_pos), tooltip_text, this);
        }
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

    qDebug() << "EventPlotOpenGLWidget::initializeBuffers - center line buffer created (static line)";
}

void EventPlotOpenGLWidget::updateMatrices() {
    // Update view matrix (pan and Y zoom only)
    _view_matrix.setToIdentity();
    _view_matrix.translate(_pan_offset_x, _pan_offset_y, 0.0f);
    _view_matrix.scale(1.0f, _y_zoom_level, 1.0f);// Only scale Y axis

    // Update projection matrix (orthographic)
    _projection_matrix.setToIdentity();

    auto bounds = calculateProjectionBounds();
    float left = bounds.min_x;
    float right = bounds.max_x;
    float bottom = bounds.min_y;
    float top = bounds.max_y;

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
    emit viewBoundsChanged(bounds);
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
        emit zoomLevelChanged(_y_zoom_level);// Emit Y zoom level change
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
    auto bounds = calculateProjectionBounds();
    float left = bounds.min_x;
    float right = bounds.max_x;

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
    // Events are now rendered through the EventPointVisualization in paintGL()
    // This method is kept for compatibility but is essentially a no-op
    qDebug() << "EventPlotOpenGLWidget::renderEvents - using EventPointVisualization system";
}

void EventPlotOpenGLWidget::setRowColorIndices(std::vector<uint32_t> const & row_color_indices) {
    _row_color_indices = row_color_indices;
    applyRowColorIndicesIfReady();
}

void EventPlotOpenGLWidget::applyRowColorIndicesIfReady() {
    if (!_event_visualization) return;
    if (_row_color_indices.empty()) return;
    // Construct per-point group ids: for each event mapping, use its trial index
    // EventPointVisualization stores one vertex per event with group id at index 2 of each vertex triplet
    // To avoid deep coupling, rebuild a per-point id array by scanning vertex data
    size_t total_points = _event_visualization->m_total_point_count;
    if (total_points == 0) return;
    std::vector<uint32_t> per_point_ids(total_points, 0u);
    // We need trial index per event. EventPointVisualization keeps mappings internally; not exposed.
    // Approximate by recomputing from _event_data order: points are appended trial by trial.
    size_t cursor = 0;
    for (size_t trial = 0; trial < _event_data.size(); ++trial) {
        uint32_t idx = (trial < _row_color_indices.size()) ? _row_color_indices[trial] : 0u;
        size_t count = _event_data[trial].size();
        for (size_t k = 0; k < count && cursor < per_point_ids.size(); ++k, ++cursor) {
            per_point_ids[cursor] = idx;
        }
    }
    _event_visualization->setPerPointGroupIds(per_point_ids);
    update();
}

void EventPlotOpenGLWidget::renderHoveredEvent() {
    // Hovered events are now rendered through the EventPointVisualization in paintGL()
    // This method is kept for compatibility but is essentially a no-op
    qDebug() << "EventPlotOpenGLWidget::renderHoveredEvent - using EventPointVisualization system";
}

void EventPlotOpenGLWidget::renderCenterLine() {
    // Use ShaderManager axes shader for center line
    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (!axesProgram) return;
    glUseProgram(axesProgram->getProgramId());
    auto nativeProgram = axesProgram->getNativeProgram();
    if (!nativeProgram) return;
    // Set up MVP
    QMatrix4x4 mvp = _projection_matrix * _view_matrix;
    int projLoc = nativeProgram->uniformLocation("projMatrix");
    int viewLoc = nativeProgram->uniformLocation("viewMatrix");
    int modelLoc = nativeProgram->uniformLocation("modelMatrix");
    int colorLoc = nativeProgram->uniformLocation("u_color");
    int alphaLoc = nativeProgram->uniformLocation("u_alpha");
    QMatrix4x4 identity;
    nativeProgram->setUniformValue(projLoc, _projection_matrix);
    nativeProgram->setUniformValue(viewLoc, _view_matrix);
    nativeProgram->setUniformValue(modelLoc, identity);
    // Set color: white for dark mode, black for light mode
    if (_plot_theme == PlotTheme::Dark) {
        nativeProgram->setUniformValue(colorLoc, QVector3D(1.0f, 1.0f, 1.0f));
    } else {
        nativeProgram->setUniformValue(colorLoc, QVector3D(0.0f, 0.0f, 0.0f));
    }
    nativeProgram->setUniformValue(alphaLoc, 1.0f);
    // Center line vertex data (4D)
    float lineVertices[8] = {0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    GLuint vbo, vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glDrawArrays(GL_LINES, 0, 2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
}

BoundingBox EventPlotOpenGLWidget::calculateProjectionBounds() const {
    if (_widget_width <= 0 || _widget_height <= 0) {
        // Use safe default bounds
        qDebug() << "EventPlotOpenGLWidget::calculateProjectionBounds - using default bounds with ranges";
        return BoundingBox(-static_cast<float>(_negative_range), -1.0f, static_cast<float>(_positive_range), 1.0f);
    }

    // Use the user-specified X-axis range (no X zoom, controlled by spinboxes)
    float x_range_width = static_cast<float>(_negative_range + _positive_range);
    float center_x = 0.0f;// Always center at 0

    // Y-axis is always normalized to [-1, 1] for trials, but apply Y zoom
    float y_range_height = 2.0f;
    float center_y = 0.0f;

    // Apply zoom: allow standardized X zoom via _zoom_level_x and Y zoom via _y_zoom_level
    float x_zoom_factor = 1.0f / _zoom_level_x;
    float y_zoom_factor = 1.0f / _y_zoom_level;

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

    float left = center_x - half_width + pan_x;
    float right = center_x + half_width + pan_x;
    float bottom = center_y - half_height + pan_y;
    float top = center_y + half_height + pan_y;

    return BoundingBox(left, bottom, right, top);
}

void EventPlotOpenGLWidget::processHoverDebounce() {
    if (!_tooltips_enabled || _hover_processing_active || !_event_visualization) {
        return;
    }

    _hover_processing_active = true;

    // Get the screen coordinates and convert to world coordinates
    QPoint screen_pos = _pending_hover_pos;
    float world_x, world_y;
    screenToWorld(screen_pos.x(), screen_pos.y(), world_x, world_y);
    QVector2D world_pos(world_x, world_y);
    
    // Calculate tolerance in world coordinates
    float tolerance = calculateWorldTolerance(10.0f); // 10 pixel tolerance

    // Use the event visualization's hover handling
    bool hover_changed = _event_visualization->handleHover(world_pos, tolerance);

    if (hover_changed) {
        if (_event_visualization->m_current_hover_point) {
            // Start tooltip timer
            _tooltip_timer->start(500); // 500ms delay
        } else {
            // Hide tooltip immediately
            _tooltip_timer->stop();
            QToolTip::hideText();
        }

        update();
    }

    _hover_processing_active = false;
}

float EventPlotOpenGLWidget::calculateWorldTolerance(float screen_tolerance) const {
    // Calculate projection bounds
    auto bounds = calculateProjectionBounds();

    // Calculate world units per pixel
    float world_width = bounds.width();
    float world_height = bounds.height();
    float world_per_pixel_x = world_width / _widget_width;
    float world_per_pixel_y = world_height / _widget_height;

    // Use the smaller of the two to ensure we don't miss events
    float world_per_pixel = std::min(world_per_pixel_x, world_per_pixel_y);

    return screen_tolerance * world_per_pixel;
}

void EventPlotOpenGLWidget::screenToWorld(int screen_x, int screen_y, float & world_x, float & world_y) {
    // Calculate projection bounds
    auto bounds = calculateProjectionBounds();

    // Convert screen coordinates to normalized coordinates [0, 1]
    float norm_x = static_cast<float>(screen_x) / _widget_width;
    float norm_y = 1.0f - static_cast<float>(screen_y) / _widget_height;// Flip Y axis

    // Convert to world coordinates
    world_x = bounds.min_x + norm_x * (bounds.width());
    world_y = bounds.min_y + norm_y * (bounds.height());
}