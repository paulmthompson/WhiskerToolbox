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
#include "Widgets/Common/widget_utilities.hpp"

#include <QDebug>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QRandomGenerator>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>


ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : _group_manager(nullptr),
      _point_size(8.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _tooltips_enabled(true),
      _opengl_resources_initialized(false),
      _data_bounds(0.0f, 0.0f, 1.0f, 1.0f),
      _data_bounds_valid(false),
      _selection_mode(SelectionMode::PointSelection) {

    if (parent) setParent(parent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    try_create_opengl_context_with_version(this, 4, 1);

    _selection_handler = std::make_unique<PointSelectionHandler>(10.0f);// Use fixed tolerance initially

    /*
    std::visit([this](auto & handler) {
        if (handler) {
            handler->setNotificationCallback([this]() { makeSelection(); });
        }
    },
               _selection_handler);
               */

    // Setup tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    connect(_tooltip_timer, &QTimer::timeout, this, &ScatterPlotOpenGLWidget::_handleTooltipTimer);

    _tooltip_refresh_timer = new QTimer(this);
    _tooltip_refresh_timer->setInterval(100);// Refresh every 100ms to keep tooltip visible
    connect(_tooltip_refresh_timer, &QTimer::timeout, this, &ScatterPlotOpenGLWidget::_handleTooltipRefresh);

    // FPS limiter timer (30 FPS = ~33ms interval)
    _fps_limiter_timer = new QTimer(this);
    _fps_limiter_timer->setSingleShot(true);
    _fps_limiter_timer->setInterval(33);// ~30 FPS
    connect(_fps_limiter_timer, &QTimer::timeout, this, [this]() {
        if (_pending_update) {
            _pending_update = false;
            update();
            emit highlightStateChanged();
        }
    });
    _pending_update = false;

    _data_bounds = BoundingBox(0.0f, 0.0f, 0.0f, 0.0f);

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

void ScatterPlotOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;

    // Connect to group manager signals to handle updates
    if (_group_manager) {
        connect(_group_manager, &GroupManager::pointAssignmentsChanged,
                this, [this](std::unordered_set<int> const & affected_groups) {
                    Q_UNUSED(affected_groups)
                    // Refresh visualization data for all point visualizations
                    _scatter_visualization->refreshGroupRenderData();
                    update();// Trigger a repaint
                });

        connect(_group_manager, &GroupManager::groupModified,
                this, [this](int group_id) {
                    Q_UNUSED(group_id)
                    // Color changes don't require vertex data refresh, just re-render
                    update();
                });
    }

    if (_scatter_visualization) {
        _scatter_visualization->setGroupManager(group_manager);
    }
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
             << _data_bounds.min_x << "," << _data_bounds.min_y << "to" << _data_bounds.max_x << "," << _data_bounds.max_y;

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

void ScatterPlotOpenGLWidget::setPointSize(float point_size) {
    _point_size = point_size;

    if (_scatter_visualization) {
        //_scatter_visualization->setPointSize(point_size); // TODO
    }

    requestThrottledUpdate();
}

//========== OpenGL Initialization ==========

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

//========== View and MVP Matrices ==========


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

//========== Mouse Events ==========

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

void ScatterPlotOpenGLWidget::keyPressEvent(QKeyEvent * event) {
    qDebug() << "ScatterPlotOpenGLWidget::keyPressEvent - Key:" << event->key() << "Text:" << event->text();

    event->accept();
}

//========== Tooltips ==========

void ScatterPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    _tooltips_enabled = enabled;

    if (!_tooltips_enabled) {
        _tooltip_timer->stop();
        QToolTip::hideText();
    }
}

void ScatterPlotOpenGLWidget::_handleTooltipTimer() {
    if (!_tooltips_enabled || !_scatter_visualization) {
        return;
    }

    // Convert mouse position to world coordinates and check for point under cursor
    QVector2D world_pos = screenToWorld(_tooltip_mouse_pos);

    // TODO: Implement proper point picking for tooltips
    // For now, show a basic tooltip
}

void ScatterPlotOpenGLWidget::_handleTooltipRefresh() {
    if (!_tooltips_enabled || !_scatter_visualization) {
        _tooltip_refresh_timer->stop();
        return;
    }

    _handleTooltipTimer();
}


void ScatterPlotOpenGLWidget::calculateDataBounds() {
    if (_x_data.empty() || _y_data.empty()) {
        _data_bounds_valid = false;
        return;
    }

    float min_x = _x_data[0];
    float max_x = _x_data[0];
    float min_y = _y_data[0];
    float max_y = _y_data[0];

    for (size_t i = 1; i < _x_data.size(); ++i) {
        min_x = std::min(min_x, _x_data[i]);
        max_x = std::max(max_x, _x_data[i]);
        min_y = std::min(min_y, _y_data[i]);
        max_y = std::max(max_y, _y_data[i]);
    }

    // Add some padding
    float padding_x = (max_x - min_x) * 0.1f;
    float padding_y = (max_y - min_y) * 0.1f;

    _data_bounds = BoundingBox(min_x - padding_x, min_y - padding_y,
                               max_x + padding_x, max_y + padding_y);
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
    compute_camera_world_view(_data_bounds, _zoom_level_x, _zoom_level_y,
                              _pan_offset_x, _pan_offset_y, _padding_factor,
                              center_x, center_y, world_width, world_height);
}

void ScatterPlotOpenGLWidget::requestThrottledUpdate() {
    qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate called, timer active:" << _fps_limiter_timer->isActive();

    if (!_fps_limiter_timer->isActive()) {
        // If timer is not running, update immediately and start timer
        qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate: Updating immediately";
        emit highlightStateChanged();// Same as SpatialOverlayOpenGLWidget::requestThrottledUpdate
        update();
        _fps_limiter_timer->start();
    } else {
        // Timer is running, just mark that we have a pending update
        qDebug() << "ScatterPlotOpenGLWidget::requestThrottledUpdate: Marking pending update";
        _pending_update = true;
    }
}
