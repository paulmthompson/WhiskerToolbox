#include "EventPlotWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include <QDebug>
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"

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

EventPlotWidget::~EventPlotWidget() {
    qDebug() << "EventPlotWidget::~EventPlotWidget destructor called";

    if (_table_view) {
        _table_view.reset();
    }
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
    if (!_data_manager || !_opengl_widget) {
        return;
    }

    loadEventData();
    
    // Request render update through signal (like SpatialOverlayPlotWidget)
    update();
    emit renderUpdateRequested(getPlotId());
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

    auto rowIntervalSeries = _data_manager->getData<DigitalIntervalSeries>(_event_data_keys[0].toStdString());
    if (!rowIntervalSeries) {
        qDebug() << "EventPlotWidget::loadEventData rowIntervalSeries is nullptr";
        return;
    }

    auto rowIntervals = rowIntervalSeries->getDigitalIntervalSeries();
    std::vector<TimeFrameInterval> timeFrameIntervals;
    for (auto const & interval : rowIntervals) {
        timeFrameIntervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
    }

    auto row_timeframe_key = _data_manager->getTimeFrame(_event_data_keys[0].toStdString());
    auto row_timeframe = _data_manager->getTime(row_timeframe_key);

    if (!row_timeframe) {
        qDebug() << "EventPlotWidget::loadEventData row_timeframe is nullptr";
        return;
    }

    auto rowSelector = std::make_unique<IntervalSelector>(timeFrameIntervals, row_timeframe);
        
    builder.setRowSelector(std::move(rowSelector));

    // Add y-axis data
    auto eventSource = dataManagerExtension->getEventSource(_y_axis_data_keys[0].toStdString());
    if (!eventSource) {
        qDebug() << "EventPlotWidget::loadEventData eventSource is nullptr";
        return;
    }

    builder.addColumn<std::vector<float>>(_y_axis_data_keys[0].toStdString(), 
        std::make_unique<EventInIntervalComputer<std::vector<float>>>(eventSource, 
            EventOperation::Gather_Center,
            _y_axis_data_keys[0].toStdString()));

    _table_view = std::make_unique<TableView>(builder.build());

    // Get the data from the table view
    if (_table_view) {
        auto event_data = _table_view->getColumnValues<std::vector<float>>(_y_axis_data_keys[0].toStdString());

        qDebug() << "EventPlotWidget::loadEventData event_data size: " << event_data.size();
        
        // Pass the event data to the OpenGL widget
        if (_opengl_widget) {
            _opengl_widget->setEventData(event_data);
            emit renderUpdateRequested(getPlotId());
        }
         
    } else {
        qDebug() << "EventPlotWidget::loadEventData _table_view is nullptr";
    }

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
    
    // Connect property change signals to trigger updates (like SpatialOverlayPlotWidget)
    connect(_opengl_widget, &EventPlotOpenGLWidget::zoomLevelChanged,
            this, [this](float) { 
                update(); // Trigger graphics item update
                emit renderUpdateRequested(getPlotId());
                emit renderingPropertiesChanged();
            });
    
    connect(_opengl_widget, &EventPlotOpenGLWidget::panOffsetChanged,
            this, [this](float, float) { 
                update(); // Trigger graphics item update
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
        emit renderUpdateRequested(getPlotId());
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