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
 * ## Incremental Migration
 * 
 * The properties panel will be incrementally populated with controls currently
 * in the left panel of DataViewer_Widget. Initially it starts empty, with plans to add:
 * - Feature tree/selection controls
 * - Theme and grid settings
 * - Series-specific options (color, scale, visibility)
 * - Export controls
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
 * 
 * Initially empty - will be incrementally populated with controls
 * migrated from the DataViewer_Widget left panel.
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

signals:
    // Future: Add signals for property changes that need to be communicated to the view
    // void themeChanged();
    // void gridSettingsChanged();
    // void seriesOptionsChanged(QString const & series_key);

private:
    Ui::DataViewerPropertiesWidget * ui;
    std::shared_ptr<DataViewerState> _state;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Connect to state signals for UI updates
     */
    void _connectStateSignals();
};

#endif  // DATA_VIEWER_PROPERTIES_WIDGET_HPP
