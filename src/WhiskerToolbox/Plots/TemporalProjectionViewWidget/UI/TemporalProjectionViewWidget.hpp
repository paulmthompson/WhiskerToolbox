#ifndef TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_WIDGET_HPP

/**
 * @file TemporalProjectionViewWidget.hpp
 * @brief Main widget for displaying temporal projection views
 *
 * TemporalProjectionViewWidget displays 2D projections of data collapsed across
 * temporal windows. Single source of truth: TemporalProjectionViewState.
 * Horizontal and vertical axis widgets use state; pan/zoom in OpenGL widget
 * update state.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class TemporalProjectionViewState;
class TemporalProjectionOpenGLWidget;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class TemporalProjectionViewWidget;
}

/**
 * @brief Main widget for temporal projection view visualization
 */
class TemporalProjectionViewWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    TemporalProjectionViewWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent = nullptr);

    ~TemporalProjectionViewWidget() override;

    void setState(std::shared_ptr<TemporalProjectionViewState> state);
    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }
    [[nodiscard]] TemporalProjectionViewState * state();

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
    Ui::TemporalProjectionViewWidget * ui;
    std::shared_ptr<TemporalProjectionViewState> _state;
    TemporalProjectionOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif  // TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
