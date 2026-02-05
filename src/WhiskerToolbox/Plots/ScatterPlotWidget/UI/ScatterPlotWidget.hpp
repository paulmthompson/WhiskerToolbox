#ifndef SCATTER_PLOT_WIDGET_HPP
#define SCATTER_PLOT_WIDGET_HPP

/**
 * @file ScatterPlotWidget.hpp
 * @brief Main widget for displaying 2D scatter plots
 * 
 * ScatterPlotWidget displays 2D scatter plots showing relationships
 * between two variables.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class ScatterPlotState;

namespace Ui {
class ScatterPlotWidget;
}

/**
 * @brief Main widget for 2D scatter plot visualization
 */
class ScatterPlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a ScatterPlotWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    ScatterPlotWidget(std::shared_ptr<DataManager> data_manager,
                      QWidget * parent = nullptr);

    ~ScatterPlotWidget() override;

    /**
     * @brief Set the ScatterPlotState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<ScatterPlotState> state);

    /**
     * @brief Get the current ScatterPlotState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] ScatterPlotState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ScatterPlotWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<ScatterPlotState> _state;
};

#endif// SCATTER_PLOT_WIDGET_HPP
