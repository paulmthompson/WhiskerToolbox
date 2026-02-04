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

#include "Core/EventPlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;

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

private slots:
    /**
     * @brief Handle plot event combo box selection change
     * @param index Selected index
     */
    void _onPlotEventChanged(int index);

    /**
     * @brief Handle alignment event combo box selection change
     * @param index Selected index
     */
    void _onAlignmentEventChanged(int index);

    /**
     * @brief Handle interval alignment combo box selection change
     * @param index Selected index (0 = Beginning, 1 = End)
     */
    void _onIntervalAlignmentChanged(int index);

    /**
     * @brief Handle offset spinbox value change
     * @param value New offset value
     */
    void _onOffsetChanged(double value);

    /**
     * @brief Handle state plot event key change
     * @param key New plot event key
     */
    void _onStatePlotEventKeyChanged(QString const & key);

    /**
     * @brief Handle state alignment event key change
     * @param key New alignment event key
     */
    void _onStateAlignmentEventKeyChanged(QString const & key);

    /**
     * @brief Handle state interval alignment type change
     * @param type New interval alignment type
     */
    void _onStateIntervalAlignmentTypeChanged(IntervalAlignmentType type);

    /**
     * @brief Handle state offset change
     * @param offset New offset value
     */
    void _onStateOffsetChanged(double offset);

private:
    /**
     * @brief Populate the plot event combo box with available DigitalEventSeries keys
     */
    void _populatePlotEventComboBox();

    /**
     * @brief Populate the alignment event combo box with available event/interval series
     */
    void _populateAlignmentEventComboBox();

    /**
     * @brief Update the event/interval count labels based on current selection
     */
    void _updateEventCount();

    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    Ui::EventPlotPropertiesWidget * ui;
    std::shared_ptr<EventPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    
    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif  // EVENT_PLOT_PROPERTIES_WIDGET_HPP
