#include "AbstractPlotWidget.hpp"

#include "DataManager/DataManager.hpp"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>

// Static member initialization
int AbstractPlotWidget::_next_plot_id = 1;

AbstractPlotWidget::AbstractPlotWidget(QGraphicsItem* parent)
    : QGraphicsWidget(parent),
      _plot_title("Untitled Plot") {
    generateUniqueId();
    
    // Make the widget selectable, movable, and resizable
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
    
    // Enable resize handles
    setWindowFlags(Qt::Window);
    setWindowFrameMargins(4, 4, 4, 4);
    
    // Set default size
    setPreferredSize(200, 150);
    resize(200, 150);
}

QString AbstractPlotWidget::getPlotTitle() const {
    return _plot_title;
}

void AbstractPlotWidget::setPlotTitle(const QString& title) {
    if (_plot_title != title) {
        _plot_title = title;
        emit propertiesChanged(_plot_id);
        update(); // Request repaint
    }
}

void AbstractPlotWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    qDebug() << "AbstractPlotWidget: setDataManager called for plot" << _plot_id << "with DataManager:" << (data_manager != nullptr);
    _data_manager = std::move(data_manager);
}

void AbstractPlotWidget::setGroupManager(GroupManager* group_manager) {
    qDebug() << "AbstractPlotWidget: setGroupManager called for plot" << _plot_id << "with GroupManager:" << (group_manager != nullptr);
    _group_manager = group_manager;
}

QString AbstractPlotWidget::getPlotId() const {
    return _plot_id;
}

void AbstractPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    // Handle selection
    emit plotSelected(_plot_id);
    
    // Call parent implementation for standard behavior (dragging, etc.)
    QGraphicsWidget::mousePressEvent(event);
}

void AbstractPlotWidget::generateUniqueId() {
    _plot_id = QString("plot_%1").arg(_next_plot_id++);
} 

