#include "PlotContainer.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/AbstractPlotPropertiesWidget.hpp"

PlotContainer::PlotContainer(std::unique_ptr<AbstractPlotWidget> plot_widget,
                            std::unique_ptr<AbstractPlotPropertiesWidget> properties_widget,
                            QObject* parent)
    : QObject(parent)
    , plot_widget_(std::move(plot_widget))
    , properties_widget_(std::move(properties_widget))
{
    // Ensure the plot widget has this container as parent for proper cleanup
    //if (plot_widget_) {
    //    plot_widget_->setParent(this);
    //}
    
    // Set properties widget configuration
    if (properties_widget_) {
        // Note: We don't set parent here because properties_widget_ is a QWidget
        // and this is a QObject. The unique_ptr ownership is sufficient.
        properties_widget_->setPlotWidget(plot_widget_.get());
    }
    
    connectInternalSignals();
}


QString PlotContainer::getPlotId() const
{
    return plot_widget_ ? plot_widget_->getPlotId() : QString();
}

QString PlotContainer::getPlotType() const
{
    return plot_widget_ ? plot_widget_->getPlotType() : QString();
}

void PlotContainer::configureManagers(std::shared_ptr<DataManager> data_manager,
                                     GroupManager* group_manager,
                                     TableManager* table_manager)
{
    qDebug() << "PlotContainer::configureManagers: Configuring managers for plot:" << getPlotId();
    qDebug() << "  - DataManager:" << (data_manager.get() != nullptr);
    qDebug() << "  - GroupManager:" << (group_manager != nullptr);
    qDebug() << "  - TableManager:" << (table_manager != nullptr);
    
    if (plot_widget_) {
        plot_widget_->setDataManager(data_manager);
        plot_widget_->setGroupManager(group_manager);
        plot_widget_->setTableManager(table_manager);
        qDebug() << "PlotContainer::configureManagers: Configured plot widget";
    } else {
        qDebug() << "PlotContainer::configureManagers: ERROR - null plot_widget_";
    }
    
    if (properties_widget_) {
        properties_widget_->setDataManager(data_manager);
        qDebug() << "PlotContainer::configureManagers: Configured properties widget";
    } else {
        qDebug() << "PlotContainer::configureManagers: ERROR - null properties_widget_";
    }
}

void PlotContainer::updatePropertiesFromPlot()
{
    if (properties_widget_) {
        properties_widget_->updateFromPlot();
    }
}

void PlotContainer::applyPropertiesToPlot()
{
    if (properties_widget_) {
        properties_widget_->applyToPlot();
    }
}

void PlotContainer::onPlotSelected(const QString& plot_id)
{
    qDebug() << "PlotContainer::onPlotSelected called with plot_id:" << plot_id;
    emit plotSelected(plot_id);
}

void PlotContainer::onPropertiesChanged()
{
    // Automatically apply properties when they change
    applyPropertiesToPlot();
    emit propertiesChanged(getPlotId());
}

void PlotContainer::onFrameJumpRequested(int64_t time_frame_index, const std::string& data_key)
{
    emit frameJumpRequested(time_frame_index, data_key);
}

void PlotContainer::connectInternalSignals()
{
    if (plot_widget_) {
        connect(plot_widget_.get(), &AbstractPlotWidget::plotSelected,
                this, &PlotContainer::onPlotSelected);
        connect(plot_widget_.get(), &AbstractPlotWidget::frameJumpRequested,
                this, &PlotContainer::onFrameJumpRequested);
    }
    
    if (properties_widget_) {
        connect(properties_widget_.get(), &AbstractPlotPropertiesWidget::propertiesChanged,
                this, &PlotContainer::onPropertiesChanged);
    }
}
