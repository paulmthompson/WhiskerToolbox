#include "SpatialOverlayOpenGLWidget.hpp"
#include "ShaderManager/ShaderManager.hpp"

#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Lines/LineDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Masks/MaskDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Points/PointDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/NoneSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PointSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <unordered_set>


SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
      _opengl_resources_initialized(false),
      _zoom_level(1.0f),
      _pan_offset_x(0.0f),
      _pan_offset_y(0.0f),
      _point_size(8.0f),
      _line_width(2.0f),
      _is_panning(false),
      _data_bounds_valid(false),
      _tooltips_enabled(true),
      _pending_update(false),
      _hover_processing_active(false),
      _selection_mode(SelectionMode::PointSelection),
      _selection_handler(std::make_unique<PointSelectionHandler>(calculateWorldTolerance(10.0f))) {

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Request OpenGL 4.3 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);// Enable multisampling for smooth points
    setFormat(format);

    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500);// 500ms delay for tooltip
    connect(_tooltip_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::_handleTooltipTimer);

    _tooltip_refresh_timer = new QTimer(this);
    _tooltip_refresh_timer->setInterval(100);// Refresh every 100ms to keep tooltip visible
    connect(_tooltip_refresh_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::handleTooltipRefresh);

    // Hover debounce timer - delays expensive polygon union calculations
    _hover_debounce_timer = new QTimer(this);
    _hover_debounce_timer->setSingleShot(true);
    _hover_debounce_timer->setInterval(50);// 50ms delay for hover processing
    connect(_hover_debounce_timer, &QTimer::timeout, this, &SpatialOverlayOpenGLWidget::processHoverDebounce);

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

    // Initialize data bounds
    _data_min_x = _data_max_x = _data_min_y = _data_max_y = 0.0f;
}

SpatialOverlayOpenGLWidget::~SpatialOverlayOpenGLWidget() {
    cleanupOpenGLResources();
}

//========== Point Data ==========

void SpatialOverlayOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;

    // Connect to group manager signals to handle updates
    if (_group_manager) {
        connect(_group_manager, &GroupManager::pointAssignmentsChanged,
                this, [this](std::unordered_set<int> const & affected_groups) {
                    Q_UNUSED(affected_groups)
                    // Refresh visualization data for all point visualizations
                    for (auto & [key, viz]: _point_data_visualizations) {
                        if (viz) {
                            viz->refreshGroupRenderData();
                        }
                    }
                    update();// Trigger a repaint
                });

        connect(_group_manager, &GroupManager::groupModified,
                this, [this](int group_id) {
                    Q_UNUSED(group_id)
                    // Color changes don't require vertex data refresh, just re-render
                    update();
                });
    }

    // Update existing point data visualizations with the group manager
    for (auto & [key, viz]: _point_data_visualizations) {
        if (viz) {
            viz->setGroupManager(_group_manager);
        }
    }

    qDebug() << "SpatialOverlayOpenGLWidget: Set group manager for" << _point_data_visualizations.size() << "visualizations";
}

void SpatialOverlayOpenGLWidget::setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const & point_data_map) {
    // Clear existing visualizations
    _point_data_visualizations.clear();

    // Set different colors for different datasets
    static std::vector<QVector4D> colors = {
            QVector4D(1.0f, 0.0f, 0.0f, 1.0f),// Red
            QVector4D(0.0f, 1.0f, 0.0f, 1.0f),// Green
            QVector4D(0.0f, 0.0f, 1.0f, 1.0f),// Blue
            QVector4D(1.0f, 1.0f, 0.0f, 1.0f),// Yellow
            QVector4D(1.0f, 0.0f, 1.0f, 1.0f),// Magenta
            QVector4D(0.0f, 1.0f, 1.0f, 1.0f),// Cyan
    };

    // Create visualization for each PointData
    size_t color_index = 0;
    for (auto const & [key, point_data]: point_data_map) {
        if (!point_data) {
            qDebug() << "SpatialOverlayOpenGLWidget: Null point data for key:" << key;
            continue;
        }

        auto viz = std::make_unique<PointDataVisualization>(key, point_data, _group_manager);
        viz->m_color = colors[color_index % colors.size()];
        color_index++;

        qDebug() << "SpatialOverlayOpenGLWidget: Created visualization for" << key
                 << "with" << viz->m_vertex_data.size() / 3 << "points";

        _point_data_visualizations[key] = std::move(viz);
    }

    calculateDataBounds();
    updateViewMatrices();

    // Ensure OpenGL context is current before forcing repaint
    if (context() && context()->isValid()) {
        makeCurrent();
        update();
        repaint();
        QApplication::processEvents();
        doneCurrent();
    } else {
        update();
    }
}

//========== Mask Data ==========

void SpatialOverlayOpenGLWidget::setMaskData(std::unordered_map<QString, std::shared_ptr<MaskData>> const & mask_data_map) {
    // Clear existing visualizations
    _mask_data_visualizations.clear();

    qDebug() << "SpatialOverlayOpenGLWidget: Setting mask data with" << mask_data_map.size() << "keys";

    // Set different colors for different datasets
    static std::vector<QVector4D> colors = {
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
            QVector4D(1.0f, 0.0f, 0.0f, 0.5f),// Red with transparency
    };

    // Create visualization for each MaskData
    size_t color_index = 0;
    for (auto const & [key, mask_data]: mask_data_map) {
        if (!mask_data) {
            qDebug() << "SpatialOverlayOpenGLWidget: Null mask data for key:" << key;
            continue;
        }

        auto viz = std::make_unique<MaskDataVisualization>(key, mask_data);
        viz->color = colors[color_index % colors.size()];
        color_index++;

        qDebug() << "SpatialOverlayOpenGLWidget: Created mask visualization for" << key
                 << "with" << mask_data->size() << "time frames"
                 << "color:" << viz->color.x() << viz->color.y() << viz->color.z() << viz->color.w();

        _mask_data_visualizations[key] = std::move(viz);
    }

    calculateDataBounds();
    updateViewMatrices();

    // Ensure OpenGL context is current before forcing repaint
    if (context() && context()->isValid()) {
        makeCurrent();
        update();
        repaint();
        QApplication::processEvents();
        doneCurrent();
    } else {
        update();
    }
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedMasks() const {
    size_t total = 0;
    for (auto const & [key, viz]: _mask_data_visualizations) {
        total += viz->selected_masks.size();
    }
    return total;
}

void SpatialOverlayOpenGLWidget::setPointSize(float point_size) {
    float new_point_size = std::max(1.0f, std::min(50.0f, point_size));// Clamp between 1 and 50 pixels
    if (new_point_size != _point_size) {
        _point_size = new_point_size;
        emit pointSizeChanged(_point_size);

        // Use throttled update for better performance
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::setLineWidth(float line_width) {
    float new_line_width = std::max(0.5f, std::min(20.0f, line_width));// Clamp between 0.5 and 20 pixels
    if (new_line_width != _line_width) {
        _line_width = new_line_width;
        emit lineWidthChanged(_line_width);

        // Use throttled update for better performance
        requestThrottledUpdate();
    }
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedPoints() const {
    size_t total = 0;
    for (auto const & [key, viz]: _point_data_visualizations) {
        total += viz->m_selected_points.size();
    }
    return total;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedLines() const {
    size_t total = 0;
    for (auto const & [key, viz]: _line_data_visualizations) {
        total += viz->m_selected_lines.size();
    }
    return total;
}

void SpatialOverlayOpenGLWidget::applyTimeRangeFilter(int start_frame, int end_frame) {

    for (auto & [key, pointViz]: _point_data_visualizations) {
        pointViz->setTimeRangeEnabled(true);
        pointViz->setTimeRange(start_frame, end_frame);
    }

    for (auto & [key, lineViz]: _line_data_visualizations) {

        lineViz->setTimeRangeEnabled(true);
        lineViz->setTimeRange(start_frame, end_frame);
    }
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::calculateDataBounds() {
    if (_point_data_visualizations.empty() && _mask_data_visualizations.empty() && _line_data_visualizations.empty()) {
        _data_bounds_valid = false;
        return;
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    bool has_points = false;

    // Calculate bounds from point data
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz->m_visible || viz->m_vertex_data.empty()) continue;

        // Iterate through vertex data (x, y, group_id triplets)
        for (size_t i = 0; i < viz->m_vertex_data.size(); i += 3) {
            float x = viz->m_vertex_data[i];
            float y = viz->m_vertex_data[i + 1];
            // Skip group_id at i + 2

            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
            has_points = true;
        }
    }

    // Calculate bounds from mask data
    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz->visible) continue;

        min_x = std::min(min_x, viz->world_min_x);
        max_x = std::max(max_x, viz->world_max_x);
        min_y = std::min(min_y, viz->world_min_y);
        max_y = std::max(max_y, viz->world_max_y);
        has_points = true;
    }

    // Calculate bounds from line data
    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz->m_visible || viz->m_vertex_data.empty()) continue;

        // Iterate through vertex data (x, y pairs)
        for (size_t i = 0; i < viz->m_vertex_data.size(); i += 2) {
            float x = viz->m_vertex_data[i];
            float y = viz->m_vertex_data[i + 1];

            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
            has_points = true;
        }
    }

    if (!has_points) {
        _data_bounds_valid = false;
        return;
    }

    // Add some padding
    float padding_x = (max_x - min_x) * 0.1f;
    float padding_y = (max_y - min_y) * 0.1f;

    _data_min_x = min_x - padding_x;
    _data_max_x = max_x + padding_x;
    _data_min_y = min_y - padding_y;
    _data_max_y = max_y + padding_y;

    _data_bounds_valid = true;
}

//========== Line Data ==========

void SpatialOverlayOpenGLWidget::setLineData(std::unordered_map<QString, std::shared_ptr<LineData>> const & line_data_map) {
    // Clear existing visualizations
    _line_data_visualizations.clear();

    // Set different colors for different datasets
    static std::vector<QVector4D> colors = {
            QVector4D(0.0f, 0.0f, 1.0f, 1.0f),// Blue
            QVector4D(0.0f, 1.0f, 0.0f, 1.0f),// Green
            QVector4D(1.0f, 0.0f, 0.0f, 1.0f),// Red
            QVector4D(1.0f, 1.0f, 0.0f, 1.0f),// Yellow
            QVector4D(1.0f, 0.0f, 1.0f, 1.0f),// Magenta
            QVector4D(0.0f, 1.0f, 1.0f, 1.0f),// Cyan
    };

    // Create visualization for each LineData
    size_t color_index = 0;
    for (auto const & [key, line_data]: line_data_map) {
        if (!line_data) {
            qDebug() << "SpatialOverlayOpenGLWidget: Null line data for key:" << key;
            continue;
        }

        auto viz = std::make_unique<LineDataVisualization>(key, line_data);
        viz->m_color = colors[color_index % colors.size()];
        color_index++;

        qDebug() << "SpatialOverlayOpenGLWidget: Created line visualization for" << key
                 << "with" << viz->m_line_identifiers.size() << "lines";

        _line_data_visualizations[key] = std::move(viz);
    }

    calculateDataBounds();
    updateViewMatrices();

    // Ensure OpenGL context is current before forcing repaint
    if (context() && context()->isValid()) {
        makeCurrent();
        update();
        repaint();
        QApplication::processEvents();
        doneCurrent();
    } else {
        update();
    }
}


//========== View and Interaction ==========

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

void SpatialOverlayOpenGLWidget::setTooltipsEnabled(bool enabled) {
    if (enabled != _tooltips_enabled) {
        _tooltips_enabled = enabled;
        emit tooltipsEnabledChanged(_tooltips_enabled);

        // Hide any currently visible tooltip when disabling
        if (!_tooltips_enabled) {
            _tooltip_timer->stop();
            _tooltip_refresh_timer->stop();
            QToolTip::hideText();

            // Clear all hover states
            for (auto const & [key, viz]: _point_data_visualizations) {
                viz->m_current_hover_point = nullptr;
            }
        }
    }
}

void SpatialOverlayOpenGLWidget::makeSelection() {
    auto context = createRenderingContext();

    if (_selection_handler.valueless_by_exception()) {
        return;
    }

    // If there's no active selection region, it implies a clear command
    bool should_clear = std::visit([](auto & handler) {
        return handler->getActiveSelectionRegion() == nullptr;
    },
                                   _selection_handler);

    if (should_clear) {
        clearSelection();
        return;
    }

    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _mask_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->applySelection(_selection_handler, context);
    }

    // Emit selection changed signal with updated counts
    size_t total_selected = getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines();
    emit selectionChanged(total_selected, QString(), 0);

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setSelectionMode(SelectionMode mode) {
    if (mode != _selection_mode) {

        std::visit([](auto & handler) {
            handler->deactivate();
            handler->clearNotificationCallback();
        },
                   _selection_handler);

        _selection_mode = mode;
        emit selectionModeChanged(_selection_mode);

        // Update cursor based on selection mode
        if (_selection_mode == SelectionMode::PolygonSelection) {
            _selection_handler = std::make_unique<PolygonSelectionHandler>();
            setCursor(Qt::CrossCursor);
        } else if (_selection_mode == SelectionMode::LineIntersection) {
            _selection_handler = std::make_unique<LineSelectionHandler>();
            setCursor(Qt::CrossCursor);
        } else if (_selection_mode == SelectionMode::PointSelection) {
            _selection_handler = std::make_unique<PointSelectionHandler>(calculateWorldTolerance(10.0f));
            setCursor(Qt::ArrowCursor);
        } else if (_selection_mode == SelectionMode::None) {
            _selection_handler = std::make_unique<NoneSelectionHandler>();
            setCursor(Qt::ArrowCursor);
        }

        std::visit([this](auto & handler) {
            handler->setNotificationCallback([this]() {
                makeSelection();
            });
        },
                   _selection_handler);

        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::initializeGL() {
    if (!initializeOpenGLFunctions()) {
        return;
    }

    // Set clear color
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    // Enable blending for transparency with premultiplied alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

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

    if (!_data_bounds_valid || !_opengl_resources_initialized) {
        return;
    }

    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;

    renderPoints();
    renderMasks();
    renderLines();

    std::visit([mvp_matrix](auto & handler) {
        if (handler) {
            handler->render(mvp_matrix);
        }
    },
               _selection_handler);

    renderCommonOverlay();
}

void SpatialOverlayOpenGLWidget::paintEvent(QPaintEvent * event) {
    QOpenGLWidget::paintEvent(event);
}

void SpatialOverlayOpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateViewMatrices();

    // No need to update vertex buffer during resize - the data is already on GPU
    // Just update the viewport and projection matrix
}

void SpatialOverlayOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());


    std::visit([event, world_pos](auto & handler) {
        if (handler) {
            handler->mousePressEvent(event, world_pos);
        }
    },
               _selection_handler);

    if (event->button() == Qt::LeftButton) {
        // Regular left click - start panning (if not in polygon or line selection mode)
        if (_selection_mode != SelectionMode::PolygonSelection && _selection_mode != SelectionMode::LineIntersection) {
            _is_panning = true;
            _last_mouse_pos = event->pos();
        }
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        // Accept right click so we get the corresponding mouseReleaseEvent
        event->accept();
    } else {
        // Let other mouse buttons propagate to parent widget
        event->ignore();
        return;
    }

    // Clear tooltips and hover processing during interaction
    _tooltip_timer->stop();
    _tooltip_refresh_timer->stop();
    _hover_debounce_timer->stop();
    _hover_processing_active = false;
    QToolTip::hideText();

    qDebug() << "SpatialOverlayOpenGLWidget: Mouse press - disabled hover processing, selection mode:" << (int) _selection_mode;

    // Clear hover states
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->m_current_hover_point = nullptr;
    }

    // Clear line hover states as well
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->setHoverLine(std::nullopt);
    }

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    _current_mouse_pos = event->pos();

    updateMouseWorldPosition(event->pos().x(), event->pos().y());

    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());
    std::visit([event, world_pos](auto & handler) {
        handler->mouseMoveEvent(event, world_pos);
    },
               _selection_handler);

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

        if (_last_mouse_pos == _current_mouse_pos) {
            return;
        } else {
            qDebug() << "SpatialOverlayOpenGLWidget: Mouse moved from"
                     << _last_mouse_pos << "to" << _current_mouse_pos;
        }

        // Handle tooltip logic if tooltips are enabled
        if (_tooltips_enabled) {
            // Store the current mouse position for debounced processing
            _pending_hover_pos = _current_mouse_pos;

            // If hover processing is currently active, skip this event
            if (_hover_processing_active) {
                qDebug() << "SpatialOverlayOpenGLWidget: Skipping hover processing - already active";
                return;
            }

            // Start or restart the debounce timer
            _hover_debounce_timer->stop();
            _hover_debounce_timer->start();

            qDebug() << "SpatialOverlayOpenGLWidget: Starting hover debounce timer for position" << _pending_hover_pos;
        }
        _last_mouse_pos = _current_mouse_pos;
        event->accept();
    }
}

void SpatialOverlayOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {

    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());
    std::visit([event, world_pos](auto & handler) {
        handler->mouseReleaseEvent(event, world_pos);
    },
               _selection_handler);

    if (event->button() == Qt::LeftButton) {
        _is_panning = false;

        // Re-enable hover processing after interaction ends
        // This ensures hover works again even if the click was in an incompatible selection mode
        _hover_processing_active = false;

        // Trigger hover processing to restart immediately if mouse is over content
        if (_tooltips_enabled && _data_bounds_valid) {
            _pending_hover_pos = event->pos();
            _hover_debounce_timer->stop();
            _hover_debounce_timer->start();
            qDebug() << "SpatialOverlayOpenGLWidget: Mouse release - restarted hover processing at" << event->pos();
        } else {
            qDebug() << "SpatialOverlayOpenGLWidget: Mouse release - hover processing not restarted (tooltips:" << _tooltips_enabled << ", bounds_valid:" << _data_bounds_valid << ")";
        }

        event->accept();
    } else if (event->button() == Qt::RightButton) {

        qDebug() << "SpatialOverlayOpenGLWidget: Right-click detected at" << event->pos();
        showContextMenu(event->pos());
        event->accept();
    } else {
        event->ignore();
    }

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        qDebug() << "SpatialOverlayOpenGLWidget: Double-click detected at" << event->pos();

        QVector2D world_pos = screenToWorld(event->pos().x(), event->pos().y());
        float tolerance = calculateWorldTolerance(10.0f);

        for (auto const & [key, viz]: _point_data_visualizations) {
            auto frame_index = viz->handleDoubleClick(world_pos, tolerance);
            if (frame_index.has_value()) {
                qDebug() << "SpatialOverlayOpenGLWidget: Double-click on point in" << key
                         << "frame:" << frame_index.value();
                emit frameJumpRequested(frame_index.value(), key);
                event->accept();
                return;
            }
        }

        qDebug() << "SpatialOverlayOpenGLWidget: Double-click but no point found near cursor";
        event->ignore();
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
    _hover_debounce_timer->stop();
    QToolTip::hideText();

    // Clear hover processing state
    _hover_processing_active = false;

    // Clear all hover states
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->clearHover();
    }

    // Clear line hover states
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->setHoverLine(std::nullopt);
    }

    for (auto const & [key, viz]: _mask_data_visualizations) {
        viz->clearHover();
    }

    requestThrottledUpdate();

    QOpenGLWidget::leaveEvent(event);
}

void SpatialOverlayOpenGLWidget::keyPressEvent(QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
        std::visit([event](auto & handler) {
            if (handler) {
                handler->keyPressEvent(event);
            }
        },
                   _selection_handler);

        requestThrottledUpdate();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (getTotalSelectedPoints() > 0) {
            clearSelection();
            event->accept();
            return;
        }
    }

    // Let parent handle other keys
    QOpenGLWidget::keyPressEvent(event);
}

void SpatialOverlayOpenGLWidget::_handleTooltipTimer() {
    if (!_data_bounds_valid || !_tooltips_enabled) return;

    QStringList tooltip_parts;

    // Get tooltips from point visualizations
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz->m_current_hover_point) {
            tooltip_parts << viz->getTooltipText();
        }
    }

    // Get tooltips from mask visualizations
    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz->current_hover_entries.empty()) {
            tooltip_parts << viz->getTooltipText();
        }
    }

    // Get tooltips from line visualizations
    for (auto const & [key, viz]: _line_data_visualizations) {
        if (viz->m_has_hover_line) {
            tooltip_parts << viz->getTooltipText();
        }
    }

    if (!tooltip_parts.isEmpty()) {
        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_parts.join("<hr>"), this);
        _tooltip_refresh_timer->start();
    } else {
        _tooltip_refresh_timer->stop();
        QToolTip::hideText();
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
    if (!_data_bounds_valid || !_tooltips_enabled) {
        _tooltip_refresh_timer->stop();
        return;
    }

    // The logic is the same as the initial timer, so we can just call it.
    _handleTooltipTimer();
}

void SpatialOverlayOpenGLWidget::clearSelection() {
    bool had_selection = false;

    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz->m_selected_points.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz->selected_masks.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz->m_selected_lines.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    if (had_selection) {
        emit selectionChanged(0, QString(), 0);
        requestThrottledUpdate();
        qDebug() << "SpatialOverlayOpenGLWidget: All selections cleared";
    }
}

QVector2D SpatialOverlayOpenGLWidget::screenToWorld(int screen_x, int screen_y) const {
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);

    if (left == right || bottom == top) {
        return QVector2D(0, 0);
    }

    // Convert screen coordinates to world coordinates using the projection bounds
    float world_x = left + (static_cast<float>(screen_x) / width()) * (right - left);
    float world_y = top - (static_cast<float>(screen_y) / height()) * (top - bottom);// Y is flipped in screen coordinates

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
    int screen_y = static_cast<int>(((top - world_y) / (top - bottom)) * height());// Y is flipped in screen coordinates

    return QPoint(screen_x, screen_y);
}

float SpatialOverlayOpenGLWidget::calculateWorldTolerance(float screen_tolerance) const {
    QVector2D world_pos = screenToWorld(0, 0);
    QVector2D world_pos_offset = screenToWorld(static_cast<int>(screen_tolerance), 0);
    return std::abs(world_pos_offset.x() - world_pos.x());
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
    if (_point_data_visualizations.empty() || !_opengl_resources_initialized) {
        return;
    }

    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;

    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->render(mvp_matrix, _point_size);
    }
}

void SpatialOverlayOpenGLWidget::renderMasks() {
    if (_mask_data_visualizations.empty() || !_opengl_resources_initialized) {
        return;
    }

    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
    for (auto const & [key, viz]: _mask_data_visualizations) {
        viz->render(mvp_matrix);
    }
}

void SpatialOverlayOpenGLWidget::renderLines() {
    if (_line_data_visualizations.empty() || !_opengl_resources_initialized) {
        return;
    }

    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;

    // Render all lines for each LineData
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->render(mvp_matrix, _line_width);// Use member variable for line width
    }
}

void SpatialOverlayOpenGLWidget::initializeOpenGLResources() {
    qDebug() << "SpatialOverlayOpenGLWidget: Initializing OpenGL resources";

    if (!ShaderManager::instance().loadProgram("point", ":/shaders/point.vert", ":/shaders/point.frag", "", ShaderSourceType::Resource)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to load point shader program from ShaderManager";
        return;
    }

    if (!ShaderManager::instance().loadProgram("line", ":/shaders/line.vert", ":/shaders/line.frag", "", ShaderSourceType::Resource)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to load line shader program from ShaderManager";
        return;
    }

    if (!ShaderManager::instance().loadProgram("texture", ":/shaders/texture.vert", ":/shaders/texture.frag", "", ShaderSourceType::Resource)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to load texture shader program from ShaderManager";
        return;
    }

    _opengl_resources_initialized = true;
    qDebug() << "SpatialOverlayOpenGLWidget: OpenGL resources initialized successfully";
}

void SpatialOverlayOpenGLWidget::cleanupOpenGLResources() {
    if (_opengl_resources_initialized) {
        makeCurrent();

        for (auto const & [key, viz]: _point_data_visualizations) {
            viz->cleanupOpenGLResources();
        }

        for (auto const & [key, viz]: _mask_data_visualizations) {
            viz->cleanupOpenGLResources();
        }

        _opengl_resources_initialized = false;

        doneCurrent();
    }
}

void SpatialOverlayOpenGLWidget::calculateProjectionBounds(float & left, float & right, float & bottom, float & top) const {
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
    float padding = 1.1f;// 10% padding
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

void SpatialOverlayOpenGLWidget::processHoverDebounce() {
    if (!_data_bounds_valid || !_tooltips_enabled || _hover_processing_active) {
        // Skip processing if data is invalid, tooltips are disabled, or we're already processing
        return;
    }

    // Set processing flag to prevent new hover calculations
    _hover_processing_active = true;

    qDebug() << "SpatialOverlayOpenGLWidget: Processing debounced hover at" << _pending_hover_pos;

    QVector2D world_pos = screenToWorld(_pending_hover_pos.x(), _pending_hover_pos.y());
    float tolerance = calculateWorldTolerance(10.0f);
    bool needs_tooltip_update = false;

    // Delegate hover handling to each point data visualization
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz->handleHover(world_pos, tolerance)) {
            needs_tooltip_update = true;
        }
    }

    // Delegate hover handling to each mask data visualization
    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (viz->handleHover(world_pos)) {
            needs_tooltip_update = true;
        }
    }

    // Delegate hover handling to each line data visualization
    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
    qDebug() << "SpatialOverlayOpenGLWidget: Processing" << _line_data_visualizations.size() << "line visualizations";
    for (auto const & [key, viz]: _line_data_visualizations) {
        qDebug() << "SpatialOverlayOpenGLWidget: Calling handleHover on line viz" << key;
        if (viz->handleHover(_pending_hover_pos, size(), mvp_matrix)) {
            needs_tooltip_update = true;
        }
    }

    if (needs_tooltip_update) {
        _tooltip_timer->stop();
        _tooltip_refresh_timer->stop();
        _tooltip_timer->start();
    }

    requestThrottledUpdate();

    // Clear processing flag to allow new hover calculations
    _hover_processing_active = false;
}

RenderingContext SpatialOverlayOpenGLWidget::createRenderingContext() const {
    float left, right, bottom, top;
    calculateProjectionBounds(left, right, bottom, top);

    return {
            _model_matrix,
            _view_matrix,
            _projection_matrix,
            rect(),
            QRectF(QPointF(left, top), QPointF(right, bottom))};
}


void SpatialOverlayOpenGLWidget::updateMouseWorldPosition(int screen_x, int screen_y) {
    _current_mouse_world_pos = screenToWorld(screen_x, screen_y);
}

void SpatialOverlayOpenGLWidget::renderCommonOverlay() {
    // Render tooltips and other common overlay elements
    // This will be expanded to handle tooltip rendering and other common UI elements
}

//========== Visibility Management ==========

void SpatialOverlayOpenGLWidget::hideSelectedItems() {
    size_t total_hidden = 0;

    for (auto const & [key, viz]: _line_data_visualizations) {
        total_hidden += viz->hideSelectedLines();
    }

    for (auto const & [key, viz]: _point_data_visualizations) {
        total_hidden += viz->hideSelectedPoints();
    }

    // TODO: Hide selected masks (no-op for now)

    if (total_hidden > 0) {
        qDebug() << "SpatialOverlayOpenGLWidget: Hidden" << total_hidden << "items";
        requestThrottledUpdate();

        // Update selection count since hidden items are no longer selected
        emit selectionChanged(getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines(), QString(), 0);
    }
}

void SpatialOverlayOpenGLWidget::showAllItemsCurrentDataset() {
    // For now, we'll show all items across all datasets since we don't have a concept of "current" dataset
    // This can be enhanced later when there's a UI concept of active/current dataset
    showAllItemsAllDatasets();
}

void SpatialOverlayOpenGLWidget::showAllItemsAllDatasets() {
    size_t total_shown = 0;

    for (auto const & [key, viz]: _line_data_visualizations) {
        total_shown += viz->showAllLines();
    }

    for (auto const & [key, viz]: _point_data_visualizations) {
        total_shown += viz->showAllPoints();
    }

    // TODO: Show all hidden masks (no-op for now)

    if (total_shown > 0) {
        qDebug() << "SpatialOverlayOpenGLWidget: Showed" << total_shown << "items";
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::showContextMenu(QPoint const & pos) {
    QMenu contextMenu(this);

    // Check if we have any selected items
    size_t total_selected = getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines();

    // Add group assignment options if we have selected points and a group manager
    if (total_selected > 0 && _group_manager) {
        // Add "Assign to Group" submenu
        QMenu * assignGroupMenu = contextMenu.addMenu("Assign to Group");

        // Add "New Group" option
        QAction * newGroupAction = assignGroupMenu->addAction("Create New Group");
        connect(newGroupAction, &QAction::triggered, this, [this]() {
            assignSelectedPointsToNewGroup();
        });

        assignGroupMenu->addSeparator();

        // Add existing groups
        auto const & groups = _group_manager->getGroups();
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            auto const & group = it.value();
            QAction * groupAction = assignGroupMenu->addAction(group.name);
            int group_id = group.id;
            connect(groupAction, &QAction::triggered, this, [this, group_id]() {
                assignSelectedPointsToGroup(group_id);
            });
        }

        // Add "Remove from Group" option
        QAction * ungroupAction = contextMenu.addAction("Remove from Group");
        connect(ungroupAction, &QAction::triggered, this, [this]() {
            ungroupSelectedPoints();
        });

        contextMenu.addSeparator();
    }

    // Add "Hide Selected" option if there are selected items
    if (total_selected > 0) {
        QAction * hideAction = contextMenu.addAction(QString("Hide Selected (%1 items)").arg(total_selected));
        connect(hideAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::hideSelectedItems);
    }

    // Add "Show All" submenu
    QMenu * showAllMenu = contextMenu.addMenu("Show All");

    QAction * showCurrentAction = showAllMenu->addAction("Show All (Current Dataset)");
    connect(showCurrentAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsCurrentDataset);

    QAction * showAllAction = showAllMenu->addAction("Show All (All Datasets)");
    connect(showAllAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsAllDatasets);

    // Show the menu at the cursor position
    contextMenu.exec(mapToGlobal(pos));
}

//========== Group Assignment Methods ==========

void SpatialOverlayOpenGLWidget::assignSelectedPointsToNewGroup() {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for group assignment";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to assign to new group";
        return;
    }

    // Create a new group
    QString group_name = QString("Group %1").arg(_group_manager->getGroups().size() + 1);
    int group_id = _group_manager->createGroup(group_name);

    // Assign selected points to the new group
    _group_manager->assignPointsToGroup(group_id, selected_point_ids);

    // Clear selection after assignment
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Assigned" << selected_point_ids.size()
             << "points to new group" << group_id;
}

void SpatialOverlayOpenGLWidget::assignSelectedPointsToGroup(int group_id) {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for group assignment";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to assign to group" << group_id;
        return;
    }

    // Assign selected points to the specified group
    _group_manager->assignPointsToGroup(group_id, selected_point_ids);

    // Clear selection after assignment
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Assigned" << selected_point_ids.size()
             << "points to group" << group_id;
}

void SpatialOverlayOpenGLWidget::ungroupSelectedPoints() {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for ungrouping";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to ungroup";
        return;
    }

    // Remove selected points from all groups
    _group_manager->ungroupPoints(selected_point_ids);

    // Clear selection after ungrouping
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Ungrouped" << selected_point_ids.size() << "points";
}
