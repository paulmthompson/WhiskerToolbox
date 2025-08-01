#ifndef SCATTERPLOTPROPERTIESWIDGET_HPP
#define SCATTERPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

class DataManager;
class ScatterPlotWidget;

namespace Ui {
class ScatterPlotPropertiesWidget;
}

/**
 * @brief Properties widget for configuring ScatterPlot settings
 * 
 * This widget provides controls for:
 * - Selecting which data sources to display
 * - Adjusting visualization parameters
 * - Managing plot appearance settings
 */
class ScatterPlotPropertiesWidget : public AbstractPlotPropertiesWidget {
    Q_OBJECT

public:
    explicit ScatterPlotPropertiesWidget(QWidget * parent = nullptr);
    ~ScatterPlotPropertiesWidget() override;

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
     * @brief Handle Y-axis data source selection changes
     */
    void onYAxisDataSourceChanged();

    /**
     * @brief Handle point size changes
     * @param value New point size value
     */
    void onPointSizeChanged(double value);

    /**
     * @brief Handle point color changes
     */
    void onPointColorChanged();

    /**
     * @brief Handle show grid toggle changes
     * @param enabled Whether grid should be shown
     */
    void onShowGridToggled(bool enabled);

    /**
     * @brief Handle show legend toggle changes
     * @param enabled Whether legend should be shown
     */
    void onShowLegendToggled(bool enabled);

private:
    Ui::ScatterPlotPropertiesWidget * ui;
    ScatterPlotWidget * _scatter_plot_widget;
    std::shared_ptr<DataManager> _data_manager;

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
     * @brief Get currently selected Y-axis data source
     * @return Selected Y-axis data source key
     */
    QString getSelectedYAxisDataSource() const;

    /**
     * @brief Set which Y-axis data source is selected
     * @param data_key The key to select
     */
    void setSelectedYAxisDataSource(QString const & data_key);

    /**
     * @brief Update the plot widget with current settings
     */
    void updatePlotWidget();

    /**
     * @brief Update the X-axis info label
     */
    void updateXAxisInfoLabel();

    /**
     * @brief Update the Y-axis info label
     */
    void updateYAxisInfoLabel();
};

#endif// SCATTERPLOTPROPERTIESWIDGET_HPP