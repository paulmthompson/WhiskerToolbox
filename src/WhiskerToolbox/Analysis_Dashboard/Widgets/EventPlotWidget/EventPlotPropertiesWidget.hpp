#ifndef EVENTPLOTPROPERTIESWIDGET_HPP
#define EVENTPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

#include <set>

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
     * @brief Handle negative range changes
     * @param value New negative range value in milliseconds
     */
    void onNegativeRangeChanged(int value);

    /**
     * @brief Handle positive range changes
     * @param value New positive range value in milliseconds
     */
    void onPositiveRangeChanged(int value);

private:
    Ui::EventPlotPropertiesWidget * ui;
    EventPlotWidget * _event_plot_widget;
    std::shared_ptr<DataManager> _data_manager;
    std::set<QString> _selected_y_axis_features;

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
     * @brief Get the negative range value in milliseconds
     * @return Negative range value
     */
    int getNegativeRange() const;

    /**
     * @brief Get the positive range value in milliseconds
     * @return Positive range value
     */
    int getPositiveRange() const;

    /**
     * @brief Set the negative range value in milliseconds
     * @param value Negative range value
     */
    void setNegativeRange(int value);

    /**
     * @brief Set the positive range value in milliseconds
     * @param value Positive range value
     */
    void setPositiveRange(int value);
};

#endif// EVENTPLOTPROPERTIESWIDGET_HPP