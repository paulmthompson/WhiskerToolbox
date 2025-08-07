#include "ScatterPlotOpenGLWidget.hpp"


#include "Groups/GroupManager.hpp"
#include "Visualizers/Points/ScatterPlotVisualization.hpp"

#include <QDebug>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QRandomGenerator>
#include <QToolTip>
#include <QWheelEvent>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _group_manager(nullptr),
      _point_size(3.0f),
      _zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _dragging(false),
      _tooltips_enabled(true),
      _opengl_resources_initialized(false),
      _data_min_x(0.0f),
      _data_max_x(1.0f),
      _data_min_y(0.0f),
      _data_max_y(1.0f),
      _data_bounds_valid(false) {
    
    // Setup tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    connect(_tooltip_timer, &QTimer::timeout, this, &ScatterPlotOpenGLWidget::handleTooltipTimer);

    // Set OpenGL format
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for anti-aliasing
    setFormat(format);
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
        true  // defer_opengl_init = true
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

void ScatterPlotOpenGLWidget::setZoomLevel(float zoom_level) {
    _zoom_level = std::max(0.1f, std::min(10.0f, zoom_level));
    updateProjectionMatrix();
    emit zoomLevelChanged(_zoom_level);
    update();
}

void ScatterPlotOpenGLWidget::setPanOffset(float offset_x, float offset_y) {
    _pan_offset_x = offset_x;
    _pan_offset_y = offset_y;
    updateProjectionMatrix();
    emit panOffsetChanged(_pan_offset_x, _pan_offset_y);
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

    // Initialize view matrix
    updateViewMatrix();
    
    // Initialize projection matrix
    _projection_matrix.setToIdentity();

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

    // Set up matrices - use projection matrix directly since zoom/pan are handled in projection
    QMatrix4x4 mvp_matrix = _projection_matrix;
    
    qDebug() << "ScatterPlotOpenGLWidget::paintGL: Rendering with point size" << _point_size;
    
    // Render the scatter plot visualization
    _scatter_visualization->render(mvp_matrix, _point_size);
}

void ScatterPlotOpenGLWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);
    
    // Update projection matrix based on data bounds
    updateProjectionMatrix();
}

void ScatterPlotOpenGLWidget::updateProjectionMatrix() {
    _projection_matrix.setToIdentity();
    
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    if (left == right || bottom == top) {
        // Fall back to default projection if data bounds are invalid
        float aspect_ratio = static_cast<float>(width()) / height();
        if (aspect_ratio > 1.0f) {
            _projection_matrix.ortho(-aspect_ratio, aspect_ratio, -1.0f, 1.0f, -1.0f, 1.0f);
        } else {
            _projection_matrix.ortho(-1.0f, 1.0f, -1.0f / aspect_ratio, 1.0f / aspect_ratio, -1.0f, 1.0f);
        }
    } else {
        _projection_matrix.ortho(left, right, bottom, top, -1.0f, 1.0f);
    }
}

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _dragging = true;
        _last_mouse_pos = event->pos();
        
        // Check for point click
        if (_scatter_visualization) {
            QVector2D world_pos = screenToWorld(event->pos());
            // TODO: Implement point picking/selection
        }
    }
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    _current_mouse_pos = event->pos();
    
    if (_dragging && event->buttons() & Qt::LeftButton) {
        // Handle panning
        QPoint delta = event->pos() - _last_mouse_pos;
        
        // Convert screen delta to world delta
        float pan_sensitivity = 2.0f / (_zoom_level * width());
        _pan_offset_x -= delta.x() * pan_sensitivity;
        _pan_offset_y += delta.y() * pan_sensitivity; // Flip Y for OpenGL coordinates
        
        updateViewMatrix();
        emit panOffsetChanged(_pan_offset_x, _pan_offset_y);
        update();
        
        _last_mouse_pos = event->pos();
    } else {
        // Handle hover for tooltips
        handleMouseHover(event->pos());
    }
}

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _dragging = false;
    }
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    // Handle zooming
    float zoom_factor = 1.0f + (event->angleDelta().y() / 1200.0f);
    setZoomLevel(_zoom_level * zoom_factor);
}

void ScatterPlotOpenGLWidget::leaveEvent(QEvent * event) {
    Q_UNUSED(event)
    
    // Hide tooltip when mouse leaves widget
    _tooltip_timer->stop();
    QToolTip::hideText();
}

void ScatterPlotOpenGLWidget::handleTooltipTimer() {
    if (!_tooltips_enabled || !_scatter_visualization) {
        return;
    }

    // Convert mouse position to world coordinates and check for point under cursor
    QVector2D world_pos = screenToWorld(_tooltip_mouse_pos);
    
    // TODO: Implement proper point picking for tooltips
    // For now, show a basic tooltip
    QToolTip::showText(mapToGlobal(_tooltip_mouse_pos), 
                      QString("Scatter Plot\nZoom: %1x").arg(_zoom_level, 0, 'f', 2));
}

void ScatterPlotOpenGLWidget::updateViewMatrix() {
    _view_matrix.setToIdentity();
    _view_matrix.scale(_zoom_level, _zoom_level, 1.0f);
    _view_matrix.translate(_pan_offset_x, _pan_offset_y, 0.0f);
}

QVector2D ScatterPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    // Convert screen coordinates to normalized device coordinates
    float x = (2.0f * screen_pos.x()) / width() - 1.0f;
    float y = 1.0f - (2.0f * screen_pos.y()) / height();
    
    // Apply inverse transformation
    QMatrix4x4 inverse_matrix = (_projection_matrix * _view_matrix).inverted();
    QVector4D world_pos = inverse_matrix * QVector4D(x, y, 0.0f, 1.0f);
    
    return QVector2D(world_pos.x(), world_pos.y());
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

    // Calculate orthographic projection bounds (similar to SpatialOverlayOpenGLWidget)
    float data_width = _data_max_x - _data_min_x;
    float data_height = _data_max_y - _data_min_y;
    float center_x = (_data_min_x + _data_max_x) * 0.5f;
    float center_y = (_data_min_y + _data_max_y) * 0.5f;

    // Add padding and apply zoom
    float padding = 1.1f; // 10% padding
    float zoom_factor = 1.0f / _zoom_level;
    float half_width = (data_width * padding * zoom_factor) / 2.0f;
    float half_height = (data_height * padding * zoom_factor) / 2.0f;

    // Apply aspect ratio correction
    float aspect_ratio = static_cast<float>(width()) / height();
    if (aspect_ratio > 1.0f) {
        half_width *= aspect_ratio;
    } else {
        half_height /= aspect_ratio;
    }

    // Apply pan offset
    float pan_x = _pan_offset_x * data_width * zoom_factor;
    float pan_y = _pan_offset_y * data_height * zoom_factor;

    left = center_x - half_width + pan_x;
    right = center_x + half_width + pan_x;
    bottom = center_y - half_height + pan_y;
    top = center_y + half_height + pan_y;
}


