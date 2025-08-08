#include "EventPlotWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include <QDebug>

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimer>


EventPlotWidget::EventPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent),
      _opengl_widget(nullptr),
      _proxy_widget(nullptr) {
    qDebug() << "EventPlotWidget::EventPlotWidget constructor called";

    setPlotTitle("Event Plot");
    setupOpenGLWidget();

    qDebug() << "EventPlotWidget::EventPlotWidget constructor done";
}

EventPlotWidget::~EventPlotWidget() {
    qDebug() << "EventPlotWidget::~EventPlotWidget destructor called";
}

QString EventPlotWidget::getPlotType() const {
    return "Event Plot";
}

void EventPlotWidget::setEventDataKeys(QStringList const & event_data_keys) {
    _event_data_keys = event_data_keys;
    // Note: Legacy method - data loading now handled by properties widget
}

void EventPlotWidget::setYAxisDataKeys(QStringList const & y_axis_data_keys) {
    _y_axis_data_keys = y_axis_data_keys;
    // Note: Legacy method - data loading now handled by properties widget
}

void EventPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    // The actual rendering is handled by the OpenGL widget
    // This method is called for the widget's background/border
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

void EventPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);

    if (_opengl_widget && _proxy_widget) {
        QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
        _opengl_widget->resize(content_rect.size().toSize());
        _proxy_widget->setGeometry(content_rect);

        // Force update after resize
        _opengl_widget->update();
    }
}

void EventPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
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

void EventPlotWidget::updateVisualization() {
    if (!_opengl_widget) {
        return;
    }

    // Note: Data loading is now handled by the properties widget
    // This method just triggers a visual update
    update();
    emit renderUpdateRequested(getPlotId());
}

void EventPlotWidget::handleFrameJumpRequest(int64_t time_frame_index, QString const & data_key) {
    // Forward frame jump request to the main dashboard
    emit frameJumpRequested(time_frame_index, data_key.toStdString());
}

void EventPlotWidget::setupOpenGLWidget() {
    _opengl_widget = new EventPlotOpenGLWidget();
    
    // Block signals during setup to prevent premature signal emissions
    _opengl_widget->blockSignals(true);
    
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);

    _proxy_widget->setFlag(QGraphicsItem::ItemIsMovable, false);
    _proxy_widget->setFlag(QGraphicsItem::ItemIsSelectable, false);

    QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
    _opengl_widget->resize(content_rect.size().toSize());
    _proxy_widget->setGeometry(content_rect);

    // Connect signals immediately, but OpenGL widget signals are blocked
    connectOpenGLSignals();
    
    // Defer unblocking signals until after OpenGL widget is fully initialized
    // Use QTimer::singleShot to delay until next event loop iteration
    QTimer::singleShot(0, this, [this]() {
        if (_opengl_widget) {
            _opengl_widget->blockSignals(false);
        }
    });
}

void EventPlotWidget::connectOpenGLSignals() {
    if (!_opengl_widget) {
        return;
    }

    // Connect signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::frameJumpRequested,
            this, &EventPlotWidget::handleFrameJumpRequest);

    // Connect property change signals to trigger updates (like SpatialOverlayPlotWidget)
    connect(_opengl_widget, &EventPlotOpenGLWidget::zoomLevelChanged,
            this, [this](float) {
                update();// Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });

    connect(_opengl_widget, &EventPlotOpenGLWidget::panOffsetChanged,
            this, [this](float, float) {
                update();// Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });

    connect(_opengl_widget, &EventPlotOpenGLWidget::tooltipsEnabledChanged,
            this, [this](bool) {
                emit renderingPropertiesChanged();
            });
}

void EventPlotWidget::setXAxisRange(int negative_range, int positive_range) {
    if (_opengl_widget) {
        _opengl_widget->setXAxisRange(negative_range, positive_range);
        //emit renderUpdateRequested(getPlotId());
    }
}

void EventPlotWidget::getXAxisRange(int & negative_range, int & positive_range) const {
    if (_opengl_widget) {
        _opengl_widget->getXAxisRange(negative_range, positive_range);
    } else {
        negative_range = 30000;
        positive_range = 30000;
    }
}

void EventPlotWidget::setYZoomLevel(float y_zoom_level) {
    if (_opengl_widget) {
        _opengl_widget->setYZoomLevel(y_zoom_level);
    }
}

float EventPlotWidget::getYZoomLevel() const {
    if (_opengl_widget) {
        return _opengl_widget->getYZoomLevel();
    } else {
        return 1.0f;
    }
}