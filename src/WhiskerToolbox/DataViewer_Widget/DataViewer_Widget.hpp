#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include <QWidget>

#include "DataViewer/VerticalSpaceManager.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class DataManager;
class MainWindow;
class QTableWidget;
class TimeScrollBar;
class TimeFrame;
class Feature_Table_Widget;
class QWheelEvent;
class QResizeEvent;
class AnalogViewer_Widget;
class IntervalViewer_Widget;
class EventViewer_Widget;
enum class PlotTheme;
struct AnalogTimeSeriesDisplayOptions;
struct DigitalEventSeriesDisplayOptions;
struct DigitalIntervalSeriesDisplayOptions;

enum class ZoomScalingMode {
    Fixed,      // Original fixed zoom factor
    Adaptive    // Zoom factor scales with current zoom level
};

namespace Ui {
class DataViewer_Widget;
}

class DataViewer_Widget : public QWidget {
    Q_OBJECT

public:
    DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                      TimeScrollBar * time_scrollbar,
                      MainWindow * main_window,
                      QWidget * parent = nullptr);

    ~DataViewer_Widget() override;

    void openWidget();

    void updateXAxisSamples(int value);

    void setZoomScalingMode(ZoomScalingMode mode) { _zoom_scaling_mode = mode; }
    [[nodiscard]] ZoomScalingMode getZoomScalingMode() const { return _zoom_scaling_mode; }

    [[nodiscard]] std::optional<AnalogTimeSeriesDisplayOptions *> getAnalogConfig(std::string const & key) const;

    [[nodiscard]] std::optional<DigitalEventSeriesDisplayOptions *> getDigitalEventConfig(std::string const & key) const;

    [[nodiscard]] std::optional<DigitalIntervalSeriesDisplayOptions *> getDigitalIntervalConfig(std::string const & key) const;

    /**
     * @brief Automatically arrange all visible series for optimal spacing
     * 
     * This function recalculates positioning for all currently visible series
     * to achieve optimal vertical space distribution and prevent overlap.
     */
    void autoArrangeVerticalSpacing();

    /**
     * @brief Print debug information about vertical spacing and positioning
     * 
     * Useful for diagnosing overlap and positioning issues. Prints detailed
     * information about all series positions, bounds, and spacing.
     */
    void debugVerticalSpacing() const;

protected:
    void closeEvent(QCloseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;
private slots:
    //void _insertRows(const std::vector<std::string>& keys);
    void _addFeatureToModel(QString const & feature, bool enabled);
    //void _highlightAvailableFeature(int row, int column);
    void _plotSelectedFeature(std::string const & key);
    void _removeSelectedFeature(std::string const & key);
    void _plotSelectedFeatureWithoutUpdate(std::string const & key);
    void _removeSelectedFeatureWithoutUpdate(std::string const & key);
    void _updatePlot(int time);
    void _handleFeatureSelected(QString const & feature);
    void _handleXAxisSamplesChanged(int value);
    void _updateGlobalScale(double scale);
    void _handleColorChanged(std::string const & feature_key, std::string const & hex_color);
    void _updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info);
    void _handleThemeChanged(int theme_index);
    void _handleGridLinesToggled(bool enabled);
    void _handleGridSpacingChanged(int spacing);
    void _handleVerticalSpacingChanged(double spacing);

private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar * _time_scrollbar;
    MainWindow * _main_window;
    Ui::DataViewer_Widget * ui;

    std::shared_ptr<TimeFrame> _time_frame;

    QString _highlighted_available_feature;
    ZoomScalingMode _zoom_scaling_mode{ZoomScalingMode::Adaptive}; // Use adaptive scaling by default

    // Vertical space management
    std::unique_ptr<VerticalSpaceManager> _vertical_space_manager;

    void _updateLabels();
    
    /**
     * @brief Convert DataManager data type to VerticalSpaceManager data type
     * 
     * @param dm_type DataManager data type
     * @return Corresponding DataSeriesType for VerticalSpaceManager
     */
    DataSeriesType _convertDataType(DM_DataType dm_type) const;
    
    /**
     * @brief Update canvas dimensions in the vertical space manager
     * 
     * Called when the OpenGL widget is resized to keep spacing calculations current.
     */
    void _updateVerticalSpaceManagerDimensions();
    
    /**
     * @brief Update OpenGL widget view bounds based on content height
     * 
     * Adjusts the viewport to accommodate all content from VerticalSpaceManager.
     */
    void _updateViewBounds();
    
    /**
     * @brief Apply vertical space manager positioning to OpenGL widget
     * 
     * Updates the positioning parameters for a specific series based on the
     * vertical space manager's calculations.
     * 
     * @param series_key Key of the series to update
     */
    void _applyVerticalSpacing(std::string const & series_key);
    
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
};


#endif//DATAVIEWER_WIDGET_HPP
