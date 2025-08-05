#ifndef EVENTPLOTPROPERTIESWIDGET_HPP
#define EVENTPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

#include <set>

class DataManager;
class DataSourceRegistry;
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

    void setDataSourceRegistry(DataSourceRegistry * data_source_registry) override;
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
     * @brief Handle table selection changes
     */
    void onTableSelectionChanged();

    /**
     * @brief Handle column selection changes
     */
    void onColumnSelectionChanged();

    /**
     * @brief Handle X-axis data source selection changes
     */
    void onXAxisDataSourceChanged();

    /**
     * @brief Handle interval beginning/end selection changes
     */
    void onIntervalSettingChanged();

    /**
     * @brief Handle Y-axis feature selection
     * @param feature The selected feature name
     */
    void onYAxisFeatureSelected(QString const & feature);

    /**
     * @brief Handle Y-axis feature addition
     * @param feature The feature to add
     */
    void onYAxisFeatureAdded(QString const & feature);

    /**
     * @brief Handle Y-axis feature removal
     * @param feature The feature to remove
     */
    void onYAxisFeatureRemoved(QString const & feature);

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
     * @brief Handle dark mode toggle changes
     * @param enabled Whether dark mode should be enabled
     */
    void onDarkModeToggled(bool enabled);

    /**
     * @brief Handle capture range changes
     * @param value New capture range value in samples
     */
    void onCaptureRangeChanged(int value);

    /**
     * @brief Handle view bounds update (when panning changes the visible area)
     */
    void onViewBoundsChanged();

    /**
     * @brief Get the selected table ID
     * @return Selected table ID, or empty string if none selected
     */
    QString getSelectedTableId() const;

    /**
     * @brief Set the selected table ID
     * @param table_id The table ID to select
     */
    void setSelectedTableId(const QString& table_id);

    /**
     * @brief Get the selected column name
     * @return Selected column name, or empty string if none selected
     */
    QString getSelectedColumnName() const;

    /**
     * @brief Set the selected column name
     * @param column_name The column name to select
     */
    void setSelectedColumnName(const QString& column_name);

    /**
     * @brief Update available tables from TableManager
     */
    void updateAvailableTables();

private:
    Ui::EventPlotPropertiesWidget * ui;
    EventPlotWidget * _event_plot_widget;
    std::shared_ptr<DataManager> _data_manager;
    DataSourceRegistry * _data_source_registry;
    std::set<QString> _selected_y_axis_features;
    bool _applying_properties;  // Flag to prevent signal emission during applyToPlot()

    /**
     * @brief Setup connections between UI elements and handlers
     */
    void setupConnections();

    /**
     * @brief Get currently selected X-axis data source
     * @return Selected X-axis data source key
     */
    QString getSelectedXAxisDataSource() const;

    /**
     * @brief Set which X-axis data source is selected
     * @param data_key The key to select
     */
    void setSelectedXAxisDataSource(QString const & data_key);

    /**
     * @brief Check if interval beginning is selected
     * @return True if beginning is selected, false if end is selected
     */
    bool isIntervalBeginningSelected() const;

    /**
     * @brief Update the plot widget with current settings
     */
    void updatePlotWidget();

    /**
     * @brief Setup the Y-axis feature table
     */
    void setupYAxisFeatureTable();

    /**
     * @brief Get currently selected Y-axis features
     * @return List of selected Y-axis feature keys
     */
    QStringList getSelectedYAxisFeatures() const;

    /**
     * @brief Update the X-axis info label
     */
    void updateXAxisInfoLabel();

    /**
     * @brief Update the visibility of interval settings based on selected data type
     */
    void updateIntervalSettingsVisibility();

    /**
     * @brief Get the capture range value in samples
     * @return Capture range value
     */
    int getCaptureRange() const;

    /**
     * @brief Set the capture range value in samples
     * @param value Capture range value
     */
    void setCaptureRange(int value);

    /**
     * @brief Update the view bounds labels based on current view
     */
    void updateViewBoundsLabels();

    void loadTableData(const QString& table_id, const QString& column_name);

    void updateAvailableColumns();
};

#endif// EVENTPLOTPROPERTIESWIDGET_HPP