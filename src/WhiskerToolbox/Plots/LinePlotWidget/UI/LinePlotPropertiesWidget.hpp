#ifndef LINE_PLOT_PROPERTIES_WIDGET_HPP
#define LINE_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file LinePlotPropertiesWidget.hpp
 * @brief Properties panel for the Line Plot Widget
 * 
 * LinePlotPropertiesWidget is the properties/inspector panel for LinePlotWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see LinePlotWidget for the view component
 * @see LinePlotState for shared state
 * @see LinePlotWidgetRegistration for factory registration
 */

#include "Core/LinePlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class PlotAlignmentWidget;

namespace Ui {
class LinePlotPropertiesWidget;
}

/**
 * @brief Properties panel for Line Plot Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with LinePlotWidget (view) via LinePlotState.
 */
class LinePlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a LinePlotPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit LinePlotPropertiesWidget(std::shared_ptr<LinePlotState> state,
                                      std::shared_ptr<DataManager> data_manager,
                                      QWidget * parent = nullptr);

    ~LinePlotPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to LinePlotState
     */
    [[nodiscard]] std::shared_ptr<LinePlotState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private:
    Ui::LinePlotPropertiesWidget * ui;
    std::shared_ptr<LinePlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;
};

#endif// LINE_PLOT_PROPERTIES_WIDGET_HPP
