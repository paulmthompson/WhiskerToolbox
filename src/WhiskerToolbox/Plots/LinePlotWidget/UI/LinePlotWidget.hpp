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
class RelativeTimeAxisWidget;
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

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    void resizeEvent(QResizeEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::LinePlotWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<LinePlotState> _state;

    /// OpenGL widget for rendering the line plot
    LinePlotOpenGLWidget * _opengl_widget;

    /// Axis widget for displaying relative time axis
    RelativeTimeAxisWidget * _axis_widget;
};

#endif// LINE_PLOT_WIDGET_HPP
