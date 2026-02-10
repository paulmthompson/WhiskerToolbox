#ifndef LINE_PLOT_WIDGET_HPP
#define LINE_PLOT_WIDGET_HPP

/**
 * @file LinePlotWidget.hpp
 * @brief Main widget for displaying line plots
 * 
 * LinePlotWidget displays line plots for visualizing time series data.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class LinePlotState;
class LinePlotOpenGLWidget;
class RelativeTimeAxisRangeControls;
class RelativeTimeAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class LinePlotWidget;
}

/**
 * @brief Main widget for line plot visualization
 */
class LinePlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a LinePlotWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    LinePlotWidget(std::shared_ptr<DataManager> data_manager,
                   QWidget * parent = nullptr);

    ~LinePlotWidget() override;

    /**
     * @brief Set the LinePlotState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<LinePlotState> state);

    /**
     * @brief Get the current LinePlotState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<LinePlotState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] LinePlotState * state();

    /**
     * @brief Get the range controls widget (for placement in properties panel)
     * @return Pointer to the range controls widget, or nullptr if not created
     */
    [[nodiscard]] RelativeTimeAxisRangeControls * getRangeControls() const;

    /**
     * @brief Get the vertical axis range controls widget (for placement in properties panel)
     * @return Pointer to the vertical axis range controls widget, or nullptr if not created
     */
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    void createTimeAxisIfNeeded();
    void wireTimeAxis();
    void wireVerticalAxis();
    void connectViewChangeSignals();
    void syncTimeAxisRange();
    void syncVerticalAxisRange();

    [[nodiscard]] std::pair<double, double> computeVisibleTimeRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleVerticalRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::LinePlotWidget * ui;

    std::shared_ptr<LinePlotState> _state;
    LinePlotOpenGLWidget * _opengl_widget;

    RelativeTimeAxisWidget * _axis_widget;
    RelativeTimeAxisRangeControls * _range_controls;

    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif// LINE_PLOT_WIDGET_HPP
