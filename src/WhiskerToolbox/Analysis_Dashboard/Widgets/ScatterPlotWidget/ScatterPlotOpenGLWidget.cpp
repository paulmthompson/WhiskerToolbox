#include "ScatterPlotOpenGLWidget.hpp"

#include "Analysis_Dashboard/Widgets/Common/PlotInteractionController.hpp"
#include "Analysis_Dashboard/Widgets/Common/ViewAdapter.hpp"
#include "Groups/GroupManager.hpp"
#include "ScatterPlotViewAdapter.hpp"
#include "Selection/LineSelectionHandler.hpp"
#include "Selection/NoneSelectionHandler.hpp"
#include "Selection/PointSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"
#include "Visualizers/Points/ScatterPlotVisualization.hpp"

#include <QDebug>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QRandomGenerator>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>


ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : _group_manager(nullptr),
      _point_size(3.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _tooltips_enabled(true),
      _opengl_resources_initialized(false),
      _data_min_x(0.0f),
      _data_max_x(1.0f),
      _data_min_y(0.0f),
      _data_max_y(1.0f),
      _data_bounds_valid(false),
      _selection_mode(SelectionMode::PointSelection) {

    // Ensure the widget receives continuous mouse move and wheel events
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    _selection_handler = std::make_unique<PointSelectionHandler>(10.0f);// Use fixed tolerance initially

    // Setup tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    connect(_tooltip_timer, &QTimer::timeout, this, &ScatterPlotOpenGLWidget::handleTooltipTimer);

    // FPS limiter timer (30 FPS = ~33ms interval)
    _fps_limiter_timer = new QTimer(this);
    _fps_limiter_timer->setSingleShot(true);
    _fps_limiter_timer->setInterval(33);// ~30 FPS
    connect(_fps_limiter_timer, &QTimer::timeout, this, [this]() {
        if (_pending_update) {
            _pending_update = false;
            update();
        }
    });
    _pending_update = false;

    // Set OpenGL format
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);// Enable multisampling for anti-aliasing
    setFormat(format);

    // Configure paint behavior for embedding inside QGraphicsProxyWidget
    if (parent) setParent(parent);
    setAttribute(Qt::WA_AlwaysStackOnTop, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    // Initialize standardized interaction state
    _zoom_level_x = 1.0f;
    _zoom_level_y = 1.0f;
    _padding_factor = 1.1f;

    _interaction = std::make_unique<PlotInteractionController>(this, std::make_unique<ScatterPlotViewAdapter>(this));
    connect(_interaction.get(), &PlotInteractionController::viewBoundsChanged, this, &ScatterPlotOpenGLWidget::viewBoundsChanged);
    connect(_interaction.get(), &PlotInteractionController::mouseWorldMoved, this, &ScatterPlotOpenGLWidget::mouseWorldMoved);
}

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget() {
    makeCurrent();
    _scatter_visualization.reset();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setScatterData(std::vector<float> const & x_data,
                                             std::vector<float> const & y_data) {
    qDebug() << "ScatterPlotOpenGLWidget::setScatterData called with"
             << x_data.size() << "x points and" << y_data.size() << "y points";

    if (x_data.size() != y_data.size()) {
        qDebug() << "ScatterPlotOpenGLWidget::setScatterData: X and Y data vectors must have the same size";
        return;
    }

    if (x_data.empty()) {
        qDebug() << "ScatterPlotOpenGLWidget::setScatterData: Data vectors are empty";
        return;
    }

    // Store data for later use
    _x_data = x_data;
    _y_data = y_data;

    // Calculate data bounds
    calculateDataBounds();

    qDebug() << "ScatterPlotOpenGLWidget::setScatterData: Data bounds:"
             << _data_min_x << "," << _data_min_y << "to" << _data_max_x << "," << _data_max_y;

    qDebug() << "Creating ScatterPlotVisualization with" << x_data.size() << "points";

    // Create new visualization with the data (using deferred initialization)
    _scatter_visualization = std::make_unique<ScatterPlotVisualization>(
            "scatter_data",
            x_data,
            y_data,
            _group_manager,
            true// defer_opengl_init = true
    );

    // If OpenGL is already initialized, initialize the visualization resources
    if (_opengl_resources_initialized && context() && context()->isValid()) {
        makeCurrent();
        try {
            _scatter_visualization->initializeOpenGLResources();
            qDebug() << "ScatterPlotVisualization OpenGL resources initialized successfully";
        } catch (...) {
            qWarning() << "Failed to initialize ScatterPlotVisualization OpenGL resources";
        }
        doneCurrent();
    }

    // Update projection matrix based on data bounds
    updateProjectionMatrix();

    update();

    qDebug() << "ScatterPlotOpenGLWidget::setScatterData completed, widget updated";
}

void ScatterPlotOpenGLWidget::setAxisLabels(QString const & x_label, QString const & y_label) {
    if (_scatter_visualization) {
        _scatter_visualization->setAxisLabels(x_label, y_label);
    }
}

void ScatterPlotOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;

    if (_scatter_visualization) {
        _scatter_visualization->setGroupManager(group_manager);
    }
}

void ScatterPlotOpenGLWidget::setPointSize(float point_size) {
    _point_size = point_size;

    if (_scatter_visualization) {
        //_scatter_visualization->setPointSize(point_size); // TODO
    }

    update();
}

void ScatterPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    _tooltips_enabled = enabled;

    if (!_tooltips_enabled) {
        _tooltip_timer->stop();
        QToolTip::hideText();
    }
}

void ScatterPlotOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Set clear color
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    // Initialize projection matrix
    updateProjectionMatrix();

    // Mark OpenGL resources as initialized
    _opengl_resources_initialized = true;

    // Initialize any pending visualizations
    if (_scatter_visualization) {
        try {
            _scatter_visualization->initializeOpenGLResources();
            qDebug() << "ScatterPlotVisualization OpenGL resources initialized in initializeGL";
        } catch (...) {
            qWarning() << "Failed to initialize ScatterPlotVisualization OpenGL resources in initializeGL";
        }
    }

    qDebug() << "ScatterPlotOpenGLWidget: OpenGL initialized";
}

void ScatterPlotOpenGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!_opengl_resources_initialized) {
        qDebug() << "ScatterPlotOpenGLWidget::paintGL: OpenGL resources not initialized yet";
        return;
    }

    if (!_scatter_visualization) {
        qDebug() << "ScatterPlotOpenGLWidget::paintGL: No visualization available";
        return;
    }

    // Set up matrices consistently with SpatialOverlayOpenGLWidget
    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;

    qDebug() << "ScatterPlotOpenGLWidget::paintGL: Rendering with point size" << _point_size;
    qDebug() << "ScatterPlotOpenGLWidget::paintGL: Using projection matrix:" << _projection_matrix;

    // Render the scatter plot visualization
    _scatter_visualization->render(mvp_matrix, _point_size);
}

void ScatterPlotOpenGLWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);

    // Update projection matrix based on data bounds
    updateProjectionMatrix();
}

void ScatterPlotOpenGLWidget::updateProjectionMatrix() {
    qDebug() << "ScatterPlotOpenGLWidget::updateProjectionMatrix called";
    _model_matrix.setToIdentity();

    if (!_data_bounds_valid || width() <= 0 || height() <= 0) {
        _projection_matrix.setToIdentity();
        _view_matrix.setToIdentity();
        return;
    }

    float cx, cy, w_world, h_world;
    computeCameraWorldView(cx, cy, w_world, h_world);

    // View: V = S * T(-center)
    _view_matrix.setToIdentity();
    float aspect = static_cast<float>(width()) / std::max(1, height());
    float scale_x = (w_world > 0.0f) ? ((2.0f * aspect) / w_world) : 1.0f;
    float scale_y = (h_world > 0.0f) ? (2.0f / h_world) : 1.0f;
    _view_matrix.scale(scale_x, scale_y, 1.0f);
    _view_matrix.translate(-cx, -cy, 0.0f);

    // Projection: aspect-only orthographic
    _projection_matrix.setToIdentity();
    float left = -aspect;
    float right = aspect;
    float bottom = -1.0f;
    float top = 1.0f;
    _projection_matrix.ortho(left, right, bottom, top, -1.0f, 1.0f);

    // Emit current visible bounds
    float half_w = 0.5f * w_world;
    float half_h = 0.5f * h_world;
    emit viewBoundsChanged(cx - half_w, cx + half_w, cy - half_h, cy + half_h);
}

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    qDebug() << "ScatterPlotOpenGLWidget::mousePressEvent called at" << event->pos();

    if (_interaction && _interaction->handleMousePress(event)) {
        return;
    } else if (event->button() == Qt::LeftButton) {
        _last_mouse_pos = event->pos();
        qDebug() << "ScatterPlotOpenGLWidget::mousePressEvent: Started panning";

        // Check for point click
        if (_scatter_visualization) {
            QVector2D world_pos = screenToWorld(event->pos());
            // TODO: Implement point picking/selection
        }
    }
    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    _current_mouse_pos = event->pos();
    if (_interaction && _interaction->handleMouseMove(event)) return;

    auto world_pos = screenToWorld(event->pos());
    emit mouseWorldMoved(world_pos.x(), world_pos.y());
    std::visit([event, world_pos](auto & handler) {
        handler->mouseMoveEvent(event, world_pos);
    },
               _selection_handler);

    if (!(_interaction && _interaction->handleMouseMove(event))) {
        if (_last_mouse_pos == _current_mouse_pos) {
            return;
        } else {
            qDebug() << "ScatterPlotOpenGLWidget: Mouse moved from"
                     << _last_mouse_pos << "to" << _current_mouse_pos;
        }

        if (_tooltips_enabled) {
            // Store the current mouse position for debounced processing
            /*
            _pending_hover_pos = _current_mouse_pos;

            // If hover processing is currently active, skip this event
            if (_hover_processing_active) {
                qDebug() << "ScatterPlotOpenGLWidget: Skipping hover processing - already active";
                return;
            }

            // Start or restart the debounce timer
            _hover_debounce_timer->stop();
            _hover_debounce_timer->start();

            qDebug() << "ScatterPlotOpenGLWidget: Starting hover debounce timer for position" << _pending_hover_pos;
        */
        }
        _last_mouse_pos = _current_mouse_pos;
        event->accept();
    }
}

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (_interaction && _interaction->handleMouseRelease(event)) {
        return;
    } else if (event->button() == Qt::LeftButton) {
    }
    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    if (_interaction && _interaction->handleWheel(event)) return;
    QOpenGLWidget::wheelEvent(event);
}

void ScatterPlotOpenGLWidget::leaveEvent(QEvent * event) {
    Q_UNUSED(event)

    // Hide tooltip when mouse leaves widget
    _tooltip_timer->stop();
    QToolTip::hideText();
    if (_interaction) _interaction->handleLeave();
}

void ScatterPlotOpenGLWidget::handleTooltipTimer() {
    if (!_tooltips_enabled || !_scatter_visualization) {
        return;
    }

    // Convert mouse position to world coordinates and check for point under cursor
    QVector2D world_pos = screenToWorld(_tooltip_mouse_pos);

    // TODO: Implement proper point picking for tooltips
    // For now, show a basic tooltip
}

QVector2D ScatterPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    // Convert screen to NDC
    float x_ndc = (2.0f * screen_pos.x()) / std::max(1, width()) - 1.0f;
    float y_ndc = 1.0f - (2.0f * screen_pos.y()) / std::max(1, height());

    // Invert full MVP (model is identity)
    QMatrix4x4 mvp = _projection_matrix * _view_matrix * _model_matrix;
    QMatrix4x4 inv = mvp.inverted();
    QVector4D world4 = inv * QVector4D(x_ndc, y_ndc, 0.0f, 1.0f);
    return QVector2D(world4.x(), world4.y());
}

void ScatterPlotOpenGLWidget::handleMouseHover(QPoint const & pos) {
    if (!_tooltips_enabled) {
        return;
    }

    _tooltip_mouse_pos = pos;
    _tooltip_timer->start(TOOLTIP_DELAY_MS);
}

void ScatterPlotOpenGLWidget::calculateDataBounds() {
    if (_x_data.empty() || _y_data.empty()) {
        _data_bounds_valid = false;
        return;
    }

    _data_min_x = _data_max_x = _x_data[0];
    _data_min_y = _data_max_y = _y_data[0];

    for (size_t i = 1; i < _x_data.size(); ++i) {
        _data_min_x = std::min(_data_min_x, _x_data[i]);
        _data_max_x = std::max(_data_max_x, _x_data[i]);
        _data_min_y = std::min(_data_min_y, _y_data[i]);
        _data_max_y = std::max(_data_max_y, _y_data[i]);
    }

    // Add some padding
    float padding_x = (_data_max_x - _data_min_x) * 0.1f;
    float padding_y = (_data_max_y - _data_min_y) * 0.1f;

    _data_min_x -= padding_x;
    _data_max_x += padding_x;
    _data_min_y -= padding_y;
    _data_max_y += padding_y;

    _data_bounds_valid = true;
}

void ScatterPlotOpenGLWidget::calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const {
    if (!_data_bounds_valid || width() <= 0 || height() <= 0) {
        left = right = bottom = top = 0.0f;
        return;
    }
    float cx, cy, w_world, h_world;
    computeCameraWorldView(cx, cy, w_world, h_world);
    left = cx - 0.5f * w_world;
    right = cx + 0.5f * w_world;
    bottom = cy - 0.5f * h_world;
    top = cy + 0.5f * h_world;
}

void ScatterPlotOpenGLWidget::computeCameraWorldView(float & center_x,
                                                     float & center_y,
                                                     float & world_width,
                                                     float & world_height) const {
    float data_width = std::max(1e-6f, _data_max_x - _data_min_x);
    float data_height = std::max(1e-6f, _data_max_y - _data_min_y);
    float cx0 = (_data_min_x + _data_max_x) * 0.5f;
    float cy0 = (_data_min_y + _data_max_y) * 0.5f;
    float padding = _padding_factor;
    float half_w = 0.5f * (data_width * padding) / std::max(1e-6f, _zoom_level_x);
    float half_h = 0.5f * (data_height * padding) / std::max(1e-6f, _zoom_level_y);
    float pan_x_world = _pan_offset_x * (data_width / std::max(1e-6f, _zoom_level_x));
    float pan_y_world = _pan_offset_y * (data_height / std::max(1e-6f, _zoom_level_y));
    center_x = cx0 + pan_x_world;
    center_y = cy0 + pan_y_world;
    world_width = 2.0f * half_w;
    world_height = 2.0f * half_h;
}

void ScatterPlotOpenGLWidget::requestThrottledUpdate() {
    qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate called, timer active:" << _fps_limiter_timer->isActive();

    if (!_fps_limiter_timer->isActive()) {
        // If timer is not running, update immediately and start timer
        qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate: Updating immediately";
        emit highlightStateChanged(); // Same as SpatialOverlayOpenGLWidget::requestThrottledUpdate
        update();
        _fps_limiter_timer->start();
    } else {
        // Timer is running, just mark that we have a pending update
        qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate: Marking pending update";
        _pending_update = true;
    }
}
