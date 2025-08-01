#include "AnalysisDashboardScene.hpp"

#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/EventPlotWidget/EventPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"
#include "DataManager/DataManager.hpp"

#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <QDebug>

AnalysisDashboardScene::AnalysisDashboardScene(QObject* parent)
    : QGraphicsScene(parent) {
    
    // Set a large scene rect to allow for many plots
    setSceneRect(0, 0, 2000, 2000);
}

void AnalysisDashboardScene::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    
    // Update existing plot widgets with the data manager
    for (auto* plot : _plot_widgets.values()) {
        if (plot) {
            plot->setDataManager(_data_manager);
        }
    }
}

void AnalysisDashboardScene::setGroupManager(GroupManager* group_manager) {
    _group_manager = group_manager;
    
    // Update existing plot widgets with the group manager
    for (auto* plot : _plot_widgets.values()) {
        if (plot) {
            plot->setGroupManager(_group_manager);
        }
    }
}

void AnalysisDashboardScene::setTableManager(TableManager* table_manager) {
    _table_manager = table_manager;
    
    // Update existing plot widgets with the table manager
    for (auto* plot : _plot_widgets.values()) {
        if (plot) {
            plot->setTableManager(_table_manager);
        }
    }
}

void AnalysisDashboardScene::addPlotWidget(AbstractPlotWidget* plot_widget, const QPointF& position) {
    if (!plot_widget) {
        return;
    }
    
    // Set data manager if available
    if (_data_manager) {
        plot_widget->setDataManager(_data_manager);
    }
    
    // Set group manager if available
    if (_group_manager) {
        plot_widget->setGroupManager(_group_manager);
    }
    
    // Set table manager if available
    if (_table_manager) {
        plot_widget->setTableManager(_table_manager);
    }
    
    // Set group manager if available
    if (_group_manager) {
        plot_widget->setGroupManager(_group_manager);
    }
    
    // Connect signals
    connect(plot_widget, &AbstractPlotWidget::plotSelected,
            this, &AnalysisDashboardScene::handlePlotSelected);
    
    connect(plot_widget, &AbstractPlotWidget::renderUpdateRequested,
            this, &AnalysisDashboardScene::handleRenderUpdateRequested);

    connect(plot_widget, &AbstractPlotWidget::frameJumpRequested,
            this, &AnalysisDashboardScene::frameJumpRequested);
    
    // Add to scene and position
    addItem(plot_widget);
    plot_widget->setPos(position);
    
    // Store in map
    QString const plot_id = plot_widget->getPlotId();
    _plot_widgets[plot_id] = plot_widget;
    
    emit plotAdded(plot_id);
    
    qDebug() << "Added plot" << plot_id << "at position" << position;
}

void AnalysisDashboardScene::removePlotWidget(const QString& plot_id) {
    auto it = _plot_widgets.find(plot_id);
    if (it != _plot_widgets.end()) {
        AbstractPlotWidget* plot = it.value();

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

AbstractPlotWidget* AnalysisDashboardScene::getPlotWidget(const QString& plot_id) const {
    auto it = _plot_widgets.find(plot_id);
    return (it != _plot_widgets.end()) ? it.value() : nullptr;
}

QMap<QString, AbstractPlotWidget*> AnalysisDashboardScene::getAllPlotWidgets() const {
    return _plot_widgets;
}

void AnalysisDashboardScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event) {
    // Accept drag if it contains plot type information
    if (event->mimeData()->hasFormat("application/x-plottype")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AnalysisDashboardScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event) {
    // Continue accepting the drag if it has the right format
    if (event->mimeData()->hasFormat("application/x-plottype")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AnalysisDashboardScene::dropEvent(QGraphicsSceneDragDropEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-plottype")) {
        event->ignore();
        return;
    }
    
    // Create a new plot widget from the dropped data
    AbstractPlotWidget* new_plot = createPlotFromMimeData(event->mimeData());
    if (new_plot) {
        // Add the plot at the drop position
        addPlotWidget(new_plot, event->scenePos());
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AnalysisDashboardScene::handlePlotSelected(const QString& plot_id) {
    // Clear selection of other items
    clearSelection();
    
    // Select the clicked plot
    AbstractPlotWidget* plot = getPlotWidget(plot_id);
    if (plot) {
        plot->setSelected(true);
    }
    
    emit plotSelected(plot_id);
}

void AnalysisDashboardScene::handleRenderUpdateRequested(const QString& plot_id) {
    qDebug() << "AnalysisDashboardScene: Received render update request from plot:" << plot_id;
    
    // Update the scene to ensure proper rendering
    update();
    
    // Optionally update only the specific plot's area for better performance
    AbstractPlotWidget* plot = getPlotWidget(plot_id);
    if (plot) {
        update(plot->sceneBoundingRect());
        qDebug() << "AnalysisDashboardScene: Updated scene area for plot:" << plot_id;
    }
}

AbstractPlotWidget* AnalysisDashboardScene::createPlotFromMimeData(const QMimeData* mime_data) {
    if (!mime_data->hasFormat("application/x-plottype")) {
        return nullptr;
    }
    
    QString const plot_type = QString::fromUtf8(mime_data->data("application/x-plottype"));
    
    // Factory pattern for creating different plot types
    if (plot_type == "scatter_plot") {
        return new ScatterPlotWidget();
    }
    else if (plot_type == "spatial_overlay_plot") {
        qDebug() << "Creating spatial overlay plot widget";
        return new SpatialOverlayPlotWidget();
    }
    else if (plot_type == "event_plot") {
        qDebug() << "Creating event plot widget";
        return new EventPlotWidget();
    }
    
    // Add more plot types here as they are implemented
    // else if (plot_type == "line_plot") {
    //     return new LinePlotWidget();
    // }
    
    qDebug() << "Unknown plot type:" << plot_type;
    return nullptr;
} 

