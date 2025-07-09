#include "AbstractPlotWidget.hpp"

#include "DataManager/DataManager.hpp"

#include <QGraphicsSceneMouseEvent>

// Static member initialization
int AbstractPlotWidget::_next_plot_id = 1;

AbstractPlotWidget::AbstractPlotWidget(QGraphicsItem* parent)
    : QGraphicsWidget(parent),
      _plot_title("Untitled Plot") {
    generateUniqueId();
    
    // Make the widget selectable and movable
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
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
    _data_manager = std::move(data_manager);
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