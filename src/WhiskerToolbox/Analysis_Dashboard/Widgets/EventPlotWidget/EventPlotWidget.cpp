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
    qDebug() << "EventPlotWidget::EventPlotWidget constructor called";
    setPlotTitle("Event Plot");
    setupOpenGLWidget();
    qDebug() << "EventPlotWidget::EventPlotWidget constructor done";
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

    if (_proxy_widget) {
        _proxy_widget->setGeometry(rect());
    }
}

void EventPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    qDebug() << "EventPlotWidget::mousePressEvent";

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
    _opengl_widget = new EventPlotOpenGLWidget();
    _proxy_widget = new QGraphicsProxyWidget(this);
    _proxy_widget->setWidget(_opengl_widget);

    _proxy_widget->setFlag(QGraphicsItem::ItemIsMovable, false);
    _proxy_widget->setFlag(QGraphicsItem::ItemIsSelectable, false);

    QRectF content_rect = boundingRect().adjusted(2, 25, -2, -2);
    _opengl_widget->resize(content_rect.size().toSize());
    _proxy_widget->setGeometry(content_rect);

    // Connect signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::frameJumpRequested,
            this, &EventPlotWidget::handleFrameJumpRequest);
}