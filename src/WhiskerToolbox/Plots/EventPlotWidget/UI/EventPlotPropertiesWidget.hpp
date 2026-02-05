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
class PlotAlignmentWidget;

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
     * @brief Handle add event button click
     */
    void _onAddEventClicked();

    /**
     * @brief Handle remove event button click
     */
    void _onRemoveEventClicked();

    /**
     * @brief Handle plot event table selection change
     */
    void _onPlotEventSelectionChanged();

    /**
     * @brief Handle state plot event added
     * @param event_name Name of the added event
     */
    void _onStatePlotEventAdded(QString const & event_name);

    /**
     * @brief Handle state plot event removed
     * @param event_name Name of the removed event
     */
    void _onStatePlotEventRemoved(QString const & event_name);

    /**
     * @brief Handle state plot event options changed
     * @param event_name Name of the updated event
     */
    void _onStatePlotEventOptionsChanged(QString const & event_name);

    /**
     * @brief Handle tick thickness spinbox value change
     * @param value New thickness value
     */
    void _onTickThicknessChanged(double value);

    /**
     * @brief Handle glyph type combo box selection change
     * @param index Selected index (0 = Tick, 1 = Circle, 2 = Square)
     */
    void _onGlyphTypeChanged(int index);

    /**
     * @brief Handle color button click
     */
    void _onColorButtonClicked();

    /**
     * @brief Handle background color button click
     */
    void _onBackgroundColorButtonClicked();

    /**
     * @brief Handle state background color changed
     * @param hex_color New background color as hex string
     */
    void _onStateBackgroundColorChanged(QString const & hex_color);

private:
    /**
     * @brief Populate the add event combo box with available DigitalEventSeries keys
     */
    void _populateAddEventComboBox();

    /**
     * @brief Update the plot events table from state
     */
    void _updatePlotEventsTable();

    /**
     * @brief Update the event options display for the selected event
     * @param event_name Name of the event to display options for
     */
    void _updateEventOptions(QString const & event_name);

    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    /**
     * @brief Get the currently selected event name from the table
     * @return Event name if selection exists, empty string otherwise
     */
    [[nodiscard]] QString _getSelectedEventName() const;

    /**
     * @brief Update the color display button with a hex color
     * @param hex_color Hex color string (e.g., "#000000")
     */
    void _updateColorDisplay(QString const & hex_color);

    /**
     * @brief Update the background color display button with a hex color
     * @param hex_color Hex color string (e.g., "#FFFFFF")
     */
    void _updateBackgroundColorDisplay(QString const & hex_color);

    Ui::EventPlotPropertiesWidget * ui;
    std::shared_ptr<EventPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif// EVENT_PLOT_PROPERTIES_WIDGET_HPP
