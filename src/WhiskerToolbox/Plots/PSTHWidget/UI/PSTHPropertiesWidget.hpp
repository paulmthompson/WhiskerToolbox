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

private:
    Ui::PSTHPropertiesWidget * ui;
    std::shared_ptr<PSTHState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;
};

#endif// PSTH_PROPERTIES_WIDGET_HPP
