#include "AnalysisDashboardScene.hpp"

#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/EventPlotWidget/EventPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"
#include "DataManager/DataManager.hpp"

#include <QDebug>

AnalysisDashboardScene::AnalysisDashboardScene(QObject * parent)
    : QGraphicsScene(parent) {

    // Set a fixed scene rect that won't change during resizing
    // This ensures plot positions remain stable
    setSceneRect(0, 0, 1000, 800);
}

void AnalysisDashboardScene::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;

    // Update existing plot widgets with the group manager
    for (auto * plot: _plot_widgets.values()) {
        if (plot) {
            plot->setGroupManager(_group_manager);
        }
    }
}

void AnalysisDashboardScene::setTableManager(TableManager * table_manager) {
    _table_manager = table_manager;

    // Update existing plot widgets with the table manager
    for (auto * plot: _plot_widgets.values()) {
        if (plot) {
            plot->setTableManager(_table_manager);
        }
    }
}

void AnalysisDashboardScene::addPlotWidget(AbstractPlotWidget * plot_widget, QPointF const & position) {
    if (!plot_widget) {
        return;
    }

    // Set group manager if available
    if (_group_manager) {
        plot_widget->setGroupManager(_group_manager);
    }

    // Set table manager if available
    if (_table_manager) {
        plot_widget->setTableManager(_table_manager);
    }

    // Connect signals
    connect(plot_widget, &AbstractPlotWidget::plotSelected,
            this, &AnalysisDashboardScene::handlePlotSelected);

    connect(plot_widget, &AbstractPlotWidget::renderUpdateRequested,
            this, &AnalysisDashboardScene::handleRenderUpdateRequested);

    connect(plot_widget, &AbstractPlotWidget::frameJumpRequested,
            this, &AnalysisDashboardScene::frameJumpRequested);

    // Add to scene
    addItem(plot_widget);

    // Position the plot - if position is (0,0) or invalid, center it in the scene
    QPointF plot_position = position;
    if (position.isNull() || (position.x() == 0 && position.y() == 0)) {
        // Center the plot in the scene
        QRectF scene_rect = sceneRect();
        QRectF plot_bounds = plot_widget->boundingRect();
        
        // Calculate center position, ensuring the plot stays within scene bounds
        qreal center_x = scene_rect.center().x() - plot_bounds.width() / 2;
        qreal center_y = scene_rect.center().y() - plot_bounds.height() / 2;
        
        // Clamp to scene bounds
        center_x = qMax(scene_rect.left(), qMin(center_x, scene_rect.right() - plot_bounds.width()));
        center_y = qMax(scene_rect.top(), qMin(center_y, scene_rect.bottom() - plot_bounds.height()));
        
        plot_position = QPointF(center_x, center_y);
    }

    plot_widget->setPos(plot_position);

    // Store in map
    QString plot_id = plot_widget->getPlotId();
    _plot_widgets[plot_id] = plot_widget;

    emit plotAdded(plot_id);

    qDebug() << "Added plot" << plot_id << "at position" << plot_position;
}

void AnalysisDashboardScene::removePlotWidget(QString const & plot_id) {
    auto it = _plot_widgets.find(plot_id);
    if (it != _plot_widgets.end()) {
        AbstractPlotWidget * plot = it.value();

        disconnect(plot, &AbstractPlotWidget::plotSelected,
                   this, &AnalysisDashboardScene::handlePlotSelected);
        disconnect(plot, &AbstractPlotWidget::renderUpdateRequested,
                   this, &AnalysisDashboardScene::handleRenderUpdateRequested);
        disconnect(plot, &AbstractPlotWidget::frameJumpRequested,
                   this, &AnalysisDashboardScene::frameJumpRequested);

        _plot_widgets.erase(it);

        // Remove from scene
        removeItem(plot);
        plot->deleteLater();

        emit plotRemoved(plot_id);

        qDebug() << "Removed plot" << plot_id;
    }
}

AbstractPlotWidget * AnalysisDashboardScene::getPlotWidget(QString const & plot_id) const {
    auto it = _plot_widgets.find(plot_id);
    return (it != _plot_widgets.end()) ? it.value() : nullptr;
}

QMap<QString, AbstractPlotWidget *> AnalysisDashboardScene::getAllPlotWidgets() const {
    return _plot_widgets;
}

void AnalysisDashboardScene::ensurePlotsVisible() {
    QRectF scene_rect = sceneRect();
    
    for (auto * plot : _plot_widgets.values()) {
        if (plot) {
            QRectF plot_bounds = plot->boundingRect();
            QPointF plot_pos = plot->pos();
            
            // Check if plot is outside scene bounds
            if (plot_pos.x() < scene_rect.left() || 
                plot_pos.x() + plot_bounds.width() > scene_rect.right() ||
                plot_pos.y() < scene_rect.top() || 
                plot_pos.y() + plot_bounds.height() > scene_rect.bottom()) {
                
                // Move plot to center of scene
                qreal center_x = scene_rect.center().x() - plot_bounds.width() / 2;
                qreal center_y = scene_rect.center().y() - plot_bounds.height() / 2;
                
                // Clamp to scene bounds
                center_x = qMax(scene_rect.left(), qMin(center_x, scene_rect.right() - plot_bounds.width()));
                center_y = qMax(scene_rect.top(), qMin(center_y, scene_rect.bottom() - plot_bounds.height()));
                
                plot->setPos(QPointF(center_x, center_y));
            }
        }
    }
}

void AnalysisDashboardScene::handlePlotSelected(QString const & plot_id) {
    // Clear selection of other items
    clearSelection();

    // Select the clicked plot
    AbstractPlotWidget * plot = getPlotWidget(plot_id);
    if (plot) {
        plot->setSelected(true);
    }

    emit plotSelected(plot_id);
}

void AnalysisDashboardScene::handleRenderUpdateRequested(QString const & plot_id) {
    qDebug() << "AnalysisDashboardScene: Received render update request from plot:" << plot_id;

    // Update the scene to ensure proper rendering
    update();

    // Optionally update only the specific plot's area for better performance
    AbstractPlotWidget * plot = getPlotWidget(plot_id);
    if (plot) {
        update(plot->sceneBoundingRect());
        qDebug() << "AnalysisDashboardScene: Updated scene area for plot:" << plot_id;
    }
}
