#include "BasePlotOpenGLWidget.hpp"
#include "GenericViewAdapter.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "PlotInteractionController.hpp"
#include "Selection/ISelectionHandler.hpp"
#include "Selection/LineSelectionHandler.hpp"
#include "Selection/NoneSelectionHandler.hpp"
#include "Selection/PointSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"
#include "TooltipManager.hpp"
#include "widget_utilities.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>

BasePlotOpenGLWidget::BasePlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _group_manager(nullptr),
      _point_size(8.0f),
      _line_width(2.0f),
      _tooltips_enabled(true),
      _opengl_resources_initialized(false),
      _pending_update(false) {

    // Set up widget properties
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Initialize default selection handler
    _selection_callback = nullptr;
    createSelectionHandler(_selection_mode);

    // Set up OpenGL context with subclass-specified requirements
    auto [major, minor] = getRequiredOpenGLVersion();
    int samples = getRequiredSamples();

    QSurfaceFormat format;
    format.setVersion(major, minor);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(samples);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    setFormat(format);

    // Set up FPS limiter timer
    _fps_limiter_timer = new QTimer(this);
    _fps_limiter_timer->setSingleShot(true);
    _fps_limiter_timer->setInterval(33);// ~30 FPS
    connect(_fps_limiter_timer, &QTimer::timeout, this, [this]() {
        if (_pending_update) {
            _pending_update = false;
            update();
        }
    });

    // Initialize tooltip manager
    _tooltip_manager = std::make_unique<TooltipManager>(this);
    _tooltip_manager->setContentProvider([this](QPoint const & screen_pos) -> std::optional<QString> {
        return generateTooltipContent(screen_pos);
    });

    qDebug() << "BasePlotOpenGLWidget: Created base plot widget with OpenGL"
             << major << "." << minor << "and" << samples << "samples";
}

BasePlotOpenGLWidget::~BasePlotOpenGLWidget() {
    // Ensure OpenGL context is current for resource cleanup
    if (_opengl_resources_initialized && context() && context()->isValid()) {
        makeCurrent();
        _preview_renderer.cleanup();
        doneCurrent();
    }
}

void BasePlotOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;

    qDebug() << "BasePlotOpenGLWidget: Set group manager";
    
    // Enforce derived class implementation
    doSetGroupManager(group_manager);
}

void BasePlotOpenGLWidget::setPointSize(float point_size) {
    float new_point_size = std::max(1.0f, std::min(50.0f, point_size));
    if (new_point_size != _point_size) {
        _point_size = new_point_size;
        requestThrottledUpdate();
    }
}

void BasePlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    _tooltips_enabled = enabled;
    if (_tooltip_manager) {
        _tooltip_manager->setEnabled(enabled);
    }
}

QVector2D BasePlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    // Update view state dimensions for accurate conversion
    _view_state.viewport_width = width();
    _view_state.viewport_height = height();

    auto world_point = CorePlotting::screenToWorld(_view_state, screen_pos.x(), screen_pos.y());
    return {world_point.x, world_point.y};
}

QPoint BasePlotOpenGLWidget::worldToScreen(float world_x, float world_y) const {
    // Update view state dimensions for accurate conversion
    _view_state.viewport_width = width();
    _view_state.viewport_height = height();

    auto screen_point = CorePlotting::worldToScreen(_view_state, world_x, world_y);
    return QPoint{static_cast<int>(screen_point.x), static_cast<int>(screen_point.y)};
}

void BasePlotOpenGLWidget::resetView() {
    // Ensure ViewState has current data bounds before resetting
    auto data_bounds = getDataBounds();
    _view_state.data_bounds = data_bounds;
    _view_state.data_bounds_valid = true;
    _view_state.viewport_width = width();
    _view_state.viewport_height = height();

    CorePlotting::resetView(_view_state);
    updateViewMatrices();
    requestThrottledUpdate();
}

void BasePlotOpenGLWidget::paintGL() {
    if (!initializeRendering()) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderBackground();
    renderData();// Pure virtual - implemented by subclasses
    renderOverlays();
    renderUI();
}

void BasePlotOpenGLWidget::initializeGL() {
    qDebug() << "BasePlotOpenGLWidget::initializeGL called";

    initializeOpenGLFunctions();

    // Validate that we got the requested OpenGL version
    if (!validateOpenGLContext()) {
        qWarning() << "BasePlotOpenGLWidget: OpenGL context validation failed";
        // Continue anyway, but subclasses should check capabilities
    }

    // Set up OpenGL state
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    _opengl_resources_initialized = true;

    // Initialize preview renderer for selection overlay rendering
    // (Phase 1 of selection handler refactoring roadmap)
    if (!_preview_renderer.initialize()) {
        qWarning() << "BasePlotOpenGLWidget: Failed to initialize PreviewRenderer";
    }

    // Create interaction controller if not already created
    if (!_interaction) {
        // This will be overridden by subclasses to provide their specific view adapter
        // For now, we'll delay creation until subclass is ready
    }

    updateViewMatrices();

    qDebug() << "BasePlotOpenGLWidget::initializeGL completed";
}

void BasePlotOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateViewMatrices();
}

void BasePlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {

    if (_tooltip_manager) {
        _tooltip_manager->setSuppressed(true);
    }

    if (_interaction) {
        _interaction->handleMousePress(event);
    }

    //if (!hasFocus()) {
    //    setFocus(Qt::MouseFocusReason);
    //}

    auto world_pos = screenToWorld(event->pos());

    if (_selection_handler) {
        qDebug() << "BasePlotOpenGLWidget: Forwarding mousePressEvent to selection handler";
        _selection_handler->mousePressEvent(event, world_pos);
    }

    requestThrottledUpdate();

    // Default behavior if interaction controller doesn't handle it
    QOpenGLWidget::mousePressEvent(event);
}

void BasePlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    if (_interaction && _interaction->handleMouseMove(event)) {
        return;
    }

    // Update tooltip manager with mouse position
    if (_tooltip_manager) {
        _tooltip_manager->handleMouseMove(event->globalPos()); // Global Pos is DEPRECATED
    }

    // Emit world coordinates for other components to use
    auto world_pos = screenToWorld(event->pos());
    emit mouseWorldMoved(world_pos.x(), world_pos.y());

    if (_selection_handler) {
        _selection_handler->mouseMoveEvent(event, world_pos);
    }

    requestThrottledUpdate();
    QOpenGLWidget::mouseMoveEvent(event);
}

void BasePlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    // Re-enable tooltips after interaction
    if (_tooltip_manager) {
        _tooltip_manager->setSuppressed(false);
    }

    if (_interaction && _interaction->handleMouseRelease(event)) {
        return;
    }

    auto world_pos = screenToWorld(event->pos());

    if (_selection_handler) {
        _selection_handler->mouseReleaseEvent(event, world_pos);
    }


    QOpenGLWidget::mouseReleaseEvent(event);
}

void BasePlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    if (_interaction && _interaction->handleWheel(event)) {
        return;
    }

    QOpenGLWidget::wheelEvent(event);
}

void BasePlotOpenGLWidget::leaveEvent(QEvent * event) {
    if (_interaction) {
        _interaction->handleLeave();
    }

    // Hide tooltip when mouse leaves widget
    if (_tooltip_manager) {
        _tooltip_manager->handleMouseLeave();
    }

    QOpenGLWidget::leaveEvent(event);
}

void BasePlotOpenGLWidget::handleKeyPress(QKeyEvent * event) {

    if (_selection_handler) {
        _selection_handler->keyPressEvent(event);
    }

    switch (event->key()) {
        case Qt::Key_R:
            // Reset view
            resetView();
            event->accept();
            break;
        default:
            // Let the widget handle it normally
            keyPressEvent(event);
            break;
    }
}

void BasePlotOpenGLWidget::setSelectionMode(SelectionMode mode) {
    qDebug() << "BasePlotOpenGLWidget::setSelectionMode called with mode:" << static_cast<int>(mode);
    
    if (_selection_mode != mode) {
        _selection_mode = mode;

        createSelectionHandler(mode);
        
        qDebug() << "BasePlotOpenGLWidget: Selection mode changed to:" << static_cast<int>(mode);

        emit selectionModeChanged(mode);
    }
}


void BasePlotOpenGLWidget::createSelectionHandler(SelectionMode mode) {
    // Ensure OpenGL context is current for handlers that need OpenGL functions
    if (mode == SelectionMode::PolygonSelection || mode == SelectionMode::LineIntersection) {
        makeCurrent();
    }

    switch (mode) {
        case SelectionMode::None:
            qDebug() << "BasePlotOpenGLWidget: Creating NoneSelectionHandler";
            _selection_handler = std::make_unique<NoneSelectionHandler>();
            break;
        case SelectionMode::PointSelection:
            qDebug() << "BasePlotOpenGLWidget: Creating PointSelectionHandler";
            _selection_handler = std::make_unique<PointSelectionHandler>(10.0f);// 10 pixel tolerance
            break;
        case SelectionMode::PolygonSelection:
            qDebug() << "BasePlotOpenGLWidget: Creating PolygonSelectionHandler";
            _selection_handler = std::make_unique<PolygonSelectionHandler>();
            break;
        case SelectionMode::LineIntersection:
            qDebug() << "BasePlotOpenGLWidget: Creating LineSelectionHandler";
            _selection_handler = std::make_unique<LineSelectionHandler>();
            qDebug() << "BasePlotOpenGLWidget: LineSelectionHandler created successfully";
            break;
    }

    // Set up notification callback for the handler
    if (_selection_handler) {
        _selection_handler->setNotificationCallback(_selection_callback);
    }
}

void BasePlotOpenGLWidget::renderBackground() {
    // Default implementation - just clear
    // Subclasses can override for custom backgrounds
}

void BasePlotOpenGLWidget::renderOverlays() {
    // Render selection handler previews using PreviewRenderer
    // This unified approach replaces the old per-handler render() methods
    if (_selection_handler && _preview_renderer.isInitialized() && _selection_handler->isActive()) {
        auto preview = _selection_handler->getPreview();
        if (preview.isValid()) {
            _preview_renderer.render(preview, width(), height());
        }
    }
}

void BasePlotOpenGLWidget::renderUI() {
    // Default implementation for axes, labels, etc.
    // Subclasses can override for custom UI elements
}

std::optional<QString> BasePlotOpenGLWidget::generateTooltipContent(QPoint const & screen_pos) const {
    // Default implementation - subclasses should override to provide specific tooltip content
    Q_UNUSED(screen_pos)
    return std::nullopt;
}

RenderingContext BasePlotOpenGLWidget::createRenderingContext() const {
    RenderingContext context;
    context.model_matrix = _model_matrix;
    context.view_matrix = _view_matrix;
    context.projection_matrix = _projection_matrix;
    context.viewport_rect = QRect(0, 0, width(), height());

    // Calculate world bounds based on current view
    auto data_bounds = getDataBounds();
    float cx, cy, w_world, h_world;
    computeCameraWorldView(cx, cy, w_world, h_world);

    float half_w = w_world * 0.5f;
    float half_h = h_world * 0.5f;
    context.world_bounds = QRectF(cx - half_w, cy - half_h, w_world, h_world);

    return context;
}

void BasePlotOpenGLWidget::updateViewMatrices() {
    _model_matrix.setToIdentity();

    auto data_bounds = getDataBounds();

    // Update ViewState with current data bounds and widget dimensions
    _view_state.data_bounds = data_bounds;
    _view_state.data_bounds_valid = true;
    _view_state.viewport_width = width();
    _view_state.viewport_height = height();

    if (width() <= 0 || height() <= 0) {
        _view_matrix.setToIdentity();
        _projection_matrix.setToIdentity();
        return;
    }

    // Use CorePlotting to compute matrices
    glm::mat4 view, proj;
    CorePlotting::computeMatricesFromViewState(_view_state, view, proj);

    // Convert glm::mat4 to QMatrix4x4 (glm is column-major, QMatrix4x4 constructor takes row-major elements)
    // So we pass m[0][0], m[1][0], m[2][0], m[3][0] etc.
    auto glmToQt = [](const glm::mat4& m) {
        return QMatrix4x4(
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]
        );
    };

    _view_matrix = glmToQt(view);
    _projection_matrix = glmToQt(proj);

    // Emit the current visible world bounds
    float cx, cy, w_world, h_world;
    computeCameraWorldView(cx, cy, w_world, h_world);
    float half_w = w_world * 0.5f;
    float half_h = h_world * 0.5f;
    BoundingBox view_bounds(cx - half_w, cy - half_h, cx + half_w, cy + half_h);
    emit viewBoundsChanged(view_bounds);
}

void BasePlotOpenGLWidget::requestThrottledUpdate() {
    if (!_fps_limiter_timer->isActive()) {
        update();
        emit highlightStateChanged();
        _fps_limiter_timer->start();
    } else {
        _pending_update = true;
    }
}

bool BasePlotOpenGLWidget::initializeRendering() {
    if (!_opengl_resources_initialized) {
        qDebug() << "BasePlotOpenGLWidget::initializeRendering: OpenGL resources not initialized yet";
        return false;
    }

    if (!context() || !context()->isValid()) {
        qWarning() << "BasePlotOpenGLWidget::initializeRendering: Invalid OpenGL context";
        return false;
    }

    return true;
}

void BasePlotOpenGLWidget::computeCameraWorldView(float & center_x, float & center_y,
                                                  float & world_width, float & world_height) const {
    auto bounds = CorePlotting::calculateVisibleWorldBounds(_view_state);
    center_x = bounds.center_x();
    center_y = bounds.center_y();
    world_width = bounds.width();
    world_height = bounds.height();
}

bool BasePlotOpenGLWidget::validateOpenGLContext() const {
    if (!context() || !context()->isValid()) {
        qWarning() << "BasePlotOpenGLWidget: OpenGL context is invalid";
        return false;
    }

    auto fmt = format();
    auto [req_major, req_minor] = getRequiredOpenGLVersion();

    qDebug() << "BasePlotOpenGLWidget: Requested OpenGL" << req_major << "." << req_minor;
    qDebug() << "BasePlotOpenGLWidget: Actual OpenGL" << fmt.majorVersion() << "." << fmt.minorVersion();

    // Check if we got at least the requested version
    if (fmt.majorVersion() < req_major ||
        (fmt.majorVersion() == req_major && fmt.minorVersion() < req_minor)) {
        qWarning() << "BasePlotOpenGLWidget: Requested OpenGL" << req_major << "." << req_minor
                   << "but got" << fmt.majorVersion() << "." << fmt.minorVersion();
        return false;
    }

    // Check profile
    if (fmt.profile() != QSurfaceFormat::CoreProfile) {
        qWarning() << "BasePlotOpenGLWidget: Expected Core profile but got" << fmt.profile();
        return false;
    }

    qDebug() << "BasePlotOpenGLWidget: OpenGL context validation successful";
    return true;
}
