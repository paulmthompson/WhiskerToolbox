#ifndef ABSTRACTPLOTPROPERTIESWIDGET_HPP
#define ABSTRACTPLOTPROPERTIESWIDGET_HPP

#include <QWidget>

class AbstractPlotWidget;
class DataManager;
class DataSourceRegistry;

/**
 * @brief Abstract base class for plot-specific properties widgets
 * 
 * This class provides the interface for properties widgets that configure
 * specific plot types in the Analysis Dashboard.
 */
class AbstractPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit AbstractPlotPropertiesWidget(QWidget * parent = nullptr);
    ~AbstractPlotPropertiesWidget() override = default;

    /**
     * @brief Set the data manager that this properties widget uses
     * @param data_manager Pointer to the data manager
     */
    virtual void setDataManager(std::shared_ptr<DataManager> data_manager) = 0;

    /**
     * @brief Set the data source registry for unified data access
     * @param data_source_registry Pointer to the data source registry
     */
    virtual void setDataSourceRegistry(DataSourceRegistry * data_source_registry) = 0;

    /**
     * @brief Set the plot widget that this properties widget configures
     * @param plot_widget Pointer to the plot widget
     */
    virtual void setPlotWidget(AbstractPlotWidget * plot_widget) = 0;

    /**
     * @brief Update the properties display from the current plot widget
     */
    virtual void updateFromPlot() = 0;

    /**
     * @brief Apply the current properties to the plot widget
     */
    virtual void applyToPlot() = 0;

signals:
    /**
     * @brief Emitted when properties are changed and should be applied
     */
    void propertiesChanged();

protected:
    AbstractPlotWidget * _plot_widget;
    DataSourceRegistry * _data_source_registry;
};

#endif// ABSTRACTPLOTPROPERTIESWIDGET_HPP