#include "EventPlotWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>



EventPlotWidget::EventPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent),
      _opengl_widget(nullptr),
      _proxy_widget(nullptr),
      _table_view(nullptr) {
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
    if (_event_data_keys.size() > 0 && _y_axis_data_keys.size() > 0) {
        loadEventData();
    }
}

void EventPlotWidget::setYAxisDataKeys(QStringList const & y_axis_data_keys) {
    _y_axis_data_keys = y_axis_data_keys;
    if (_event_data_keys.size() > 0 && _y_axis_data_keys.size() > 0) {
        loadEventData();
    }
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
    qDebug() << "EventPlotWidget::loadEventData";
    qDebug() << "EventPlotWidget::loadEventData _event_data_keys: " << _event_data_keys;
    qDebug() << "EventPlotWidget::loadEventData _y_axis_data_keys: " << _y_axis_data_keys;

    // We will use builder to create a table view


    auto dataManagerExtension = std::make_shared<DataManagerExtension>(*_data_manager);
    TableViewBuilder builder(dataManagerExtension);


    qDebug() << "EventPlotWidget::loadEventData done";
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

void EventPlotWidget::setXAxisRange(int negative_range, int positive_range) {
    if (_opengl_widget) {
        _opengl_widget->setXAxisRange(negative_range, positive_range);
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