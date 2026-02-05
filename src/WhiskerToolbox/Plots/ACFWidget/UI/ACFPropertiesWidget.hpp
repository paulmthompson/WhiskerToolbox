#ifndef ACF_PROPERTIES_WIDGET_HPP
#define ACF_PROPERTIES_WIDGET_HPP

/**
 * @file ACFPropertiesWidget.hpp
 * @brief Properties panel for the ACF Widget
 * 
 * ACFPropertiesWidget is the properties/inspector panel for ACFWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see ACFWidget for the view component
 * @see ACFState for shared state
 * @see ACFWidgetRegistration for factory registration
 */

#include "Core/ACFState.hpp"

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui {
class ACFPropertiesWidget;
}

/**
 * @brief Properties panel for ACF Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with ACFWidget (view) via ACFState.
 */
class ACFPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an ACFPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit ACFPropertiesWidget(std::shared_ptr<ACFState> state,
                                  std::shared_ptr<DataManager> data_manager,
                                  QWidget * parent = nullptr);

    ~ACFPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to ACFState
     */
    [[nodiscard]] std::shared_ptr<ACFState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private slots:
    /**
     * @brief Handle event key combo box selection change
     * @param index Selected index
     */
    void _onEventKeyComboChanged(int index);

private:
    /**
     * @brief Populate the event key combo box with available DigitalEventSeries keys
     */
    void _populateEventKeyComboBox();

    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    Ui::ACFPropertiesWidget * ui;
    std::shared_ptr<ACFState> _state;
    std::shared_ptr<DataManager> _data_manager;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif// ACF_PROPERTIES_WIDGET_HPP
