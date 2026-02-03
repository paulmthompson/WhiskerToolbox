#ifndef EVENT_PLOT_PROPERTIES_WIDGET_HPP
#define EVENT_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file EventPlotPropertiesWidget.hpp
 * @brief Properties panel for the Event Plot Widget
 * 
 * EventPlotPropertiesWidget is the properties/inspector panel for EventPlotWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see EventPlotWidget for the view component
 * @see EventPlotState for shared state
 * @see EventPlotWidgetRegistration for factory registration
 */

#include <QWidget>

#include <memory>

class DataManager;
class EventPlotState;

namespace Ui {
class EventPlotPropertiesWidget;
}

/**
 * @brief Properties panel for Event Plot Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with EventPlotWidget (view) via EventPlotState.
 */
class EventPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an EventPlotPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                        std::shared_ptr<DataManager> data_manager,
                                        QWidget * parent = nullptr);

    ~EventPlotPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to EventPlotState
     */
    [[nodiscard]] std::shared_ptr<EventPlotState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private:
    Ui::EventPlotPropertiesWidget * ui;
    std::shared_ptr<EventPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif  // EVENT_PLOT_PROPERTIES_WIDGET_HPP
