#include "SpatialOverlayPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/TimeFrame/TimeFrame.hpp"
#include "SpatialOverlayOpenGLWidget.hpp"   


#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QTimer>

#include <cmath>

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

void SpatialOverlayPlotWidget::setDataKeys(QStringList const & point_data_keys,
                                           QStringList const & mask_data_keys,
                                           QStringList const & line_data_keys) {
    _point_data_keys = point_data_keys;
    _mask_data_keys = mask_data_keys;
    _line_data_keys = line_data_keys;
    updateVisualization();
}

void SpatialOverlayPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (isFrameAndTitleVisible()) {
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

void SpatialOverlayPlotWidget::keyPressEvent(QKeyEvent * event) {
    qDebug() << "SpatialOverlayPlotWidget::keyPressEvent - Key:" << event->key() << "Text:" << event->text();
    
    // Forward key events to the OpenGL widget using public handle method
    if (_opengl_widget) {
        qDebug() << "SpatialOverlayPlotWidget::keyPressEvent - Forwarding to OpenGL widget";
        _opengl_widget->handleKeyPress(event);
        qDebug() << "SpatialOverlayPlotWidget::keyPressEvent - Public handleKeyPress call completed";
        return; // Event was handled by OpenGL widget
    } else {
        qDebug() << "SpatialOverlayPlotWidget::keyPressEvent - No OpenGL widget available";
    }
    
    // If not handled by OpenGL widget, let parent handle it
    qDebug() << "SpatialOverlayPlotWidget::keyPressEvent - Calling parent implementation";
    AbstractPlotWidget::keyPressEvent(event);
}

void SpatialOverlayPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);

    if (_opengl_widget && _proxy_widget) {
        QRectF content_rect = isFrameAndTitleVisible()
            ? boundingRect().adjusted(2, 25, -2, -2)
            : boundingRect();
        _opengl_widget->resize(content_rect.size().toSize());
        _proxy_widget->setGeometry(content_rect);

        // Force update after resize
        _opengl_widget->update();
    }
}

void SpatialOverlayPlotWidget::updateVisualization() {

    if (_is_updating_visualization) {
        return;
    }

    if (!_parameters.data_manager || !_opengl_widget) {
        return;
    }

    _is_updating_visualization = true;

    loadPointData();
    loadMaskData();
    loadLineData();

    // Schedule a single coalesced render update for this tick
    scheduleRenderUpdate();
    _is_updating_visualization = false;
}

void SpatialOverlayPlotWidget::scheduleRenderUpdate() {
    if (_render_update_pending) {
        return;
    }
    _render_update_pending = true;
    // Use single-shot 0ms to coalesce multiple sources into one emission
    QTimer::singleShot(0, this, [this]() {
        _render_update_pending = false;
        update();
        emit renderUpdateRequested(getPlotId());
    });
}

void SpatialOverlayPlotWidget::handleFrameJumpRequest(EntityId entity_id, QString const & data_key) {

    //Get point data from entityID
    auto point_data = _parameters.data_manager->getData<PointData>(data_key.toStdString());
    if (point_data) {
        auto point = point_data->getTimeAndIndexByEntityId(entity_id);
        if (point) {
            emit frameJumpRequested(point->first.getValue(), data_key.toStdString());
        }
    }
}

void SpatialOverlayPlotWidget::loadPointData() {
    std::unordered_map<QString, std::shared_ptr<PointData>> point_data_map;

    for (QString const & key: _point_data_keys) {
        auto point_data = _parameters.data_manager->getData<PointData>(key.toStdString());
        if (point_data) {
            point_data_map[key] = point_data;
        }
    }
    // Always forward, even if empty, so the GL widget can clear its visualizations
    _opengl_widget->setPointData(point_data_map);
}

void SpatialOverlayPlotWidget::loadMaskData() {
    std::unordered_map<QString, std::shared_ptr<MaskData>> mask_data_map;

    for (QString const & key: _mask_data_keys) {
        auto mask_data = _parameters.data_manager->getData<MaskData>(key.toStdString());
        if (mask_data) {
            mask_data_map[key] = mask_data;
        }
    }
    // Always forward, even if empty, so the GL widget can clear its visualizations
    _opengl_widget->setMaskData(mask_data_map);
}

void SpatialOverlayPlotWidget::loadLineData() {
    std::unordered_map<QString, std::shared_ptr<LineData>> line_data_map;

    for (QString const & key: _line_data_keys) {
        auto line_data = _parameters.data_manager->getData<LineData>(key.toStdString());
        if (line_data) {
            line_data_map[key] = line_data;
        }
    }
    // Always forward, even if empty, so the GL widget can clear its visualizations
    _opengl_widget->setLineData(line_data_map);
}

void SpatialOverlayPlotWidget::setupOpenGLWidget() {
    _opengl_widget = new SpatialOverlayOpenGLWidget();
    
    _opengl_widget->setAttribute(Qt::WA_AlwaysStackOnTop, false);
    _opengl_widget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    _opengl_widget->setAttribute(Qt::WA_NoSystemBackground, true);
    
    // Ensure the widget is properly initialized
    _opengl_widget->setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    
    qDebug() << "SpatialOverlayPlotWidget: Created OpenGL widget with format:" << _opengl_widget->format().majorVersion() << "." << _opengl_widget->format().minorVersion();
    
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);

    // Configure the proxy widget to not interfere with parent interactions
    _proxy_widget->setFlag(QGraphicsItem::ItemIsMovable, false);
    _proxy_widget->setFlag(QGraphicsItem::ItemIsSelectable, false);
    
    // Set cache mode for better OpenGL rendering
    _proxy_widget->setCacheMode(QGraphicsItem::NoCache);

    // Set initial size and position
    QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
    _opengl_widget->resize(content_rect.size().toSize());
    _proxy_widget->setGeometry(content_rect);

    // Connect signals from the refactored base class
    connect(_opengl_widget, &BasePlotOpenGLWidget::viewBoundsChanged,
            this, [this](BoundingBox const& bounds) {
                Q_UNUSED(bounds)
                // Coalesce render updates across sources
                scheduleRenderUpdate();
            });

    connect(_opengl_widget, &BasePlotOpenGLWidget::highlightStateChanged,
            this, [this]() {
                scheduleRenderUpdate();
            });

    connect(_opengl_widget, &BasePlotOpenGLWidget::mouseWorldMoved,
            this, [this](float world_x, float world_y) {
                Q_UNUSED(world_x) Q_UNUSED(world_y)
                // Could emit world coordinates if needed by parent widgets
            });

    // Connect property change signals to trigger updates
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::pointSizeChanged,
            this, [this](float) {
                scheduleRenderUpdate();
                emit renderingPropertiesChanged();
            });

    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::tooltipsEnabledChanged,
            this, [this](bool) {
                emit renderingPropertiesChanged();
            });

    // Connect spatial overlay specific signals
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::frameJumpRequested,
            this, &SpatialOverlayPlotWidget::handleFrameJumpRequest);
}

void SpatialOverlayPlotWidget::setSelectionMode(SelectionMode mode) {
    if (_opengl_widget) {
        _opengl_widget->setSelectionMode(mode);
    }
}

SelectionMode SpatialOverlayPlotWidget::getSelectionMode() const {
    if (_opengl_widget) {
        return _opengl_widget->getSelectionMode();
    }
    return SelectionMode::None;
}

void SpatialOverlayPlotWidget::setGroupManager(GroupManager * group_manager) {
    // Call parent implementation
    AbstractPlotWidget::setGroupManager(group_manager);

    // Pass group manager to OpenGL widget
    if (_opengl_widget) {
        _opengl_widget->setGroupManager(group_manager);
    }
}

void SpatialOverlayPlotWidget::onGroupCreated(int group_id, QString const & name, QColor const & color) {
    Q_UNUSED(name)
    Q_UNUSED(color)
    if (_opengl_widget) {
        // Ask GL to refresh its per-point group render data based on current GroupManager state
        _opengl_widget->refreshGroupRenderDataAll();
    }
    // Coalesce scene-level updates as well
    scheduleRenderUpdate();
}

void SpatialOverlayPlotWidget::onGroupRemoved(int group_id) {
    Q_UNUSED(group_id)
    if (_opengl_widget) {
        // Ask GL to refresh its per-point group render data based on current GroupManager state
        _opengl_widget->refreshGroupRenderDataAll();
    }
    // Coalesce scene-level updates as well
    scheduleRenderUpdate();
}

void SpatialOverlayPlotWidget::onGroupPropertiesChanged(int group_id) {
    Q_UNUSED(group_id)
    if (_opengl_widget) {
        // Ask GL to refresh its per-point group render data based on current GroupManager state
        _opengl_widget->refreshGroupRenderDataAll();
    }
    // Coalesce scene-level updates as well
    scheduleRenderUpdate();
}