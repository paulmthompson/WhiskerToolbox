#ifndef DATAVIEWER_WIDGET_HPP
#define DATAVIEWER_WIDGET_HPP

#include <QWidget>

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

    void _updateLabels();
    void _calculateOptimalScaling(std::vector<std::string> const & group_keys);
};


#endif//DATAVIEWER_WIDGET_HPP
