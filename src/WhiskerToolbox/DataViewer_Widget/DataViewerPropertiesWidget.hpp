#ifndef DATA_VIEWER_PROPERTIES_WIDGET_HPP
#define DATA_VIEWER_PROPERTIES_WIDGET_HPP

/**
 * @file DataViewerPropertiesWidget.hpp
 * @brief Properties panel for the Data Viewer Widget
 * 
 * DataViewerPropertiesWidget is the properties/inspector panel for DataViewer_Widget.
 * It displays controls for managing displayed features and their options.
 * 
 * ## Architecture
 * 
 * The Data Viewer Widget follows a View + Properties split:
 * - **DataViewer_Widget** (View): Contains the OpenGL canvas and visualization
 * - **DataViewerPropertiesWidget** (Properties): Contains controls for configuring
 *   the visualization, series options, theme settings, etc.
 * 
 * Both widgets share the same DataViewerState for coordination.
 * 
 * ## Migrated Controls
 * 
 * The following controls have been migrated from DataViewer_Widget:
 * - Theme selection (Dark/Purple/Light)
 * - Global Y Scale
 * - X Axis Samples
 * - Grid Lines enabled
 * - Grid Spacing
 * - Auto-Arrange button
 * 
 * @see DataViewer_Widget for the view component
 * @see DataViewerState for shared state
 * @see DataViewerWidgetRegistration for factory registration
 */

#include <QWidget>

#include <memory>

class DataManager;
class DataViewerState;

namespace Ui {
class DataViewerPropertiesWidget;
}

/**
 * @brief Properties panel for Data Viewer Widget
 * 
 * Displays controls for configuring the data visualization.
 * Shares state with DataViewer_Widget (view) via DataViewerState.
 */
class DataViewerPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a DataViewerPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit DataViewerPropertiesWidget(std::shared_ptr<DataViewerState> state,
                                         std::shared_ptr<DataManager> data_manager,
                                         QWidget * parent = nullptr);

    ~DataViewerPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to DataViewerState
     */
    [[nodiscard]] std::shared_ptr<DataViewerState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the maximum value for the X axis samples spinbox
     * 
     * Called by the view widget to set the maximum based on data range.
     * 
     * @param max Maximum number of samples
     */
    void setXAxisSamplesMaximum(int max);

signals:
    /**
     * @brief Emitted when the auto-arrange button is clicked
     * 
     * The view widget should connect to this signal to trigger
     * auto-arrangement of all visible series.
     */
    void autoArrangeRequested();

    /**
     * @brief Emitted when the export SVG button is clicked
     * 
     * The view widget should connect to this signal to trigger SVG export.
     * 
     * @param includeScalebar Whether to include a scalebar in the export
     * @param scalebarLength Length of the scalebar in time units
     */
    void exportSVGRequested(bool includeScalebar, int scalebarLength);

private slots:
    void _onThemeChanged(int index);
    void _onGlobalZoomChanged(double value);
    void _onXAxisSamplesChanged(int value);
    void _onGridLinesToggled(bool enabled);
    void _onGridSpacingChanged(int value);

private:
    Ui::DataViewerPropertiesWidget * ui;
    std::shared_ptr<DataViewerState> _state;
    std::shared_ptr<DataManager> _data_manager;
    
    /// Guard to prevent signal loops during programmatic UI updates
    bool _updating_from_state = false;

    /**
     * @brief Connect UI controls to state
     */
    void _connectUIControls();
    
    /**
     * @brief Connect to state signals for UI updates
     */
    void _connectStateSignals();
    
    /**
     * @brief Initialize UI values from current state
     */
    void _initializeFromState();
};

#endif  // DATA_VIEWER_PROPERTIES_WIDGET_HPP
