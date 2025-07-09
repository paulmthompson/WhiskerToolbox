#include "SpatialOverlayPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/TimeFrame.hpp"

#include <QApplication>
#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneResizeEvent>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QPen>
#include <QToolTip>
#include <QWheelEvent>

#include <GL/gl.h>
#include <cmath>

// SpatialOverlayOpenGLWidget implementation

SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _is_panning(false),
      _data_bounds_valid(false) {

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay for tooltip
    connect(_tooltip_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::handleTooltipTimer);

    // Initialize data bounds
    _data_min_x = _data_max_x = _data_min_y = _data_max_y = 0.0f;
}

void SpatialOverlayOpenGLWidget::setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map) {
    _all_points.clear();

    qDebug() << "SpatialOverlayOpenGLWidget: Setting point data with" << point_data_map.size() << "datasets";

    // Collect all points from all PointData objects
    for (auto const & [key, point_data]: point_data_map) {
        if (!point_data) {
            qDebug() << "SpatialOverlayOpenGLWidget: Null point data for key:" << key;
            continue;
        }

        qDebug() << "SpatialOverlayOpenGLWidget: Processing point data for key:" << key;
        size_t points_added = 0;

        // Iterate through all time indices in the point data
        for (auto const & time_points_pair: point_data->GetAllPointsAsRange()) {
            for (auto const & point: time_points_pair.points) {
                _all_points.emplace_back(
                        point.x, point.y,
                        time_points_pair.time.getValue(),
                        key);
                points_added++;
            }
        }

        qDebug() << "SpatialOverlayOpenGLWidget: Added" << points_added << "points for key:" << key;
    }

    qDebug() << "SpatialOverlayOpenGLWidget: Total points loaded:" << _all_points.size();

    calculateDataBounds();
    updateSpatialIndex();
    
    // Ensure view matrices are updated with current widget size
    updateViewMatrices();
    
    // Force immediate repaint
    update();
    repaint();
    
    qDebug() << "SpatialOverlayOpenGLWidget: setPointData completed, widget size:" << width() << "x" << height();
}

void SpatialOverlayOpenGLWidget::setZoomLevel(float zoom_level) {
    _zoom_level = std::max(0.1f, std::min(10.0f, zoom_level));
    updateViewMatrices();
    update();
}

void SpatialOverlayOpenGLWidget::setPanOffset(float offset_x, float offset_y) {
    _pan_offset_x = offset_x;
    _pan_offset_y = offset_y;
    updateViewMatrices();
    update();
}

void SpatialOverlayOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

    setupPointRendering();
    updateViewMatrices();
}

void SpatialOverlayOpenGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Debug widget and viewport information
    qDebug() << "SpatialOverlayOpenGLWidget: paintGL called";
    qDebug() << "SpatialOverlayOpenGLWidget: Widget size:" << width() << "x" << height();
    
    // Get viewport information
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    qDebug() << "SpatialOverlayOpenGLWidget: OpenGL viewport:" << viewport[0] << viewport[1] << viewport[2] << viewport[3];
    
    qDebug() << "SpatialOverlayOpenGLWidget: Points:" << _all_points.size() << "bounds valid:" << _data_bounds_valid;

    if (_all_points.empty() || !_data_bounds_valid) {
        qDebug() << "SpatialOverlayOpenGLWidget: Skipping rendering - empty points or invalid bounds";
        return;
    }

    // Test rendering - draw a simple triangle first to verify OpenGL is working
    qDebug() << "SpatialOverlayOpenGLWidget: Drawing test triangle";
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f); // Green triangle
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.0f, 0.5f);
    glEnd();
    
    qDebug() << "SpatialOverlayOpenGLWidget: Calling renderPoints()";
    renderPoints();
}

void SpatialOverlayOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateViewMatrices();
}

void SpatialOverlayOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _is_panning = true;
        _last_mouse_pos = event->pos();
        event->accept();
    } else {
        // Let other mouse buttons propagate to parent widget
        event->ignore();
        return;
    }
    _tooltip_timer->stop();
    QToolTip::hideText();
}

void SpatialOverlayOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    _current_mouse_pos = event->pos();

    if (_is_panning && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - _last_mouse_pos;

        // Convert pixel delta to world coordinates
        float world_scale = 2.0f / (_zoom_level * std::min(width(), height()));
        float dx = delta.x() * world_scale;
        float dy = -delta.y() * world_scale;// Flip Y axis

        setPanOffset(_pan_offset_x + dx, _pan_offset_y + dy);
        _last_mouse_pos = event->pos();
        event->accept();
    } else {
        // Stop panning if button was released
        _is_panning = false;
        
        // Start tooltip timer for hover
        _tooltip_timer->stop();
        _tooltip_timer->start();
        event->accept();
    }
}

void SpatialOverlayOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        event->accept();
    } else {
        event->ignore();
    }
}

void SpatialOverlayOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        SpatialPointData const * point = findPointNear(event->pos().x(), event->pos().y());
        if (point) {
            emit frameJumpRequested(point->time_frame_index);
        }
    }
}

void SpatialOverlayOpenGLWidget::wheelEvent(QWheelEvent * event) {
    float zoom_factor = 1.0f + (event->angleDelta().y() / 1200.0f);
    setZoomLevel(_zoom_level * zoom_factor);

    event->accept();
}

void SpatialOverlayOpenGLWidget::handleTooltipTimer() {
    if (!_data_bounds_valid) return;

    SpatialPointData const * point = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
    if (point) {
        QString tooltip_text = QString("Frame: %1\nData: %2\nPosition: (%3, %4)")
                                       .arg(point->time_frame_index)
                                       .arg(point->point_data_key)
                                       .arg(point->x, 0, 'f', 2)
                                       .arg(point->y, 0, 'f', 2);

        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
    }
}

void SpatialOverlayOpenGLWidget::updateSpatialIndex() {
    if (_all_points.empty() || !_data_bounds_valid) {
        _spatial_index.reset();
        return;
    }

    // Create spatial index with data bounds
    BoundingBox bounds(_data_min_x, _data_min_y, _data_max_x, _data_max_y);
    _spatial_index = std::make_unique<QuadTree<size_t>>(bounds);

    // Insert all points with their index as data
    for (size_t i = 0; i < _all_points.size(); ++i) {
        auto const & point = _all_points[i];
        _spatial_index->insert(point.x, point.y, i);
    }
}

void SpatialOverlayOpenGLWidget::calculateDataBounds() {
    if (_all_points.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: calculateDataBounds - no points, setting invalid bounds";
        _data_bounds_valid = false;
        return;
    }

    _data_min_x = _data_max_x = _all_points[0].x;
    _data_min_y = _data_max_y = _all_points[0].y;

    for (auto const & point: _all_points) {
        _data_min_x = std::min(_data_min_x, point.x);
        _data_max_x = std::max(_data_max_x, point.x);
        _data_min_y = std::min(_data_min_y, point.y);
        _data_max_y = std::max(_data_max_y, point.y);
    }

    // Add some padding
    float padding_x = (_data_max_x - _data_min_x) * 0.1f;
    float padding_y = (_data_max_y - _data_min_y) * 0.1f;

    _data_min_x -= padding_x;
    _data_max_x += padding_x;
    _data_min_y -= padding_y;
    _data_max_y += padding_y;

    _data_bounds_valid = true;
    
    qDebug() << "SpatialOverlayOpenGLWidget: Data bounds calculated:"
             << "X:" << _data_min_x << "to" << _data_max_x
             << "Y:" << _data_min_y << "to" << _data_max_y;
}

QVector2D SpatialOverlayOpenGLWidget::screenToWorld(int screen_x, int screen_y) const {
    if (!_data_bounds_valid) return QVector2D(0, 0);

    // Convert screen coordinates to normalized device coordinates (-1 to 1)
    float ndc_x = (2.0f * screen_x / width()) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / height());// Flip Y axis

    // Apply inverse transformations
    float world_scale = 1.0f / _zoom_level;
    float world_x = (ndc_x * world_scale) - _pan_offset_x;
    float world_y = (ndc_y * world_scale) - _pan_offset_y;

    // Scale to data coordinates
    float data_width = _data_max_x - _data_min_x;
    float data_height = _data_max_y - _data_min_y;
    float aspect_ratio = static_cast<float>(width()) / height();

    if (aspect_ratio > 1.0f) {
        world_x *= data_width * aspect_ratio * 0.5f;
        world_y *= data_height * 0.5f;
    } else {
        world_x *= data_width * 0.5f;
        world_y *= data_height / aspect_ratio * 0.5f;
    }

    world_x += (_data_min_x + _data_max_x) * 0.5f;
    world_y += (_data_min_y + _data_max_y) * 0.5f;

    return QVector2D(world_x, world_y);
}

QPoint SpatialOverlayOpenGLWidget::worldToScreen(float world_x, float world_y) const {
    if (!_data_bounds_valid) return QPoint(0, 0);

    // Transform world coordinates to screen
    float data_width = _data_max_x - _data_min_x;
    float data_height = _data_max_y - _data_min_y;
    float aspect_ratio = static_cast<float>(width()) / height();

    // Center and normalize
    float norm_x = (world_x - (_data_min_x + _data_max_x) * 0.5f);
    float norm_y = (world_y - (_data_min_y + _data_max_y) * 0.5f);

    if (aspect_ratio > 1.0f) {
        norm_x /= data_width * aspect_ratio * 0.5f;
        norm_y /= data_height * 0.5f;
    } else {
        norm_x /= data_width * 0.5f;
        norm_y /= data_height / aspect_ratio * 0.5f;
    }

    // Apply transformations
    norm_x = (norm_x + _pan_offset_x) * _zoom_level;
    norm_y = (norm_y + _pan_offset_y) * _zoom_level;

    // Convert to screen coordinates
    int screen_x = static_cast<int>((norm_x + 1.0f) * width() * 0.5f);
    int screen_y = static_cast<int>((1.0f - norm_y) * height() * 0.5f);

    return QPoint(screen_x, screen_y);
}

SpatialPointData const * SpatialOverlayOpenGLWidget::findPointNear(int screen_x, int screen_y, float tolerance_pixels) const {
    if (!_spatial_index || !_data_bounds_valid) {
        return nullptr;
    }

    // Convert screen tolerance to world tolerance
    QVector2D world_pos = screenToWorld(screen_x, screen_y);
    QVector2D world_pos_offset = screenToWorld(screen_x + static_cast<int>(tolerance_pixels), screen_y);
    float world_tolerance = std::abs(world_pos_offset.x() - world_pos.x());

    // Find nearest point using spatial index
    auto const * nearest_point_index = _spatial_index->findNearest(
            world_pos.x(), world_pos.y(), world_tolerance);

    if (nearest_point_index) {
        // Add bounds checking to prevent crash
        size_t index = nearest_point_index->data;
        if (index < _all_points.size()) {
            return &_all_points[index];
        } else {
            qDebug() << "SpatialOverlayOpenGLWidget: findPointNear - invalid index" << index << "size:" << _all_points.size();
        }
    }

    return nullptr;
}

void SpatialOverlayOpenGLWidget::updateViewMatrices() {
    // Update projection matrix for current aspect ratio
    _projection_matrix.setToIdentity();
    float aspect_ratio = static_cast<float>(width()) / height();
    if (aspect_ratio > 1.0f) {
        _projection_matrix.ortho(-aspect_ratio, aspect_ratio, -1.0f, 1.0f, -1.0f, 1.0f);
    } else {
        _projection_matrix.ortho(-1.0f, 1.0f, -1.0f / aspect_ratio, 1.0f / aspect_ratio, -1.0f, 1.0f);
    }

    // Update view matrix with zoom and pan
    _view_matrix.setToIdentity();
    _view_matrix.scale(_zoom_level);
    _view_matrix.translate(_pan_offset_x, _pan_offset_y);
}

void SpatialOverlayOpenGLWidget::renderPoints() {
    if (!_data_bounds_valid || _all_points.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: renderPoints - skipping, bounds valid:" << _data_bounds_valid << "points:" << _all_points.size();
        return;
    }

    qDebug() << "SpatialOverlayOpenGLWidget: renderPoints - starting with" << _all_points.size() << "points";
    qDebug() << "SpatialOverlayOpenGLWidget: Data bounds: X[" << _data_min_x << "," << _data_max_x << "] Y[" << _data_min_y << "," << _data_max_y << "]";
    qDebug() << "SpatialOverlayOpenGLWidget: Zoom level:" << _zoom_level << "Pan offset:" << _pan_offset_x << "," << _pan_offset_y;

    // Check for OpenGL errors before rendering
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "SpatialOverlayOpenGLWidget: OpenGL error before rendering:" << error;
    }

    glPointSize(8.0f); // Increase point size to make sure they're visible
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // Bright red, fully opaque

    // Set up simple orthographic projection directly with manual OpenGL commands
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Simple orthographic projection that maps data bounds to [-1,1]
    float data_width = _data_max_x - _data_min_x;
    float data_height = _data_max_y - _data_min_y;
    float center_x = (_data_min_x + _data_max_x) * 0.5f;
    float center_y = (_data_min_y + _data_max_y) * 0.5f;

    qDebug() << "SpatialOverlayOpenGLWidget: Data center:" << center_x << "," << center_y;
    qDebug() << "SpatialOverlayOpenGLWidget: Data size:" << data_width << "x" << data_height;

    // Set orthographic projection to show the data bounds with some padding
    float padding = 1.1f; // 10% padding
    float left = center_x - (data_width * padding) / 2.0f;
    float right = center_x + (data_width * padding) / 2.0f;
    float bottom = center_y - (data_height * padding) / 2.0f;
    float top = center_y + (data_height * padding) / 2.0f;
    
    qDebug() << "SpatialOverlayOpenGLWidget: Projection bounds: left" << left << "right" << right << "bottom" << bottom << "top" << top;
    
    glOrtho(left, right, bottom, top, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Apply zoom and pan
    glScalef(_zoom_level, _zoom_level, 1.0f);
    glTranslatef(_pan_offset_x, _pan_offset_y, 0.0f);

    // Check for OpenGL errors after matrix setup
    error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "SpatialOverlayOpenGLWidget: OpenGL error after matrix setup:" << error;
    }

    qDebug() << "SpatialOverlayOpenGLWidget: About to render" << _all_points.size() << "points";
    
    glBegin(GL_POINTS);
    int points_rendered = 0;
    for (auto const & point: _all_points) {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // Keep it simple - bright red
        glVertex2f(point.x, point.y);
        points_rendered++;
        
        // Debug first few points
        if (points_rendered <= 5) {
            qDebug() << "SpatialOverlayOpenGLWidget: Point" << points_rendered << "at" << point.x << "," << point.y;
        }
    }
    glEnd();

    qDebug() << "SpatialOverlayOpenGLWidget: Rendered" << points_rendered << "points";

    // Check for OpenGL errors after rendering
    error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "SpatialOverlayOpenGLWidget: OpenGL error after rendering:" << error;
    }
    
    // Force a flush to make sure OpenGL commands are executed
    glFlush();
    
    qDebug() << "SpatialOverlayOpenGLWidget: renderPoints completed";
}

void SpatialOverlayOpenGLWidget::setupPointRendering() {
    // Basic OpenGL setup for point rendering
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

// SpatialOverlayPlotWidget implementation

SpatialOverlayPlotWidget::SpatialOverlayPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent),
      _opengl_widget(nullptr),
      _proxy_widget(nullptr) {

    qDebug() << "SpatialOverlayPlotWidget: Constructor called";
    setPlotTitle("Spatial Overlay Plot");
    setupOpenGLWidget();
    qDebug() << "SpatialOverlayPlotWidget: Constructor completed, OpenGL widget:" << (_opengl_widget != nullptr);
}

QString SpatialOverlayPlotWidget::getPlotType() const {
    return "Spatial Overlay Plot";
}

void SpatialOverlayPlotWidget::setPointDataKeys(QStringList const & point_data_keys) {
    qDebug() << "SpatialOverlayPlotWidget: setPointDataKeys called with:" << point_data_keys;
    _point_data_keys = point_data_keys;
    updateVisualization();
}

void SpatialOverlayPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // Draw frame around the plot
    QRectF rect = boundingRect();

    QPen border_pen;
    if (isSelected()) {
        border_pen.setColor(QColor(0, 120, 200));
        border_pen.setWidth(2);
    } else {
        border_pen.setColor(QColor(100, 100, 100));
        border_pen.setWidth(1);
    }
    painter->setPen(border_pen);
    painter->drawRect(rect);

    // Draw title
    painter->setPen(QColor(0, 0, 0));
    QFont title_font = painter->font();
    title_font.setBold(true);
    painter->setFont(title_font);

    QRectF title_rect = rect.adjusted(5, 5, -5, -rect.height() + 20);
    painter->drawText(title_rect, Qt::AlignCenter, getPlotTitle());
}

void SpatialOverlayPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    // Check if the click is in the title area (top 25 pixels)
    QRectF title_area = boundingRect().adjusted(0, 0, 0, -boundingRect().height() + 25);
    
    if (title_area.contains(event->pos())) {
        // Click in title area - handle selection and allow movement
        emit plotSelected(getPlotId());
        // Make sure the item is movable for dragging
        setFlag(QGraphicsItem::ItemIsMovable, true);
        AbstractPlotWidget::mousePressEvent(event);
    } else {
        // Click in content area - let the OpenGL widget handle it
        // But still emit selection signal
        emit plotSelected(getPlotId());
        // Disable movement when clicking in content area
        setFlag(QGraphicsItem::ItemIsMovable, false);
        // Don't call parent implementation to avoid interfering with OpenGL panning
        event->accept();
    }
}

void SpatialOverlayPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);

    if (_opengl_widget && _proxy_widget) {
        QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
        qDebug() << "SpatialOverlayPlotWidget: Resizing OpenGL widget to:" << content_rect.size();
        _opengl_widget->resize(content_rect.size().toSize());
        _proxy_widget->setGeometry(content_rect);
        
        // Force update after resize
        _opengl_widget->update();
    }
}

void SpatialOverlayPlotWidget::updateVisualization() {
    qDebug() << "SpatialOverlayPlotWidget: updateVisualization called";
    qDebug() << "SpatialOverlayPlotWidget: DataManager available:" << (_data_manager != nullptr);
    qDebug() << "SpatialOverlayPlotWidget: OpenGL widget available:" << (_opengl_widget != nullptr);
    
    if (!_data_manager || !_opengl_widget) {
        qDebug() << "SpatialOverlayPlotWidget: Missing DataManager or OpenGL widget, aborting";
        return;
    }

    loadPointData();
}

void SpatialOverlayPlotWidget::handleFrameJumpRequest(int64_t time_frame_index) {
    emit frameJumpRequested(time_frame_index);
}

void SpatialOverlayPlotWidget::loadPointData() {
    std::unordered_map<QString, std::shared_ptr<PointData>> point_data_map;

    qDebug() << "SpatialOverlayPlotWidget: loadPointData called with keys:" << _point_data_keys;
    qDebug() << "SpatialOverlayPlotWidget: DataManager available:" << (_data_manager != nullptr);

    for (QString const & key: _point_data_keys) {
        auto point_data = _data_manager->getData<PointData>(key.toStdString());
        if (point_data) {
            qDebug() << "SpatialOverlayPlotWidget: Found point data for key:" << key;
            point_data_map[key] = point_data;
        } else {
            qDebug() << "SpatialOverlayPlotWidget: No point data found for key:" << key;
        }
    }

    qDebug() << "SpatialOverlayPlotWidget: Passing" << point_data_map.size() << "datasets to OpenGL widget";
    _opengl_widget->setPointData(point_data_map);
}

void SpatialOverlayPlotWidget::setupOpenGLWidget() {
    _opengl_widget = new SpatialOverlayOpenGLWidget();
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);
    
    // Configure the proxy widget to not interfere with parent interactions
    _proxy_widget->setFlag(QGraphicsItem::ItemIsMovable, false);
    _proxy_widget->setFlag(QGraphicsItem::ItemIsSelectable, false);

    // Set initial size and position
    QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
    _opengl_widget->resize(content_rect.size().toSize());
    _proxy_widget->setGeometry(content_rect);

    // Connect signals
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::frameJumpRequested,
            this, &SpatialOverlayPlotWidget::handleFrameJumpRequest);
}