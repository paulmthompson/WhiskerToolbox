#ifndef EVENT_PLOT_WIDGET_HPP
#define EVENT_PLOT_WIDGET_HPP

/**
 * @file EventPlotWidget.hpp
 * @brief Main widget for displaying event raster plots
 * 
 * EventPlotWidget displays neuroscience-style raster plots showing events
 * across multiple channels or trials.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class RelativeTimeAxisWidget;
class VerticalAxisWidget;
class EventPlotOpenGLWidget;
class EventPlotState;

namespace Ui {
class EventPlotWidget;
}

/**
 * @brief Main widget for event raster plot visualization
 */
class EventPlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an EventPlotWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                    QWidget * parent = nullptr);

    ~EventPlotWidget() override;

    /**
     * @brief Set the EventPlotState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<EventPlotState> state);

    /**
     * @brief Get the current EventPlotState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<EventPlotState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] EventPlotState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

    /**
     * @brief Emitted when user single-clicks on an event to select it
     * @param trial_index The trial (row) index of the selected event
     * @param relative_time_ms The time relative to alignment in ms
     * @param series_key The key of the event series containing the event
     */
    void eventSelected(int trial_index, float relative_time_ms, QString const & series_key);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::EventPlotWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<EventPlotState> _state;

    /// OpenGL rendering widget
    EventPlotOpenGLWidget * _opengl_widget;

    /// Time axis widget below the plot
    RelativeTimeAxisWidget * _axis_widget;

    /// Vertical axis widget on the left side
    VerticalAxisWidget * _vertical_axis_widget;

    /// Cached trial count for vertical axis range calculation
    size_t _trial_count = 0;
};

#endif// EVENT_PLOT_WIDGET_HPP
