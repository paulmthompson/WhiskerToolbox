#include "PlotFactory.hpp"
#include "PlotContainer.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/AbstractPlotPropertiesWidget.hpp"
//#include "Widgets/EventPlotWidget/EventPlotPropertiesWidget.hpp"
//#include "Widgets/EventPlotWidget/EventPlotWidget.hpp"
#include "Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.hpp"
#include "Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp"
#include "Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp"
#include "Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"

#include <QDebug>

std::unique_ptr<PlotContainer> PlotFactory::createPlotContainer(QString const & plot_type) {
    auto plot_widget = createPlotWidget(plot_type);
    auto properties_widget = createPropertiesWidget(plot_type);

    if (!plot_widget || !properties_widget) {
        qDebug() << "Failed to create plot or properties widget for type:" << plot_type;
        return nullptr;
    }

    return std::make_unique<PlotContainer>(std::move(plot_widget), std::move(properties_widget));
}

std::unique_ptr<AbstractPlotWidget> PlotFactory::createPlotWidget(QString const & plot_type) {
    if (plot_type == "scatter_plot") {
        return std::make_unique<ScatterPlotWidget>();
    } else if (plot_type == "spatial_overlay_plot") {
        qDebug() << "Creating spatial overlay plot widget";
        return std::make_unique<SpatialOverlayPlotWidget>();
    //} else if (plot_type == "event_plot") {
    //    qDebug() << "Creating event plot widget";
    //    return std::make_unique<EventPlotWidget>();
    }

    // Add more plot types here as they are implemented
    // else if (plot_type == "line_plot") {
    //     return std::make_unique<LinePlotWidget>();
    // }

    qDebug() << "Unknown plot type:" << plot_type;
    return nullptr;
}

std::unique_ptr<AbstractPlotPropertiesWidget> PlotFactory::createPropertiesWidget(QString const & plot_type) {
    if (plot_type == "scatter_plot") {
        return std::make_unique<ScatterPlotPropertiesWidget>();
    } else if (plot_type == "spatial_overlay_plot") {
        qDebug() << "Creating spatial overlay plot properties widget";
        return std::make_unique<SpatialOverlayPlotPropertiesWidget>();
    //} else if (plot_type == "event_plot") {
        //qDebug() << "Creating event plot properties widget";
        //return std::make_unique<EventPlotPropertiesWidget>();
    }

    // Add more plot types here as they are implemented
    // else if (plot_type == "line_plot") {
    //     return std::make_unique<LinePlotPropertiesWidget>();
    // }

    qDebug() << "Unknown plot properties type:" << plot_type;
    return nullptr;
}
