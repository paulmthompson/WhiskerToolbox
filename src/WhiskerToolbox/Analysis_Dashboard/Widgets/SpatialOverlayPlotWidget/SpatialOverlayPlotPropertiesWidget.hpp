#ifndef SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP
#define SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

class DataManager;
class SpatialOverlayPlotWidget;
class QListWidgetItem;

namespace Ui {
class SpatialOverlayPlotPropertiesWidget;
}

/**
 * @brief Properties widget for configuring SpatialOverlayPlot settings
 * 
 * This widget provides controls for:
 * - Selecting which PointData sources to display
 * - Adjusting visualization parameters
 * - Managing zoom and pan settings
 */
class SpatialOverlayPlotPropertiesWidget : public AbstractPlotPropertiesWidget {
    Q_OBJECT

public:
    explicit SpatialOverlayPlotPropertiesWidget(QWidget * parent = nullptr);
    ~SpatialOverlayPlotPropertiesWidget() override;

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
     * @brief Handle changes in data source selection
     * @param item The item that was changed
     */
    void onDataSourceItemChanged(QListWidgetItem * item);

    //=========== Point Data Visualization Settings ============

    /**
     * @brief Handle point size changes
     * @param value New point size value
     */
    void onPointSizeChanged(double value);

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

    /**
     * @brief Handle selection mode changes
     * @param index Index of selected mode in combo box
     */
    void onSelectionModeChanged(int index);

    /**
     * @brief Clear all selected points
     */
    void onClearSelectionClicked();

    /**
     * @brief Select all data sources
     */
    void onSelectAllClicked();

    /**
     * @brief Deselect all data sources
     */
    void onDeselectAllClicked();

private:
    Ui::SpatialOverlayPlotPropertiesWidget * ui;
    SpatialOverlayPlotWidget * _spatial_plot_widget;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Setup connections between UI elements and handlers
     */
    void setupConnections();

    /**
     * @brief Update the data sources list from DataManager
     */
    void refreshDataSourcesList();

    /**
     * @brief Get currently selected data source keys
     * @return List of selected data source keys
     */
    QStringList getSelectedDataSources() const;

    /**
     * @brief Set which data sources are selected
     * @param selected_keys List of keys to select
     */
    void setSelectedDataSources(QStringList const & selected_keys);

    /**
     * @brief Update the plot widget with current settings
     */
    void updatePlotWidget();

    /**
     * @brief Update the instruction text based on current selection mode
     */
    void updateSelectionInstructions();
};

#endif// SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP