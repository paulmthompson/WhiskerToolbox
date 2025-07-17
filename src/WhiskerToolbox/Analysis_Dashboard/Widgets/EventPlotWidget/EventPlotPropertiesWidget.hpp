#ifndef EVENTPLOTPROPERTIESWIDGET_HPP
#define EVENTPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

class DataManager;
class EventPlotWidget;

namespace Ui {
class EventPlotPropertiesWidget;
}

/**
 * @brief Properties widget for configuring EventPlot settings
 * 
 * This widget provides controls for:
 * - Selecting which event data sources to display
 * - Adjusting visualization parameters
 * - Managing zoom and pan settings
 */
class EventPlotPropertiesWidget : public AbstractPlotPropertiesWidget {
    Q_OBJECT

public:
    explicit EventPlotPropertiesWidget(QWidget * parent = nullptr);
    ~EventPlotPropertiesWidget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager) override;
    void setPlotWidget(AbstractPlotWidget * plot_widget) override;
    void updateFromPlot() override;
    void applyToPlot() override;

public slots:
    /**
     * @brief Update the available data sources
     */
    void updateAvailableDataSources();

private slots:
    /**
     * @brief Handle event data source selection changes
     */
    void onEventDataSourcesChanged();

    /**
     * @brief Handle zoom level changes
     * @param value New zoom level value
     */
    void onZoomLevelChanged(double value);

    /**
     * @brief Reset view to fit all data
     */
    void onResetViewClicked();

    /**
     * @brief Handle tooltip enable/disable changes
     * @param enabled Whether tooltips should be enabled
     */
    void onTooltipsEnabledChanged(bool enabled);

private:
    Ui::EventPlotPropertiesWidget * ui;
    EventPlotWidget * _event_plot_widget;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Setup connections between UI elements and handlers
     */
    void setupConnections();

    /**
     * @brief Get currently selected event data source keys
     * @return List of selected event data source keys
     */
    QStringList getSelectedEventDataSources() const;

    /**
     * @brief Set which event data sources are selected
     * @param selected_keys List of keys to select
     */
    void setSelectedEventDataSources(QStringList const & selected_keys);

    /**
     * @brief Update the plot widget with current settings
     */
    void updatePlotWidget();
};

#endif// EVENTPLOTPROPERTIESWIDGET_HPP