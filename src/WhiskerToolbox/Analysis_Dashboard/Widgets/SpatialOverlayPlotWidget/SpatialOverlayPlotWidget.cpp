#include "SpatialOverlayPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/TimeFrame.hpp"
#include "SpatialOverlayOpenGLWidget.hpp"


#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

#include <cmath>

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

void SpatialOverlayPlotWidget::handleFrameJumpRequest(int64_t time_frame_index, QString const & data_key) {
    qDebug() << "SpatialOverlayPlotWidget: Frame jump requested to frame:" 
             << time_frame_index << "for data key:" 
             << data_key;
    emit frameJumpRequested(time_frame_index, data_key.toStdString());
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
    
    // Connect selection change signals
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::selectionChanged,
            this, [this](size_t selected_count) {
                emit selectionChanged(selected_count);
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
            });
    
    // Connect selection mode change signals
    connect(_opengl_widget, &SpatialOverlayOpenGLWidget::selectionModeChanged,
            this, [this](SelectionMode mode) {
                emit selectionModeChanged(mode);
            });
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