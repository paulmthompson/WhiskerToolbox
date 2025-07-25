#include "SpatialOverlayOpenGLWidget.hpp"
#include "ShaderManager/ShaderManager.hpp"

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Lines/LineDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Masks/MaskDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Points/PointDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/NoneSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <unordered_set>


SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent),
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
      _hover_processing_active(false),
      _selection_mode(SelectionMode::PointSelection),
      _selection_handler(std::make_unique<NoneSelectionHandler>()) {

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
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

        auto viz = std::make_unique<PointDataVisualization>(key, point_data);
        viz->color = colors[color_index % colors.size()];
        color_index++;

        qDebug() << "SpatialOverlayOpenGLWidget: Created visualization for" << key
                 << "with" << viz->vertex_data.size() / 2 << "points";

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

std::vector<std::pair<MaskDataVisualization *, std::vector<RTreeEntry<MaskIdentifier>>>> SpatialOverlayOpenGLWidget::findMasksNear(int screen_x, int screen_y) const {
    std::vector<std::pair<MaskDataVisualization *, std::vector<RTreeEntry<MaskIdentifier>>>> result;

    if (_mask_data_visualizations.empty()) {
        return result;
    }

    QVector2D world_pos = screenToWorld(screen_x, screen_y);
    BoundingBox point_bbox(world_pos.x(), world_pos.y(), world_pos.x(), world_pos.y());

    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz->visible || !viz->spatial_index) continue;

        std::vector<RTreeEntry<MaskIdentifier>> entries;
        viz->spatial_index->query(point_bbox, entries);

        if (!entries.empty()) {
            result.emplace_back(viz.get(), std::move(entries));
        }
    }

    return result;
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


std::pair<PointDataVisualization *, QuadTreePoint<int64_t> const *> SpatialOverlayOpenGLWidget::findPointNear(int screen_x, int screen_y, float tolerance_pixels) const {
    if (_point_data_visualizations.empty()) {
        return {nullptr, nullptr};
    }

    QVector2D world_pos = screenToWorld(screen_x, screen_y);
    float world_tolerance = calculateWorldTolerance(tolerance_pixels);

    QuadTreePoint<int64_t> const * nearest_point = nullptr;
    PointDataVisualization * nearest_viz = nullptr;
    float nearest_distance = std::numeric_limits<float>::max();

    // Query all PointData QuadTrees
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz->visible || !viz->spatial_index) continue;

        auto const * candidate = viz->spatial_index->findNearest(
                world_pos.x(), world_pos.y(), world_tolerance);

        if (candidate) {
            float distance = std::sqrt(std::pow(candidate->x - world_pos.x(), 2) +
                                       std::pow(candidate->y - world_pos.y(), 2));
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest_point = candidate;
                nearest_viz = viz.get();
            }
        }
    }

    return {nearest_viz, nearest_point};
}

PointDataVisualization * SpatialOverlayOpenGLWidget::getCurrentHoverVisualization() const {
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz->current_hover_point) {
            return viz.get();
        }
    }
    return nullptr;
}

std::pair<LineDataVisualization *, std::optional<LineIdentifier>> SpatialOverlayOpenGLWidget::findLineNear(int screen_x, int screen_y) const {
    if (_line_data_visualizations.empty()) {
        return {nullptr, std::nullopt};
    }

    // Try to find a line at the screen position using GPU picking
    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz->visible) continue;

        // Get widget width and height
        int widget_width = width();
        int widget_height = height();

        auto line_id = viz->getLineAtScreenPosition(screen_x, screen_y, widget_width, widget_height);
        if (line_id.has_value()) {
            return {viz.get(), line_id};
        }
    }

    return {nullptr, std::nullopt};
}

LineDataVisualization * SpatialOverlayOpenGLWidget::getCurrentHoverLineVisualization() const {
    for (auto const & [key, viz]: _line_data_visualizations) {
        if (viz->has_hover_line) {
            return viz.get();
        }
    }
    return nullptr;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedPoints() const {
    size_t total = 0;
    for (auto const & [key, viz]: _point_data_visualizations) {
        total += viz->selected_points.size();
    }
    return total;
}

std::vector<std::pair<QString, std::vector<LineIdentifier>>> SpatialOverlayOpenGLWidget::getSelectedLineData() const {
    std::vector<std::pair<QString, std::vector<LineIdentifier>>> result;

    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz->selected_lines.empty()) {
            std::vector<LineIdentifier> selected_lines;
            selected_lines.reserve(viz->selected_lines.size());
            for (auto const & line_id: viz->selected_lines) {
                selected_lines.push_back(line_id);
            }
            result.emplace_back(key, std::move(selected_lines));
        }
    }

    return result;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedLines() const {
    size_t total = 0;
    for (auto const & [key, viz]: _line_data_visualizations) {
        total += viz->selected_lines.size();
    }
    return total;
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
        if (!viz->visible || viz->vertex_data.empty()) continue;

        // Iterate through vertex data (x, y pairs)
        for (size_t i = 0; i < viz->vertex_data.size(); i += 2) {
            float x = viz->vertex_data[i];
            float y = viz->vertex_data[i + 1];

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
        if (!viz->visible || viz->vertex_data.empty()) continue;

        // Iterate through vertex data (x, y pairs)
        for (size_t i = 0; i < viz->vertex_data.size(); i += 2) {
            float x = viz->vertex_data[i];
            float y = viz->vertex_data[i + 1];

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
        viz->color = colors[color_index % colors.size()];
        color_index++;

        qDebug() << "SpatialOverlayOpenGLWidget: Created line visualization for" << key
                 << "with" << viz->line_identifiers.size() << "lines";

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
                viz->current_hover_point = nullptr;
            }
        }
    }
}

void SpatialOverlayOpenGLWidget::makeSelection() {

    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _mask_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->applySelection(_selection_handler);
    }

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setSelectionMode(SelectionMode mode) {
    if (mode != _selection_mode) {

        std::visit([](auto & handler) {
                handler->deactivate();
                handler->clearNotificationCallback();
        }, _selection_handler);

        _selection_mode = mode;
        emit selectionModeChanged(_selection_mode);

        // Update cursor based on selection mode
        if (_selection_mode == SelectionMode::PolygonSelection) {
            _selection_handler = std::make_unique<PolygonSelectionHandler>();
            setCursor(Qt::CrossCursor);
        } else if (_selection_mode == SelectionMode::LineIntersection) {
            _selection_handler = std::make_unique<LineSelectionHandler>();
            setCursor(Qt::CrossCursor);
        } else if (_selection_mode == SelectionMode::None) {
            _selection_handler = std::make_unique<NoneSelectionHandler>();
            setCursor(Qt::ArrowCursor);
        }

        std::visit([this](auto & handler) {
            handler->setNotificationCallback([this]() {
                    makeSelection();
            });
        }, _selection_handler);

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

    // Render data visualizations
    renderPoints();
    renderMasks();
    renderLines();

    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
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

    requestThrottledUpdate();

    if (event->button() == Qt::LeftButton) {
        // Point and mask selection mode - requires Ctrl+Click for toggle, Shift+Click for removal
        if (_selection_mode == SelectionMode::PointSelection && (event->modifiers() & Qt::ControlModifier)) {
            // First try to find points near the click
            auto [viz, point] = findPointNear(event->pos().x(), event->pos().y());
            if (viz && point) {
                bool was_selected = viz->togglePointSelection(point);

                qDebug() << "SpatialOverlayOpenGLWidget: Point" << (was_selected ? "selected" : "deselected")
                         << "in" << viz->key << "at (" << point->x << "," << point->y << ")";

                // Emit selection changed signal with data-specific information
                emit selectionChanged(getTotalSelectedPoints(), viz->key, viz->selected_points.size());
                requestThrottledUpdate();
                event->accept();
                return;
            }

            // If no points found, try to find masks near the click
            auto mask_results = findMasksNear(event->pos().x(), event->pos().y());

            if (!mask_results.empty()) {
                for (auto const & [mask_viz, entries]: mask_results) {
                    // Refine using precise point checking
                    auto refined_masks = mask_viz->refineMasksContainingPoint(entries, world_pos.x(), world_pos.y());

                    if (!refined_masks.empty()) {
                        // Toggle the first mask found (most common use case)
                        auto const & first_mask = refined_masks[0];
                        bool was_selected = mask_viz->toggleMaskSelection(first_mask);

                        qDebug() << "SpatialOverlayOpenGLWidget: Mask" << (was_selected ? "selected" : "deselected")
                                 << "in" << mask_viz->key << "at timeframe" << first_mask.timeframe
                                 << "index" << first_mask.mask_index;

                        // Emit selection changed signal
                        emit selectionChanged(getTotalSelectedMasks(), mask_viz->key, mask_viz->selected_masks.size());
                        requestThrottledUpdate();
                        event->accept();
                        return;
                    }
                }
            }
        }

        // Point and mask removal mode - Shift+Click to remove from selection
        if (_selection_mode == SelectionMode::PointSelection && (event->modifiers() & Qt::ShiftModifier)) {
            // First try to find points near the click
            auto [viz, point] = findPointNear(event->pos().x(), event->pos().y());
            if (viz && point) {
                bool was_removed = viz->removePointFromSelection(point);

                if (was_removed) {
                    qDebug() << "SpatialOverlayOpenGLWidget: Point removed from selection"
                             << "in" << viz->key << "at (" << point->x << "," << point->y << ")";

                    // Emit selection changed signal
                    emit selectionChanged(getTotalSelectedPoints(), viz->key, viz->selected_points.size());
                    requestThrottledUpdate();
                    event->accept();
                    return;
                }
            }

            // If no points found, try to find masks near the click
            auto mask_results = findMasksNear(event->pos().x(), event->pos().y());

            if (!mask_results.empty()) {
                for (auto const & [mask_viz, entries]: mask_results) {
                    // Refine using precise point checking
                    auto refined_masks = mask_viz->refineMasksContainingPoint(entries, world_pos.x(), world_pos.y());

                    if (!refined_masks.empty()) {
                        // Remove all intersecting masks between current selection and new masks
                        size_t removed_count = mask_viz->removeIntersectingMasks(refined_masks);

                        if (removed_count > 0) {
                            qDebug() << "SpatialOverlayOpenGLWidget: Removed" << removed_count
                                     << "intersecting masks from selection in" << mask_viz->key;

                            // Emit selection changed signal
                            emit selectionChanged(getTotalSelectedMasks(), mask_viz->key, mask_viz->selected_masks.size());
                            requestThrottledUpdate();
                            event->accept();
                            return;
                        }
                    }
                }
            }
        }

        // Regular left click - start panning (if not in polygon selection mode)
        if (_selection_mode != SelectionMode::PolygonSelection) {
            _is_panning = true;
            _last_mouse_pos = event->pos();
        }
        event->accept();
    } else if (event->button() == Qt::RightButton) {

        event->ignore();

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

    // Clear hover states
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->current_hover_point = nullptr;
    }

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    _current_mouse_pos = event->pos();

    updateMouseWorldPosition(event->pos().x(), event->pos().y());

    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());
    std::visit([event, world_pos](auto & handler) {
        handler->mouseMoveEvent(event, world_pos);
    }, _selection_handler);

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
    }, _selection_handler);

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

        auto [viz, point] = findPointNear(event->pos().x(), event->pos().y());
        if (viz && point) {
            qDebug() << "SpatialOverlayOpenGLWidget: Double-click on point at ("
                     << point->x << "," << point->y << ") frame:" << point->data
                     << "data:" << viz->key;

            emit frameJumpRequested(point->data, viz->key);
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
    _hover_debounce_timer->stop();
    QToolTip::hideText();

    // Clear hover processing state
    _hover_processing_active = false;

    // Clear all hover states
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->current_hover_point = nullptr;
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

    auto [viz, point] = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
    if (viz && point && viz->current_hover_point == point) {
        QString tooltip_text = create_tooltipText(point, viz->key);
        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
        _tooltip_refresh_timer->start();
    } else {
        // Check for mask tooltips
        size_t total_hover_masks = 0;
        QStringList mask_info;

        for (auto const & [key, mask_viz]: _mask_data_visualizations) {
            if (!mask_viz->current_hover_entries.empty()) {
                total_hover_masks += mask_viz->current_hover_entries.size();
                mask_info << QString("%1: %2 masks").arg(key).arg(mask_viz->current_hover_entries.size());
            }
        }

        if (total_hover_masks > 0) {
            QString tooltip_text = QString("Masks under cursor: %1\n%2")
                                           .arg(total_hover_masks)
                                           .arg(mask_info.join("\n"));
            QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
            _tooltip_refresh_timer->start();
        }
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

    // Check if we're still hovering over the same point
    auto [viz, point] = findPointNear(_current_mouse_pos.x(), _current_mouse_pos.y());
    if (viz && point && viz->current_hover_point == point) {
        QString tooltip_text = create_tooltipText(point, viz->key);
        QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
    } else {
        // Check for mask tooltips
        size_t total_hover_masks = 0;
        QStringList mask_info;

        for (auto const & [key, mask_viz]: _mask_data_visualizations) {
            if (!mask_viz->current_hover_entries.empty()) {
                total_hover_masks += mask_viz->current_hover_entries.size();
                mask_info << QString("%1: %2 masks").arg(key).arg(mask_viz->current_hover_entries.size());
            }
        }

        if (total_hover_masks > 0) {
            QString tooltip_text = QString("Masks under cursor: %1\n%2")
                                           .arg(total_hover_masks)
                                           .arg(mask_info.join("\n"));
            QToolTip::showText(mapToGlobal(_current_mouse_pos), tooltip_text, this);
        } else {
            // No longer hovering over point or masks, stop refresh timer
            _tooltip_refresh_timer->stop();
            QToolTip::hideText();
        }
    }
}

float SpatialOverlayOpenGLWidget::calculateWorldTolerance(float screen_tolerance) const {
    QVector2D world_pos = screenToWorld(0, 0);
    QVector2D world_pos_offset = screenToWorld(static_cast<int>(screen_tolerance), 0);
    return std::abs(world_pos_offset.x() - world_pos.x());
}

void SpatialOverlayOpenGLWidget::clearSelection() {
    bool had_selection = false;

    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz->selected_points.empty()) {
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
        if (!viz->selected_lines.empty()) {
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

    // Get point shader program from ShaderManager
    auto pointProgram = ShaderManager::instance().getProgram("point");
    if (!pointProgram || !pointProgram->getNativeProgram()->bind()) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to bind point shader program";
        return;
    }

    // Set uniform matrices
    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
    pointProgram->getNativeProgram()->setUniformValue("u_mvp_matrix", mvp_matrix);

    // Enable blending for regular points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // === DRAW CALL 1: Render all regular points for each PointData ===
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->renderPoints(pointProgram->getNativeProgram(), _point_size);
    }

    // === DRAW CALL 2: Render selected points for each PointData ===
    glDisable(GL_BLEND);// Solid color for selections
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->renderSelectedPoints(pointProgram->getNativeProgram(), _point_size);
    }

    // === DRAW CALL 3: Render hover point (if any) ===
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (viz->current_hover_point) {
            viz->renderHoverPoint(pointProgram->getNativeProgram(), _point_size, _highlight_vertex_buffer, _highlight_vertex_array_object);
            break;// Only one hover point at a time
        }
    }

    // Re-enable blending
    glEnable(GL_BLEND);

    pointProgram->getNativeProgram()->release();
}

void SpatialOverlayOpenGLWidget::renderMasks() {
    if (_mask_data_visualizations.empty() || !_opengl_resources_initialized) {
        return;
    }


    if (_texture_shader_program && _texture_shader_program->bind()) {

        // === DRAW CALL 1: Render binary image textures ===
        QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
        _texture_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (auto const & [key, viz]: _mask_data_visualizations) {
            viz->renderBinaryImage(_texture_shader_program);
        }

        // Render selected masks with textures
        glDisable(GL_BLEND);// Solid color for selections
        for (auto const & [key, viz]: _mask_data_visualizations) {
            viz->renderSelectedMasks(_texture_shader_program);
        }

        _texture_shader_program->release();
    }

    // === DRAW CALL 2: Render mask bounding boxes ===
    auto lineProgram = ShaderManager::instance().getProgram("line");
    if (lineProgram && lineProgram->getNativeProgram()->bind()) {
        QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
        lineProgram->getNativeProgram()->setUniformValue("u_mvp_matrix", mvp_matrix);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Render hover mask bounding boxes (on top of everything else)
        for (auto const & [key, viz]: _mask_data_visualizations) {
            viz->renderHoverMaskUnionPolygon(lineProgram->getNativeProgram());
        }

        lineProgram->getNativeProgram()->release();
    }
}

void SpatialOverlayOpenGLWidget::renderLines() {
    if (_line_data_visualizations.empty() || !_opengl_resources_initialized) {
        return;
    }

    // Get line shader program from ShaderManager
    auto lineProgram = ShaderManager::instance().getProgram("line_with_geometry");
    if (!lineProgram || !lineProgram->getNativeProgram()->bind()) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to bind line shader program";
        return;
    }

    // Set uniform matrices
    QMatrix4x4 mvp_matrix = _projection_matrix * _view_matrix * _model_matrix;
    lineProgram->getNativeProgram()->setUniformValue("u_mvp_matrix", mvp_matrix);

    // Enable blending for regular lines
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render all lines for each LineData
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->renderLines(lineProgram->getNativeProgram(), 2.0f);// Default line width
    }

    lineProgram->getNativeProgram()->release();
}

void SpatialOverlayOpenGLWidget::initializeOpenGLResources() {
    qDebug() << "SpatialOverlayOpenGLWidget: Initializing OpenGL resources";

    // Load point shader program from ShaderManager
    if (!ShaderManager::instance().loadProgram("point", ":/shaders/point.vert", ":/shaders/point.frag", "", ShaderSourceType::Resource)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to load point shader program from ShaderManager";
        return;
    }

    // Load line shader program from ShaderManager
    if (!ShaderManager::instance().loadProgram("line", ":/shaders/line.vert", ":/shaders/line.frag", "", ShaderSourceType::Resource)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to load line shader program from ShaderManager";
        return;
    }

    // Create and compile texture shader program
    _texture_shader_program = new QOpenGLShaderProgram(this);

    // Vertex shader for texture rendering
    QString texture_vertex_shader_source = R"(
        #version 410 core
        
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texCoord;
        
        uniform mat4 u_mvp_matrix;
        
        out vec2 v_texCoord;
        
        void main() {
            gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
            v_texCoord = a_texCoord;
        }
    )";

    // Fragment shader for texture rendering
    QString texture_fragment_shader_source = R"(
        #version 410 core
        
        in vec2 v_texCoord;
        uniform sampler2D u_texture;
        uniform vec4 u_color;
        
        out vec4 FragColor;
        
        void main() {
            float intensity = texture(u_texture, v_texCoord).r;
            
            // Only render pixels where masks exist
            if (intensity <= 0.0) {
                discard;
            }
            
            // Proper density visualization with premultiplied alpha output
            vec3 final_color = u_color.rgb;  // Keep color pure (red for masks)
            float final_alpha = u_color.a * intensity;  // Density affects transparency
            
            // Output premultiplied alpha to work correctly with GL_ONE, GL_ONE_MINUS_SRC_ALPHA
            FragColor = vec4(final_color * final_alpha, final_alpha);
        }
    )";

    if (!_texture_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, texture_vertex_shader_source)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to compile texture vertex shader:" << _texture_shader_program->log();
        return;
    }

    if (!_texture_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, texture_fragment_shader_source)) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to compile texture fragment shader:" << _texture_shader_program->log();
        return;
    }

    if (!_texture_shader_program->link()) {
        qDebug() << "SpatialOverlayOpenGLWidget: Failed to link texture shader program:" << _texture_shader_program->log();
        return;
    }

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
    qDebug() << "SpatialOverlayOpenGLWidget: OpenGL resources initialized successfully";
}

void SpatialOverlayOpenGLWidget::cleanupOpenGLResources() {
    if (_opengl_resources_initialized) {
        makeCurrent();

        delete _texture_shader_program;
        _texture_shader_program = nullptr;

        _highlight_vertex_buffer.destroy();
        _highlight_vertex_array_object.destroy();

        // Clean up all PointData visualizations
        for (auto const & [key, viz]: _point_data_visualizations) {
            viz->cleanupOpenGLResources();
        }

        // Clean up all MaskData visualizations
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

QString create_tooltipText(QuadTreePoint<int64_t> const * point, QString const & data_key) {
    if (!point) return QString();

    return QString("Dataset: %1\nInterval: %2\nPosition: (%3, %4)")
            .arg(data_key)
            .arg(point->data)
            .arg(point->x, 0, 'f', 2)
            .arg(point->y, 0, 'f', 2);
}

void SpatialOverlayOpenGLWidget::processHoverDebounce() {
    if (!_data_bounds_valid || !_tooltips_enabled || _hover_processing_active) {
        // Skip processing if data is invalid, tooltips are disabled, or we're already processing
        return;
    }

    // Set processing flag to prevent new hover calculations
    _hover_processing_active = true;

    qDebug() << "SpatialOverlayOpenGLWidget: Processing debounced hover at" << _pending_hover_pos;

    // Find points, masks, and lines near the stored hover position
    auto [viz, point] = findPointNear(_pending_hover_pos.x(), _pending_hover_pos.y());
    auto mask_results = findMasksNear(_pending_hover_pos.x(), _pending_hover_pos.y());
    auto [line_viz, line_id] = findLineNear(_pending_hover_pos.x(), _pending_hover_pos.y());

    // Clear old hover states
    PointDataVisualization * old_hover_viz = getCurrentHoverVisualization();
    if (old_hover_viz && old_hover_viz != viz) {
        old_hover_viz->current_hover_point = nullptr;
    }

    // Clear old mask hover states
    for (auto const & [key, mask_viz]: _mask_data_visualizations) {
        mask_viz->clearHover();
    }

    // Clear old line hover states
    LineDataVisualization * old_line_hover_viz = getCurrentHoverLineVisualization();
    if (old_line_hover_viz && old_line_hover_viz != line_viz) {
        old_line_hover_viz->setHoverLine(std::nullopt);
    }

    // Set new hover state
    if (viz && point) {
        if (viz->current_hover_point != point) {
            viz->current_hover_point = point;
            _tooltip_timer->stop();
            _tooltip_refresh_timer->stop();
            _tooltip_timer->start();

            qDebug() << "SpatialOverlayOpenGLWidget: Hovering over point in" << viz->key
                     << "at" << point->x << "," << point->y << "frame:" << point->data;
        }
    } else if (!mask_results.empty()) {
        // Set mask hover states - this is where the expensive polygon union happens
        for (auto const & [mask_viz, entries]: mask_results) {
            mask_viz->setHoverEntries(entries);
        }

        _tooltip_timer->stop();
        _tooltip_refresh_timer->stop();
        _tooltip_timer->start();

        size_t total_masks = 0;
        for (auto const & [mask_viz, entries]: mask_results) {
            total_masks += entries.size();
        }
        qDebug() << "SpatialOverlayOpenGLWidget: Hovering over" << total_masks << "masks (processed after debounce)";
    } else if (line_viz && line_id.has_value()) {
        // Set line hover state
        line_viz->setHoverLine(line_id);
        _tooltip_timer->stop();
        _tooltip_refresh_timer->stop();
        _tooltip_timer->start();

        qDebug() << "SpatialOverlayOpenGLWidget: Hovering over line in" << line_viz->key
                 << "timeframe:" << line_id.value().time_frame << "line_id:"
                 << line_id.value().line_id;
    } else {
        // No point, mask, or line under cursor
        if (old_hover_viz) {
            old_hover_viz->current_hover_point = nullptr;
        }
        _tooltip_timer->stop();
        _tooltip_refresh_timer->stop();
        QToolTip::hideText();
    }

    requestThrottledUpdate();

    // Clear processing flag to allow new hover calculations
    _hover_processing_active = false;
}


void SpatialOverlayOpenGLWidget::updateMouseWorldPosition(int screen_x, int screen_y) {
    _current_mouse_world_pos = screenToWorld(screen_x, screen_y);
}

void SpatialOverlayOpenGLWidget::renderCommonOverlay() {
    // Render tooltips and other common overlay elements
    // This will be expanded to handle tooltip rendering and other common UI elements
}

