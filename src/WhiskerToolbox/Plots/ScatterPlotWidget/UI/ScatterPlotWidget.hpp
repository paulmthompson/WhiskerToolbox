#ifndef SCATTER_PLOT_WIDGET_HPP
#define SCATTER_PLOT_WIDGET_HPP

/**
 * @file ScatterPlotWidget.hpp
 * @brief Main widget for displaying 2D scatter plots
 *
 * Single source of truth: ScatterPlotState. Horizontal and vertical axis
 * widgets use state; pan/zoom in OpenGL widget update state.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class ScatterPlotState;
class ScatterPlotOpenGLWidget;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class ScatterPlotWidget;
}

/**
 * @brief Main widget for 2D scatter plot visualization
 */
class ScatterPlotWidget : public QWidget {
    Q_OBJECT

public:
    ScatterPlotWidget(std::shared_ptr<DataManager> data_manager,
                      QWidget * parent = nullptr);
    ~ScatterPlotWidget() override;

    void setState(std::shared_ptr<ScatterPlotState> state);
    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }
    [[nodiscard]] ScatterPlotState * state();

    [[nodiscard]] HorizontalAxisRangeControls * getHorizontalRangeControls() const;
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

signals:
    void timePositionSelected(TimePosition position);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    void createHorizontalAxisIfNeeded();
    void createVerticalAxisIfNeeded();
    void wireHorizontalAxis();
    void wireVerticalAxis();
    void connectViewChangeSignals();
    void syncHorizontalAxisRange();
    void syncVerticalAxisRange();

    [[nodiscard]] std::pair<double, double> computeVisibleXRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleYRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::ScatterPlotWidget * ui;
    std::shared_ptr<ScatterPlotState> _state;
    ScatterPlotOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif  // SCATTER_PLOT_WIDGET_HPP
