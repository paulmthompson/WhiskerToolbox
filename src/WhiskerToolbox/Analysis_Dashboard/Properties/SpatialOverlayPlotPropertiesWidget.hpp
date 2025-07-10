#ifndef SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP
#define SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP

#include "AbstractPlotPropertiesWidget.hpp"

#include <QStringList>

class DataManager;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QGroupBox;
class SpatialOverlayPlotWidget;

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
    ~SpatialOverlayPlotPropertiesWidget() override = default;

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
     * @brief Select all data sources
     */
    void onSelectAllClicked();

    /**
     * @brief Deselect all data sources
     */
    void onDeselectAllClicked();

private:
    SpatialOverlayPlotWidget * _spatial_plot_widget;

    // Data source selection
    QGroupBox * _data_sources_group;
    QListWidget * _data_sources_list;
    QPushButton * _select_all_button;
    QPushButton * _deselect_all_button;

    // Visualization settings
    QGroupBox * _visualization_group;
    QDoubleSpinBox * _point_size_spinbox;
    QDoubleSpinBox * _zoom_level_spinbox;
    QPushButton * _reset_view_button;
    QCheckBox * _tooltips_checkbox;

    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Initialize the UI components
     */
    void initializeUI();

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
};

#endif// SPATIALOVERLAYPLOTPROPERTIESWIDGET_HPP