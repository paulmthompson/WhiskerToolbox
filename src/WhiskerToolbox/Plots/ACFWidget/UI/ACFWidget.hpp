#ifndef ACF_WIDGET_HPP
#define ACF_WIDGET_HPP

/**
 * @file ACFWidget.hpp
 * @brief Main widget for displaying autocorrelation function plots
 *
 * Single source of truth: ACFState. Horizontal and vertical axis widgets use
 * state; pan/zoom in OpenGL widget update state. Axis labels: Lag (X), Value (Y).
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class ACFOpenGLWidget;
class ACFState;
class DataManager;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class ACFWidget;
}

/**
 * @brief Main widget for autocorrelation function visualization
 */
class ACFWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an ACFWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    ACFWidget(std::shared_ptr<DataManager> data_manager,
              QWidget * parent = nullptr);

    ~ACFWidget() override;

    void setState(std::shared_ptr<ACFState> state);
    [[nodiscard]] std::shared_ptr<ACFState> state() const { return _state; }
    [[nodiscard]] ACFState * state();

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
    Ui::ACFWidget * ui;
    std::shared_ptr<ACFState> _state;
    ACFOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif  // ACF_WIDGET_HPP
