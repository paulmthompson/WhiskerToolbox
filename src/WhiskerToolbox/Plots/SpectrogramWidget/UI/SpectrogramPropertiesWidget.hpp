#ifndef SPECTROGRAM_PROPERTIES_WIDGET_HPP
#define SPECTROGRAM_PROPERTIES_WIDGET_HPP

/**
 * @file SpectrogramPropertiesWidget.hpp
 * @brief Properties panel for the Spectrogram Widget
 * 
 * SpectrogramPropertiesWidget is the properties/inspector panel for SpectrogramWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see SpectrogramWidget for the view component
 * @see SpectrogramState for shared state
 * @see SpectrogramWidgetRegistration for factory registration
 */

#include "Core/SpectrogramState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class SpectrogramWidget;

namespace Ui {
class SpectrogramPropertiesWidget;
}

/**
 * @brief Properties panel for Spectrogram Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with SpectrogramWidget (view) via SpectrogramState.
 */
class SpectrogramPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a SpectrogramPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit SpectrogramPropertiesWidget(std::shared_ptr<SpectrogramState> state,
                                         std::shared_ptr<DataManager> data_manager,
                                         QWidget * parent = nullptr);

    ~SpectrogramPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to SpectrogramState
     */
    [[nodiscard]] std::shared_ptr<SpectrogramState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the SpectrogramWidget
     * @param plot_widget The SpectrogramWidget instance
     */
    void setPlotWidget(SpectrogramWidget * plot_widget);

private:
    Ui::SpectrogramPropertiesWidget * ui;
    std::shared_ptr<SpectrogramState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SpectrogramWidget * _plot_widget;
};

#endif// SPECTROGRAM_PROPERTIES_WIDGET_HPP
