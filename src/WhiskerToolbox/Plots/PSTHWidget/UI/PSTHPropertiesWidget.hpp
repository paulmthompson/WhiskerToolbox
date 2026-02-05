#ifndef PSTH_PROPERTIES_WIDGET_HPP
#define PSTH_PROPERTIES_WIDGET_HPP

/**
 * @file PSTHPropertiesWidget.hpp
 * @brief Properties panel for the PSTH Widget
 * 
 * PSTHPropertiesWidget is the properties/inspector panel for PSTHWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see PSTHWidget for the view component
 * @see PSTHState for shared state
 * @see PSTHWidgetRegistration for factory registration
 */

#include "Core/PSTHState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class PlotAlignmentWidget;

namespace Ui {
class PSTHPropertiesWidget;
}

/**
 * @brief Properties panel for PSTH Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with PSTHWidget (view) via PSTHState.
 */
class PSTHPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a PSTHPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit PSTHPropertiesWidget(std::shared_ptr<PSTHState> state,
                                   std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent = nullptr);

    ~PSTHPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to PSTHState
     */
    [[nodiscard]] std::shared_ptr<PSTHState> state() const { return _state; }

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
     * @brief Handle color button click
     */
    void _onColorButtonClicked();

    /**
     * @brief Handle style combo box selection change
     * @param index Selected index (0 = Bar, 1 = Line)
     */
    void _onStyleChanged(int index);

    /**
     * @brief Handle bin size spinbox value change
     * @param value New bin size value
     */
    void _onBinSizeChanged(double value);

    /**
     * @brief Handle Y-axis minimum spinbox value change
     * @param value New Y-axis minimum value
     */
    void _onYMinChanged(double value);

    /**
     * @brief Handle Y-axis maximum spinbox value change
     * @param value New Y-axis maximum value
     */
    void _onYMaxChanged(double value);

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

    Ui::PSTHPropertiesWidget * ui;
    std::shared_ptr<PSTHState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif// PSTH_PROPERTIES_WIDGET_HPP
