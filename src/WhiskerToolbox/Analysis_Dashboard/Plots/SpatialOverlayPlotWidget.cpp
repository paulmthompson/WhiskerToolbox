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

#include <cmath>

// SpatialOverlayOpenGLWidget implementation

SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _shader_program(nullptr),
      _vertex_buffer(QOpenGLBuffer::VertexBuffer),
      _highlight_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      _opengl_resources_initialized(false),
      _zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _point_size(8.0f),
      _is_panning(false),
      _data_bounds_valid(false),
      _tooltips_enabled(true),
      _pending_update(false),
      _current_hover_point(nullptr) {

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for smooth points
    setFormat(format);

    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay for tooltip
    connect(_tooltip_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::handleTooltipTimer);

    _tooltip_refresh_timer = new QTimer(this);
    _tooltip_refresh_timer->setInterval(100); // Refresh every 100ms to keep tooltip visible
    connect(_tooltip_refresh_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::handleTooltipRefresh);

    // FPS limiter timer (30 FPS = ~33ms interval)
    _fps_limiter_timer = new QTimer(this);
    _fps_limiter_timer->setSingleShot(true);
    _fps_limiter_timer->setInterval(33); // ~30 FPS
    connect(_fps_limiter_timer, &QTimer::timeout, this, [this]() {
        if (_pending_update) {
            _pending_update = false;
            update();
            emit highlightStateChanged();
        }
    });

    // Initialize data bounds
    _data_min_x = _data_max_x = _data_min_y = _data_max_y = 0.0f;
}

SpatialOverlayOpenGLWidget::~SpatialOverlayOpenGLWidget() {
    cleanupOpenGLResources();
}

void SpatialOverlayOpenGLWidget::setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map) {
    _all_points.clear();

    // Collect all points from all PointData objects
    for (auto const & [key, point_data]: point_data_map) {
        if (!point_data) {
            qDebug() << "SpatialOverlayOpenGLWidget: Null point data for key:" << key;
            continue;
        }

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
    }

    calculateDataBounds();
    updateSpatialIndex();
    
    // Update vertex buffer with new data
    if (_opengl_resources_initialized) {
        updateVertexBuffer();
    }
    
    // Ensure view matrices are updated with current widget size
    updateViewMatrices();
    
    // Ensure OpenGL context is current before forcing repaint
    if (context() && context()->isValid()) {
        makeCurrent();
        
        // Force immediate repaint
        update();
        repaint();
        
        // Process any pending events to ensure immediate rendering
        QApplication::processEvents();
        
        doneCurrent();
    } else {
        update();
    }
}

void SpatialOverlayOpenGLWidget::setZoomLevel(float zoom_level) {
    float new_zoom_level = std::max(0.1f, std::min(10.0f, zoom_level));
    if (new_zoom_level != _zoom_level) {
        _zoom_level = new_zoom_level;
        emit zoomLevelChanged(_zoom_level);
        updateViewMatrices();
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::setPanOffset(float offset_x, float offset_y) {
    if (offset_x != _pan_offset_x || offset_y != _pan_offset_y) {
        _pan_offset_x = offset_x;
        _pan_offset_y = offset_y;
        emit panOffsetChanged(_pan_offset_x, _pan_offset_y);
        updateViewMatrices();
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::setPointSize(float point_size) {
    float new_point_size = std::max(1.0f, std::min(50.0f, point_size)); // Clamp between 1 and 50 pixels
    if (new_point_size != _point_size) {
        _point_size = new_point_size;
        emit pointSizeChanged(_point_size);
        
        // Use throttled update for better performance
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::setTooltipsEnabled(bool enabled) {
    if (enabled != _tooltips_enabled) {
        _tooltips_enabled = enabled;
        emit tooltipsEnabledChanged(_tooltips_enabled);
        
        // Hide any currently visible tooltip when disabling
        if (!_tooltips_enabled) {
            _tooltip_timer->stop();
            _tooltip_refresh_timer->stop();
            QToolTip::hideText();
            _current_hover_point = nullptr;
        }
    }
}

void SpatialOverlayOpenGLWidget::initializeGL() {
    if (!initializeOpenGLFunctions()) {
        return;
    }

    // Set clear color
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    
    // Enable multisampling if available
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Enable programmable point size
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // Check OpenGL capabilities
    GLint max_point_size;
    glGetIntegerv(GL_POINT_SIZE_RANGE, &max_point_size);

    // Initialize OpenGL resources
    initializeOpenGLResources();
    updateViewMatrices();
}

void SpatialOverlayOpenGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (_all_points.empty() || !_data_bounds_valid || !_opengl_resources_initialized) {
        return;
    }

    renderPoints();
}

void SpatialOverlayOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateViewMatrices();
    
    // No need to update vertex buffer during resize - the data is already on GPU
    // Just update the viewport and projection matrix
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
    _tooltip_refresh_timer->stop();
    QToolTip::hideText();
    _current_hover_point = nullptr;
    
    // Trigger repaint to clear highlight
    update();
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
        
        // Handle tooltip logic if tooltips are enabled
        if (_tooltips_enabled) {
            SpatialPointData const * point = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
            
            if (point != _current_hover_point) {
                // We're hovering over a different point (or no point)
                if (point) {
                    qDebug() << "SpatialOverlayOpenGLWidget: Hovering over new point at" 
                             << point->x << "," << point->y << "frame:" << point->time_frame_index;
                } else {
                    qDebug() << "SpatialOverlayOpenGLWidget: No longer hovering over a point";
                }
                
                _current_hover_point = point;
                _tooltip_timer->stop();
                _tooltip_refresh_timer->stop();
                
                // Use throttled update to prevent excessive redraws
                requestThrottledUpdate();
                
                if (point) {
                    // Start timer for new point
                    _tooltip_timer->start();
                } else {
                    // No point under cursor, hide tooltip
                    QToolTip::hideText();
                }
            } else if (point) {
                // Still hovering over the same point, update tooltip position
                QString tooltip_text = QString("Frame: %1\nData: %2\nPosition: (%3, %4)")
                                               .arg(point->time_frame_index)
                                               .arg(point->point_data_key)
                                               .arg(point->x, 0, 'f', 2)
                                               .arg(point->y, 0, 'f', 2);
                QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
            }
        }
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
        qDebug() << "SpatialOverlayOpenGLWidget: Double-click detected at" << event->pos();
        
        SpatialPointData const * point = findPointNear(event->pos().x(), event->pos().y());
        if (point) {
            qDebug() << "SpatialOverlayOpenGLWidget: Double-click on point at (" 
                     << point->x << "," << point->y << ") frame:" << point->time_frame_index 
                     << "data:" << point->point_data_key;
            
            emit frameJumpRequested(point->time_frame_index);
            event->accept();
        } else {
            qDebug() << "SpatialOverlayOpenGLWidget: Double-click but no point found near cursor";
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void SpatialOverlayOpenGLWidget::wheelEvent(QWheelEvent * event) {
    float zoom_factor = 1.0f + (event->angleDelta().y() / 1200.0f);
    setZoomLevel(_zoom_level * zoom_factor);

    event->accept();
}

void SpatialOverlayOpenGLWidget::leaveEvent(QEvent * event) {
    // Hide tooltips when mouse leaves the widget
    _tooltip_timer->stop();
    _tooltip_refresh_timer->stop();
    QToolTip::hideText();
    _current_hover_point = nullptr;
    
    // Use throttled update
    requestThrottledUpdate();
    
    QOpenGLWidget::leaveEvent(event);
}

void SpatialOverlayOpenGLWidget::handleTooltipTimer() {
    if (!_data_bounds_valid || !_tooltips_enabled) return;

    SpatialPointData const * point = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
    if (point && point == _current_hover_point) {
        QString tooltip_text = QString("Frame: %1\nData: %2\nPosition: (%3, %4)")
                                       .arg(point->time_frame_index)
                                       .arg(point->point_data_key)
                                       .arg(point->x, 0, 'f', 2)
                                       .arg(point->y, 0, 'f', 2);

        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
        
        // Start refresh timer to keep tooltip visible
        _tooltip_refresh_timer->start();
    }
}

void SpatialOverlayOpenGLWidget::requestThrottledUpdate() {
    if (!_fps_limiter_timer->isActive()) {
        // If timer is not running, update immediately and start timer
        update();
        emit highlightStateChanged();
        _fps_limiter_timer->start();
    } else {
        // Timer is running, just mark that we have a pending update
        _pending_update = true;
    }
}

void SpatialOverlayOpenGLWidget::handleTooltipRefresh() {
    if (!_data_bounds_valid || !_tooltips_enabled || !_current_hover_point) {
        _tooltip_refresh_timer->stop();
        return;
    }

    // Check if we're still hovering over the same point
    SpatialPointData const * point = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
    if (point == _current_hover_point) {
        // Refresh the tooltip to keep it visible
        QString tooltip_text = QString("Frame: %1\nData: %2\nPosition: (%3, %4)")
                                       .arg(point->time_frame_index)
                                       .arg(point->point_data_key)
                                       .arg(point->x, 0, 'f', 2)
                                       .arg(point->y, 0, 'f', 2);

        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
    } else {
        // No longer hovering over the same point, stop refresh timer
        _tooltip_refresh_timer->stop();
        QToolTip::hideText();
        _current_hover_point = nullptr;
    }
}

void SpatialOverlayOpenGLWidget::updateSpatialIndex() {
    qDebug() << "SpatialOverlayOpenGLWidget: updateSpatialIndex called with" << _all_points.size() << "points";
    
    if (_all_points.empty() || !_data_bounds_valid) {
        _spatial_index.reset();
        qDebug() << "SpatialOverlayOpenGLWidget: Spatial index reset (no points or invalid bounds)";
        return;
    }

    // Temporarily disable mouse tracking during spatial index rebuild
    bool was_tracking = hasMouseTracking();
    setMouseTracking(false);

    // Create spatial index with data bounds
    BoundingBox bounds(_data_min_x, _data_min_y, _data_max_x, _data_max_y);
    _spatial_index = std::make_unique<QuadTree<size_t>>(bounds);

    // Insert all points with their index as data
    for (size_t i = 0; i < _all_points.size(); ++i) {
        auto const & point = _all_points[i];
        _spatial_index->insert(point.x, point.y, i);
    }
    
    // Re-enable mouse tracking
    setMouseTracking(was_tracking);
}

void SpatialOverlayOpenGLWidget::calculateDataBounds() {
    if (_all_points.empty()) {
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
}

QVector2D SpatialOverlayOpenGLWidget::screenToWorld(int screen_x, int screen_y) const {
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    if (left == right || bottom == top) {
        return QVector2D(0, 0);
    }
    
    // Convert screen coordinates to world coordinates using the projection bounds
    float world_x = left + (static_cast<float>(screen_x) / width()) * (right - left);
    float world_y = top - (static_cast<float>(screen_y) / height()) * (top - bottom); // Y is flipped in screen coordinates
    
    return QVector2D(world_x, world_y);
}

QPoint SpatialOverlayOpenGLWidget::worldToScreen(float world_x, float world_y) const {
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    if (left == right || bottom == top) {
        return QPoint(0, 0);
    }
    
    // Convert world coordinates to screen coordinates using the projection bounds
    int screen_x = static_cast<int>(((world_x - left) / (right - left)) * width());
    int screen_y = static_cast<int>(((top - world_y) / (top - bottom)) * height()); // Y is flipped in screen coordinates
    
    return QPoint(screen_x, screen_y);
}

SpatialPointData const * SpatialOverlayOpenGLWidget::findPointNear(int screen_x, int screen_y, float tolerance_pixels) const {
    // Add extra safety checks to prevent crashes during resizing
    if (!_spatial_index || !_data_bounds_valid || _all_points.empty()) {
        return nullptr;
    }

    // Create a local copy of the points size to avoid race conditions
    size_t points_size = _all_points.size();
    if (points_size == 0) {
        return nullptr;
    }

    // Convert screen tolerance to world tolerance more accurately
    QVector2D world_pos = screenToWorld(screen_x, screen_y);
    QVector2D world_pos_x_offset = screenToWorld(screen_x + static_cast<int>(tolerance_pixels), screen_y);
    QVector2D world_pos_y_offset = screenToWorld(screen_x, screen_y + static_cast<int>(tolerance_pixels));
    
    // Use the maximum of X and Y tolerance for circular tolerance
    float world_tolerance_x = std::abs(world_pos_x_offset.x() - world_pos.x());
    float world_tolerance_y = std::abs(world_pos_y_offset.y() - world_pos.y());
    float world_tolerance = std::max(world_tolerance_x, world_tolerance_y);
    
    // Find nearest point using spatial index with extra error checking
    try {
        auto const * nearest_point_index = _spatial_index->findNearest(
                world_pos.x(), world_pos.y(), world_tolerance);

        if (nearest_point_index) {
            // Add bounds checking to prevent crash
            size_t index = nearest_point_index->data;
            if (index < points_size && index < _all_points.size()) {
                SpatialPointData const * found_point = &_all_points[index];
                
                // Verify the point is actually within our tolerance by converting back to screen
                QPoint point_screen = worldToScreen(found_point->x, found_point->y);
                float screen_distance = std::sqrt(std::pow(point_screen.x() - screen_x, 2) + 
                                                 std::pow(point_screen.y() - screen_y, 2));
                
                if (screen_distance <= tolerance_pixels) {
                    return found_point;
                } else {
                    qDebug() << "SpatialOverlayOpenGLWidget: Point outside screen tolerance, rejecting";
                }
            } else {
                qDebug() << "SpatialOverlayOpenGLWidget: findPointNear - invalid index" << index 
                         << "points_size:" << points_size << "_all_points.size():" << _all_points.size();
                // Rebuild spatial index if corrupted
                const_cast<SpatialOverlayOpenGLWidget*>(this)->updateSpatialIndex();
            }
        }
    } catch (...) {
        qDebug() << "SpatialOverlayOpenGLWidget: findPointNear - exception caught, rebuilding spatial index";
        const_cast<SpatialOverlayOpenGLWidget*>(this)->updateSpatialIndex();
    }

    return nullptr;
}

void SpatialOverlayOpenGLWidget::updateViewMatrices() {
    // Update projection matrix for current aspect ratio
    _projection_matrix.setToIdentity();
    
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);
    
    if (left == right || bottom == top) {
        qDebug() << "SpatialOverlayOpenGLWidget: updateViewMatrices - invalid projection bounds";
        return;
    }
    
    _projection_matrix.ortho(left, right, bottom, top, -1.0f, 1.0f);

    // Update view matrix (identity for now, transformations handled in projection)
    _view_matrix.setToIdentity();
    
    // Update model matrix (identity for now)
    _model_matrix.setToIdentity();
}

void SpatialOverlayOpenGLWidget::renderPoints() {
    if (!_data_bounds_valid || _all_points.empty() || !_opengl_resources_initialized) {
        return;
    }

    // Use shader program
    if (!_shader_program->bind()) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to bind shader program";
        return;
    }

    // Set uniform matrices
    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
    _shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    
    // === DRAW CALL 1: Render all regular points ===
    _vertex_array_object.bind();
    _vertex_buffer.bind();
    
    // Verify vertex buffer has data
    GLint buffer_size = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);
    if (buffer_size == 0) {
        qDebug() << "SpatialOverlayOpenGLWidget: Vertex buffer is empty, updating";
        updateVertexBuffer();
        _vertex_buffer.bind(); // Re-bind after update
    }
    
    // Set vertex attributes for regular points
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Enable blending for regular points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set uniforms for regular points
    _shader_program->setUniformValue("u_color", QVector4D(1.0f, 0.0f, 0.0f, 1.0f)); // Solid red
    _shader_program->setUniformValue("u_point_size", _point_size);
    
    // Draw all regular points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(_all_points.size()));
    
    // Unbind regular point resources
    _vertex_buffer.release();
    _vertex_array_object.release();

    // === DRAW CALL 2: Render highlighted point ===
    if (_current_hover_point) {
        renderHighlightedPoint();
    }

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug() << "SpatialOverlayOpenGLWidget: OpenGL error during rendering:" << error;
    }

    _shader_program->release();
}

void SpatialOverlayOpenGLWidget::renderHighlightedPoint() {
    if (!_current_hover_point) return;
    
    // Find the index of the current hover point in the main vertex buffer
    size_t hover_point_index = 0;
    bool found = false;
    for (size_t i = 0; i < _all_points.size(); ++i) {
        if (&_all_points[i] == _current_hover_point) {
            hover_point_index = i;
            found = true;
            break;
        }
    }
    
    if (!found) {
        qDebug() << "SpatialOverlayOpenGLWidget: Hover point not found in main point list";
        return;
    }
    
    // Bind highlight VAO and VBO
    _highlight_vertex_array_object.bind();
    _highlight_vertex_buffer.bind();
    
    // Use glBufferSubData to copy the highlighted point data from main VBO
    // First, ensure the highlight buffer is allocated (2 floats for x,y)
    GLint highlight_buffer_size = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &highlight_buffer_size);
    
    if (highlight_buffer_size < static_cast<GLint>(2 * sizeof(float))) {
        // Allocate buffer if not already allocated or too small
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    }
    
    // Copy data from main vertex buffer at the specific point index
    // Calculate offset in the main buffer (each point is 2 floats)
    GLintptr source_offset = static_cast<GLintptr>(hover_point_index * 2 * sizeof(float));
    
    // Map the main vertex buffer to read the data
    _vertex_buffer.bind();
    float* main_buffer_data = static_cast<float*>(glMapBufferRange(
        GL_ARRAY_BUFFER, 
        source_offset, 
        2 * sizeof(float), 
        GL_MAP_READ_BIT
    ));
    
    if (main_buffer_data) {
        // Switch back to highlight buffer and update it
        _highlight_vertex_buffer.bind();
        glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), main_buffer_data);
        
        // Unmap the main buffer
        _vertex_buffer.bind();
        glUnmapBuffer(GL_ARRAY_BUFFER);
        _highlight_vertex_buffer.bind();
    } else {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to map main vertex buffer";
        // Fallback: directly set the highlight point data
        float highlight_data[2] = {_current_hover_point->x, _current_hover_point->y};
        glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), highlight_data);
    }
    
    // Set vertex attributes for highlight point
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    
    // Disable blending for highlight point (solid color, no transparency)
    glDisable(GL_BLEND);
    
    // Set uniforms for highlight rendering
    _shader_program->setUniformValue("u_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f)); // Black
    _shader_program->setUniformValue("u_point_size", _point_size * 2.5f); // Larger size
    
    // Draw the highlighted point
    glDrawArrays(GL_POINTS, 0, 1);
    
    // Re-enable blending for subsequent rendering
    glEnable(GL_BLEND);
    
    // Unbind highlight resources
    _highlight_vertex_buffer.release();
    _highlight_vertex_array_object.release();
    
 }

void SpatialOverlayOpenGLWidget::initializeOpenGLResources() {
    qDebug() << "SpatialOverlayOpenGLWidget: Initializing OpenGL resources";

    // Create and compile shader program
    _shader_program = new QOpenGLShaderProgram(this);
    
    // Vertex shader
    QString vertex_shader_source = R"(
        #version 410 core
        
        layout(location = 0) in vec2 a_position;
        
        uniform mat4 u_mvp_matrix;
        uniform float u_point_size;
        
        void main() {
            gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
            gl_PointSize = u_point_size;
        }
    )";
    
    // Fragment shader
    QString fragment_shader_source = R"(
        #version 410 core
        
        uniform vec4 u_color;
        
        out vec4 FragColor;
        
        void main() {
            // Create circular points
            vec2 coord = gl_PointCoord - vec2(0.5, 0.5);
            float distance = length(coord);
            
            // Discard fragments outside the circle
            if (distance > 0.5) {
                discard;
            }
            
            // Smooth anti-aliased edge
            float alpha = 1.0 - smoothstep(0.4, 0.5, distance);
            FragColor = vec4(u_color.rgb, u_color.a * alpha);
        }
    )";
    
    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_source)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to compile vertex shader:" << _shader_program->log();
        return;
    }
    
    if (!_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader_source)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to compile fragment shader:" << _shader_program->log();
        return;
    }
    
    if (!_shader_program->link()) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to link shader program:" << _shader_program->log();
        return;
    }

    // Create vertex array object
    _vertex_array_object.create();
    _vertex_array_object.bind();

    // Create vertex buffer
    _vertex_buffer.create();
    _vertex_buffer.bind();
    _vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _vertex_array_object.release();
    _vertex_buffer.release();

    // Create highlight vertex array object and buffer
    _highlight_vertex_array_object.create();
    _highlight_vertex_array_object.bind();

    _highlight_vertex_buffer.create();
    _highlight_vertex_buffer.bind();
    _highlight_vertex_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    // Pre-allocate highlight buffer for one point (2 floats)
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    
    // Set up vertex attributes for highlight
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    _highlight_vertex_array_object.release();
    _highlight_vertex_buffer.release();

    _opengl_resources_initialized = true;
}

void SpatialOverlayOpenGLWidget::cleanupOpenGLResources() {
    if (_opengl_resources_initialized) {
        makeCurrent();
        
        delete _shader_program;
        _shader_program = nullptr;
        
        _vertex_buffer.destroy();
        _vertex_array_object.destroy();
        
        _highlight_vertex_buffer.destroy();
        _highlight_vertex_array_object.destroy();
        
        _opengl_resources_initialized = false;
        
        doneCurrent();
    }
}

void SpatialOverlayOpenGLWidget::updateVertexBuffer() {
    if (!_opengl_resources_initialized || _all_points.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: updateVertexBuffer - skipping, resources initialized:" 
                 << _opengl_resources_initialized << "points:" << _all_points.size();
        return;
    }

    // Prepare vertex data (just x, y coordinates)
    std::vector<float> vertex_data;
    vertex_data.reserve(_all_points.size() * 2);
    
    for (auto const & point : _all_points) {
        vertex_data.push_back(point.x);
        vertex_data.push_back(point.y);
    }

    // Bind VAO and update buffer
    _vertex_array_object.bind();
    _vertex_buffer.bind();
    _vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));
    
    // Verify buffer was allocated
    GLint buffer_size = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);
    
    // Ensure vertex attributes are properly set
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    
    // Unbind in proper order
    _vertex_buffer.release();
    _vertex_array_object.release();
}

void SpatialOverlayOpenGLWidget::calculateProjectionBounds(float &left, float &right, float &bottom, float &top) const {
    if (!_data_bounds_valid || width() <= 0 || height() <= 0) {
        left = right = bottom = top = 0.0f;
        return;
    }
    
    // Calculate orthographic projection bounds (same logic as updateViewMatrices)
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
        _opengl_widget->resize(content_rect.size().toSize());
        _proxy_widget->setGeometry(content_rect);
        
        // Force update after resize
        _opengl_widget->update();
    }
}

void SpatialOverlayPlotWidget::updateVisualization() {
   
    if (!_data_manager || !_opengl_widget) {
        return;
    }

    loadPointData();
    
    // Request render update through signal
    update();
    emit renderUpdateRequested(getPlotId());
}

void SpatialOverlayPlotWidget::handleFrameJumpRequest(int64_t time_frame_index) {
    qDebug() << "SpatialOverlayPlotWidget: Frame jump requested to frame:" << time_frame_index;
    emit frameJumpRequested(time_frame_index);
}

void SpatialOverlayPlotWidget::loadPointData() {
    std::unordered_map<QString, std::shared_ptr<PointData>> point_data_map;

    for (QString const & key: _point_data_keys) {
        auto point_data = _data_manager->getData<PointData>(key.toStdString());
        if (point_data) {
            point_data_map[key] = point_data;
        }
    }
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
    
    // Connect property change signals to trigger updates
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::pointSizeChanged,
            this, [this](float) { 
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });
    
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::zoomLevelChanged,
            this, [this](float) { 
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });
    
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::panOffsetChanged,
            this, [this](float, float) { 
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });
    
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::tooltipsEnabledChanged,
            this, [this](bool) { 
                emit renderingPropertiesChanged();
            });
    
    // Connect highlight state changes to trigger scene graph updates
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::highlightStateChanged,
            this, [this]() {
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
            });
}