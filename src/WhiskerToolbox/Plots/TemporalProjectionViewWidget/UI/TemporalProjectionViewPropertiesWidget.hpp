#ifndef TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP

/**
 * @file TemporalProjectionViewPropertiesWidget.hpp
 * @brief Properties panel for the Temporal Projection View Widget
 * 
 * TemporalProjectionViewPropertiesWidget is the properties/inspector panel for TemporalProjectionViewWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see TemporalProjectionViewWidget for the view component
 * @see TemporalProjectionViewState for shared state
 * @see TemporalProjectionViewWidgetRegistration for factory registration
 */

#include "Core/TemporalProjectionViewState.hpp"

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui {
class TemporalProjectionViewPropertiesWidget;
}

/**
 * @brief Properties panel for Temporal Projection View Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with TemporalProjectionViewWidget (view) via TemporalProjectionViewState.
 */
class TemporalProjectionViewPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit TemporalProjectionViewPropertiesWidget(std::shared_ptr<TemporalProjectionViewState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent = nullptr);

    ~TemporalProjectionViewPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to TemporalProjectionViewState
     */
    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private slots:
    /**
     * @brief Handle X-axis minimum value changes
     * @param value New X-axis minimum value
     */
    void _onXMinChanged(double value);

    /**
     * @brief Handle X-axis maximum value changes
     * @param value New X-axis maximum value
     */
    void _onXMaxChanged(double value);

    /**
     * @brief Handle Y-axis minimum value changes
     * @param value New Y-axis minimum value
     */
    void _onYMinChanged(double value);

    /**
     * @brief Handle Y-axis maximum value changes
     * @param value New Y-axis maximum value
     */
    void _onYMaxChanged(double value);

private:
    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    Ui::TemporalProjectionViewPropertiesWidget * ui;
    std::shared_ptr<TemporalProjectionViewState> _state;
    std::shared_ptr<DataManager> _data_manager;
};

#endif// TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
