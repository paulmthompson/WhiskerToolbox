#include "EventPlotWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

EventPlotWidget::EventPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent),
      _opengl_widget(nullptr),
      _proxy_widget(nullptr) {
    setupOpenGLWidget();
}

QString EventPlotWidget::getPlotType() const {
    return "Event Plot";
}

void EventPlotWidget::setEventDataKeys(QStringList const & event_data_keys) {
    _event_data_keys = event_data_keys;
    loadEventData();
}

void EventPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    // The actual rendering is handled by the OpenGL widget
    // This method is called for the widget's background/border
    Q_UNUSED(painter)
    Q_UNUSED(option)
    Q_UNUSED(widget)
}

void EventPlotWidget::resizeEvent(QGraphicsSceneResizeEvent * event) {
    AbstractPlotWidget::resizeEvent(event);

    if (_proxy_widget) {
        _proxy_widget->setGeometry(rect());
    }
}

void EventPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
   

    // Emit selection signal
    emit plotSelected(getPlotId());

    AbstractPlotWidget::mousePressEvent(event);
}

void EventPlotWidget::updateVisualization() {
    if (_opengl_widget) {
        _opengl_widget->update();
    }
}

void EventPlotWidget::handleFrameJumpRequest(int64_t time_frame_index, QString const & data_key) {
    // Forward frame jump request to the main dashboard
    emit frameJumpRequested(time_frame_index, data_key.toStdString());
}

void EventPlotWidget::loadEventData() {
    // TODO: Implement event data loading from DataManager
    // This will be implemented in subsequent steps
}

void EventPlotWidget::setupOpenGLWidget() {
    // Create the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget();

    // Create proxy widget to embed OpenGL widget in graphics scene
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);
    _proxy_widget->setGeometry(rect());

    // Connect signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::frameJumpRequested,
            this, &EventPlotWidget::handleFrameJumpRequest);
}