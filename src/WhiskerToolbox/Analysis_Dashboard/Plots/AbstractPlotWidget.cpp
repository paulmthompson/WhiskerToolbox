#include "AbstractPlotWidget.hpp"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>

AbstractPlotWidget::AbstractPlotWidget(QGraphicsItem * parent)
    : QGraphicsWidget(parent) {

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
    return QString::fromStdString(_parameters.getPlotTitle());
}

void AbstractPlotWidget::setPlotTitle(QString const & title) {
    std::string std_title = title.toStdString();
    if (_parameters.getPlotTitle() != std_title) {
        _parameters.setPlotTitle(std_title);
        emit propertiesChanged(QString::fromStdString(_parameters.getPlotId()));
        update();// Request repaint
    }
}

void AbstractPlotWidget::setDataSourceRegistry(DataSourceRegistry* data_source_registry) {
    qDebug() << "AbstractPlotWidget: setDataSourceRegistry called for plot" << QString::fromStdString(_parameters.getPlotId()) << "with registry:" << (data_source_registry != nullptr);
    _parameters.data_source_registry = data_source_registry;
}

void AbstractPlotWidget::setGroupManager(GroupManager * group_manager) {
    qDebug() << "AbstractPlotWidget: setGroupManager called for plot" << QString::fromStdString(_parameters.getPlotId()) << "with GroupManager:" << (group_manager != nullptr);
    _parameters.group_manager = group_manager;
}

QString AbstractPlotWidget::getPlotId() const {
    return QString::fromStdString(_parameters.getPlotId());
}

void AbstractPlotWidget::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    // Handle selection
    QString plot_id = QString::fromStdString(_parameters.getPlotId());
    qDebug() << "AbstractPlotWidget::mousePressEvent: Emitting plotSelected for plot_id:" << plot_id;
    emit plotSelected(plot_id);

    // Call parent implementation for standard behavior (dragging, etc.)
    QGraphicsWidget::mousePressEvent(event);
}

AbstractPlotParameters & AbstractPlotWidget::getParameters() {
    return _parameters;
}

AbstractPlotParameters const & AbstractPlotWidget::getParameters() const {
    return _parameters;
}
