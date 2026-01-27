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
 * - **DataViewerPropertiesWidget** (Properties): Contains feature selection tree,
 *   per-feature controls, theme settings, grid settings, and export options.
 * 
 * Both widgets share the same DataViewerState for coordination. The properties
 * widget receives a pointer to OpenGLWidget for feature control widgets that
 * need to modify display options directly.
 * 
 * ## Features
 * 
 * - Feature Tree: Browse and select data series to display
 * - Feature Controls: Per-type settings (AnalogViewer_Widget, etc.)
 * - Display Settings: Theme, global zoom, X axis samples
 * - Grid Settings: Enable/spacing
 * - Actions: Auto-arrange, SVG export
 * 
 * @see DataViewer_Widget for the view component
 * @see DataViewerState for shared state
 * @see DataViewerWidgetRegistration for factory registration
 */

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class DataViewerState;
class Feature_Tree_Model;
class OpenGLWidget;
class TimeFrame;

namespace Ui {
class DataViewerPropertiesWidget;
}

/**
 * @brief Properties panel for Data Viewer Widget
 * 
 * Displays feature tree, per-feature controls, and visualization settings.
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
     * @param opengl_widget OpenGLWidget from the view for feature controls (can be nullptr initially)
     * @param parent Parent widget
     */
    explicit DataViewerPropertiesWidget(std::shared_ptr<DataViewerState> state,
                                         std::shared_ptr<DataManager> data_manager,
                                         OpenGLWidget * opengl_widget = nullptr,
                                         QWidget * parent = nullptr);

    ~DataViewerPropertiesWidget() override;

    /**
     * @brief Set the OpenGLWidget reference
     * 
     * Called after construction if the OpenGLWidget wasn't available initially.
     * Initializes the feature control widgets that need it.
     * 
     * @param opengl_widget The OpenGLWidget from the view
     */
    void setOpenGLWidget(OpenGLWidget * opengl_widget);

    /**
     * @brief Set the time frame for coordinate conversion
     * @param time_frame Shared pointer to TimeFrame
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame);

    /**
     * @brief Refresh the feature tree from DataManager
     */
    void refreshFeatureTree();

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

    /**
     * @brief Emitted when a feature should be added to the plot
     * @param key The data key to add
     * @param color The color (hex string) for the feature
     */
    void featureAddRequested(std::string const & key, std::string const & color);

    /**
     * @brief Emitted when a feature should be removed from the plot
     * @param key The data key to remove
     */
    void featureRemoveRequested(std::string const & key);

    /**
     * @brief Emitted when multiple features should be added (batch operation)
     * @param keys The data keys to add
     * @param colors The colors (hex strings) for each key (parallel array)
     */
    void featuresAddRequested(std::vector<std::string> const & keys, std::vector<std::string> const & colors);

    /**
     * @brief Emitted when multiple features should be removed (batch operation)
     * @param keys The data keys to remove
     */
    void featuresRemoveRequested(std::vector<std::string> const & keys);

    /**
     * @brief Emitted when feature color changes
     * @param key The data key
     * @param hex_color New color in hex format
     */
    void featureColorChanged(std::string const & key, std::string const & hex_color);

    /**
     * @brief Emitted when a group context menu is requested
     * @param group_name The group name
     * @param global_pos Global position for the menu
     */
    void groupContextMenuRequested(std::string const & group_name, QPoint const & global_pos);

private slots:
    void _onThemeChanged(int index);
    void _onGlobalZoomChanged(double value);
    void _onXAxisSamplesChanged(int value);
    void _onGridLinesToggled(bool enabled);
    void _onGridSpacingChanged(int value);
    void _handleFeatureSelected(QString const & feature);
    void _handleColorChanged(std::string const & feature_key, std::string const & hex_color);

private:
    Ui::DataViewerPropertiesWidget * ui;
    std::shared_ptr<DataViewerState> _state;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget = nullptr;
    std::shared_ptr<TimeFrame> _time_frame;
    std::unique_ptr<Feature_Tree_Model> _feature_tree_model;
    
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

    /**
     * @brief Set up the feature tree widget
     */
    void _setupFeatureTree();

    /**
     * @brief Set up the stacked widget with per-type viewers
     */
    void _setupStackedWidget();
};

#endif  // DATA_VIEWER_PROPERTIES_WIDGET_HPP
