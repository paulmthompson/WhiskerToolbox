#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include "Ordering/ChannelPositionMetadata.hpp"
#include "Ordering/SwindaleSpikeSorterLoader.hpp"

#include "DataTypeEnum/DM_DataType.hpp"
#include "EditorState/StrongTypes.hpp"// For TimePosition
#include "TimeFrame/TimeFrame.hpp"

#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class DataViewerState;
class EditorRegistry;
class MainWindow;
class MultiLaneVerticalAxisWidget;
class OpenGLWidget;
class QTableWidget;
class TimeFrame;
class Feature_Table_Widget;
class Feature_Tree_Model;
class QWheelEvent;
class QResizeEvent;
class AnalogViewer_Widget;
class IntervalViewer_Widget;
class EventViewer_Widget;

namespace Ui {
class DataViewer_Widget;
}

class DataViewer_Widget : public QWidget {
    Q_OBJECT

public:
    DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                      QWidget * parent = nullptr);

    ~DataViewer_Widget() override;

    void openWidget();

    /**
     * @brief Set the DataViewerState for this widget
     * 
     * The state manages all serializable settings including view state,
     * theme, grid settings, UI preferences, and per-series display options.
     * This widget owns the state and shares it with OpenGLWidget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<DataViewerState> state);

    /**
     * @brief Get the current DataViewerState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<DataViewerState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] DataViewerState * state();

    /**
     * @brief Get the OpenGLWidget (for testing)
     * @return Pointer to the internal OpenGLWidget
     */
    [[nodiscard]] OpenGLWidget * getOpenGLWidget() const;

    /**
     * @brief Automatically arrange all visible series for optimal spacing
     * 
     * This function recalculates positioning for all currently visible series
     * to achieve optimal vertical space distribution and prevent overlap.
     */
    void autoArrangeVerticalSpacing();

    /**
     * @brief Export the current plot to SVG
     * 
     * Opens a file dialog and exports the plot to an SVG file.
     * 
     * @param includeScalebar Whether to include a scalebar in the export
     * @param scalebarLength Length of the scalebar in time units
     */
    void exportToSVG(bool includeScalebar, int scalebarLength);

    /**
     * @brief Add a feature to the plot
     * 
     * Called by the properties widget when a feature is toggled on.
     * 
     * @param key The data key to add
     * @param color The color (hex string) for the feature
     */
    void addFeature(std::string const & key, std::string const & color);

    /**
     * @brief Remove a feature from the plot
     * 
     * Called by the properties widget when a feature is toggled off.
     * 
     * @param key The data key to remove
     */
    void removeFeature(std::string const & key);

    /**
     * @brief Add multiple features (batch operation)
     * 
     * Called by the properties widget when a group is toggled on.
     * 
     * @param keys The data keys to add
     * @param colors The colors (hex strings) for each key (parallel array)
     */
    void addFeatures(std::vector<std::string> const & keys, std::vector<std::string> const & colors);

    /**
     * @brief Remove multiple features (batch operation)
     * 
     * Called by the properties widget when a group is toggled off.
     * 
     * @param keys The data keys to remove
     */
    void removeFeatures(std::vector<std::string> const & keys);

    /**
     * @brief Handle color change from properties widget
     * 
     * @param key The data key
     * @param hex_color New color in hex format
     */
    void handleColorChanged(std::string const & key, std::string const & hex_color);

    /**
     * @brief Show context menu for a group
     * 
     * @param group_name The group name
     * @param global_pos Global position for the menu
     */
    void showGroupContextMenu(std::string const & group_name, QPoint const & global_pos);

    void _onTimeChanged(TimePosition const & position);

protected:
    void closeEvent(QCloseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;
private slots:
    void _plotSelectedFeature(std::string const & key, std::string const & color);
    void _removeSelectedFeature(std::string const & key);
    void _plotSelectedFeatureWithoutUpdate(std::string const & key, std::string const & color);
    void _removeSelectedFeatureWithoutUpdate(std::string const & key);
    void _updatePlot(int time);
    void _updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info);
    void _loadSpikeSorterConfigurationForGroup(QString const & group_name);
    void _loadSpikeSorterConfigurationFromText(QString const & group_name, QString const & text);
    void _clearConfigurationForGroup(QString const & group_name);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::DataViewer_Widget * ui;

    /// Serializable state shared with OpenGLWidget
    /// Source of truth for view state, theme, grid, UI preferences
    std::shared_ptr<DataViewerState> _state;

    std::shared_ptr<TimeFrame> _time_frame;

    /// Multi-lane vertical axis widget (non-owning; owned by Qt parent)
    MultiLaneVerticalAxisWidget * _multi_lane_axis_widget{nullptr};

    // Flag to prevent auto-arrange during batch operations
    bool _is_batch_add = false;

    void _updateLabels();

    /**
     * @brief Update lane descriptors for the multi-lane axis widget
     *
     * Called after each scene rebuild to extract lane descriptors from the
     * LayoutResponse and push them to the MultiLaneVerticalAxisState.
     */
    void _updateLaneDescriptors();

    /**
     * @brief Convert DataManager data type to series type for PlottingManager
     * 
     * @param dm_type DataManager data type
     * @return String identifier for the data type
     */
    static std::string _convertDataType(DM_DataType dm_type);

    void _calculateOptimalScaling(std::vector<std::string> const & group_keys);

    /**
     * @brief Calculate optimal spacing and height for digital event series when loading groups
     * 
     * Automatically calculates and applies optimal vertical spacing and event height
     * for a group of digital event series to ensure all events fit well on the canvas.
     * Similar to _calculateOptimalScaling but specifically for event stacking parameters.
     * 
     * @param group_keys Vector of series keys in the group to auto-configure
     */
    void _calculateOptimalEventSpacing(std::vector<std::string> const & group_keys);

    /**
     * @brief Automatically scale all visible series to fill the available canvas
     * 
     * Calculates optimal scaling factors for all currently visible series
     * (analog, digital events, digital intervals) to make best use of the
     * available canvas space. Adjusts global scale and vertical spacing
     * to minimize empty space.
     */
    void _autoFillCanvas();

    /**
     * @brief Apply group-unified std_dev scaling after a batch add
     * 
     * Detects analog series among the added keys, computes the group's
     * max std_dev, and applies it to all analog series in the batch.
     * Stores group scaling state in DataViewerState.
     * 
     * @param keys All keys added in the batch (may include non-analog)
     */
    void _applyGroupScalingForBatch(std::vector<std::string> const & keys);

    /**
     * @brief Toggle unified scaling for a specific group
     * 
     * When enabling: applies group max std_dev to all series matching the group.
     * When disabling: reverts each series to its individual std_dev.
     * 
     * @param group_name Group identifier (e.g., "voltage")
     * @param enabled true to enable unified scaling, false to revert
     */
    void _toggleGroupUnifiedScaling(std::string const & group_name, bool enabled);

    /**
     * @brief Find all analog series keys belonging to a group
     * 
     * Searches the currently displayed analog series for keys whose
     * group name matches the specified pattern (key prefix before _N suffix).
     * 
     * @param group_name Group identifier
     * @return Vector of matching series keys
     */
    [[nodiscard]] std::vector<std::string> _findGroupAnalogKeys(std::string const & group_name) const;

    /**
     * @brief Clean up data references that have been deleted from the DataManager
     * 
     * This method is called by the DataManager observer to automatically
     * remove data references when they are deleted from the DataManager.
     * It cleans up both the OpenGLWidget and PlottingManager references.
     */
    void cleanupDeletedData();

    // Parsing helper
    static std::vector<ChannelPosition> _parseSpikeSorterConfig(std::string const & text);

    // ==================== Wheel Modifier Helpers ====================

    /**
     * @brief Handle Alt+Scroll → Global Y scale adjustment
     * @param numSteps Normalized wheel steps (positive = scroll up)
     */
    void _handleGlobalYScaleWheel(float numSteps);

    /**
     * @brief Handle Alt+Shift+Scroll → Per-lane Y scale for hovered analog series
     * @param numSteps Normalized wheel steps
     * @param pos Mouse position in widget coordinates
     */
    void _handlePerLaneYScaleWheel(float numSteps, QPointF const & pos);

    /**
     * @brief Handle Alt+Ctrl+Scroll → Y viewport zoom (cursor-anchored)
     * @param numSteps Normalized wheel steps
     * @param pos Mouse position in widget coordinates
     */
    void _handleYViewportZoomWheel(float numSteps, QPointF const & pos);
};


#endif//DATAVIEWER_WIDGET_HPP
