#ifndef SCATTER_PLOT_PROPERTIES_WIDGET_HPP
#define SCATTER_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file ScatterPlotPropertiesWidget.hpp
 * @brief Properties panel for the Scatter Plot Widget
 * 
 * ScatterPlotPropertiesWidget is the properties/inspector panel for ScatterPlotWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see ScatterPlotWidget for the view component
 * @see ScatterPlotState for shared state
 * @see ScatterPlotWidgetRegistration for factory registration
 */

#include "Core/ScatterPlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui {
class ScatterPlotPropertiesWidget;
}

/**
 * @brief Properties panel for Scatter Plot Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with ScatterPlotWidget (view) via ScatterPlotState.
 */
class ScatterPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a ScatterPlotPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                         std::shared_ptr<DataManager> data_manager,
                                         QWidget * parent = nullptr);

    ~ScatterPlotPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to ScatterPlotState
     */
    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private:
    Ui::ScatterPlotPropertiesWidget * ui;
    std::shared_ptr<ScatterPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif// SCATTER_PLOT_PROPERTIES_WIDGET_HPP
