#ifndef SCATTERPLOTPROPERTIESWIDGET_HPP
#define SCATTERPLOTPROPERTIESWIDGET_HPP

#include "Analysis_Dashboard/Properties/AbstractPlotPropertiesWidget.hpp"

#include <QStringList>
#include <QMap>

class DataSourceRegistry;
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

    void setDataSourceRegistry(DataSourceRegistry * data_source_registry) override;
    void setPlotWidget(AbstractPlotWidget * plot_widget) override;
    void updateFromPlot() override;
    void applyToPlot() override;

    /**
     * @brief Set the scatter plot widget directly (since it's no longer an AbstractPlotWidget)
     * @param scatter_widget The scatter plot widget
     */
    void setScatterPlotWidget(ScatterPlotWidget * scatter_widget);

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

    /**
     * @brief Update coordinate range display from current view
     */
    void updateCoordinateRange();

    /**
     * @brief Handle zoom level changes from OpenGL widget
     * @param zoom_level The new zoom level
     */
    void onZoomLevelChanged(float zoom_level);

    /**
     * @brief Handle pan offset changes from OpenGL widget
     * @param offset_x The new X pan offset
     * @param offset_y The new Y pan offset
     */
    void onPanOffsetChanged(float offset_x, float offset_y);

private:
    Ui::ScatterPlotPropertiesWidget * ui;
    ScatterPlotWidget * _scatter_plot_widget;
    DataSourceRegistry * _data_source_registry;
    bool _applying_properties;  // Flag to prevent signal emission during applyToPlot()
    
    // Data storage for coordinate range calculations
    std::vector<float> _x_data;
    std::vector<float> _y_data;

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
     * @brief Load data from a data key (either analog or table-based)
     * @param data_key The key identifying the data source
     * @return Vector of float values
     */
    std::vector<float> loadDataFromKey(QString const & data_key);

    /**
     * @brief Update the X-axis info label
     */
    void updateXAxisInfoLabel();

    /**
     * @brief Update the Y-axis info label
     */
    void updateYAxisInfoLabel();

    /**
     * @brief Get available numeric column keys that can be used for scatter plotting
     * @return QStringList of column keys in format "table:table_id:column_name" 
     */
    QStringList getAvailableNumericColumns() const;

    /**
     * @brief Get detailed information about available numeric columns
     * @return Map of column keys to their type information
     */
    QMap<QString, QString> getAvailableNumericColumnsWithTypes() const;

    /**
     * @brief Static utility function to get numeric columns from any DataSourceRegistry
     * @param data_source_registry Pointer to the data source registry
     * @return QStringList of numeric column keys suitable for plotting
     */
    static QStringList getNumericColumnsFromRegistry(DataSourceRegistry * data_source_registry);
    
};

#endif// SCATTERPLOTPROPERTIESWIDGET_HPP