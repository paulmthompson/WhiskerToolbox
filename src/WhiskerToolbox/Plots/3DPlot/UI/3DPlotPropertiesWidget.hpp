#ifndef THREE_D_PLOT_PROPERTIES_WIDGET_HPP
#define THREE_D_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file 3DPlotPropertiesWidget.hpp
 * @brief Properties panel for the 3D Plot Widget
 * 
 * 3DPlotPropertiesWidget is the properties/inspector panel for ThreeDPlotWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see ThreeDPlotWidget for the view component
 * @see ThreeDPlotState for shared state
 * @see ThreeDPlotWidgetRegistration for factory registration
 */

#include "Core/3DPlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class ThreeDPlotWidget;

namespace Ui {
class ThreeDPlotPropertiesWidget;
}

/**
 * @brief Properties panel for 3D Plot Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with ThreeDPlotWidget (view) via ThreeDPlotState.
 */
class ThreeDPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a ThreeDPlotPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit ThreeDPlotPropertiesWidget(std::shared_ptr<ThreeDPlotState> state,
                                        std::shared_ptr<DataManager> data_manager,
                                        QWidget * parent = nullptr);

    ~ThreeDPlotPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to ThreeDPlotState
     */
    [[nodiscard]] std::shared_ptr<ThreeDPlotState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the ThreeDPlotWidget to connect controls
     * @param plot_widget The ThreeDPlotWidget instance
     */
    void setPlotWidget(ThreeDPlotWidget * plot_widget);

private:
    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    Ui::ThreeDPlotPropertiesWidget * ui;
    std::shared_ptr<ThreeDPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    ThreeDPlotWidget * _plot_widget;
};

#endif// THREE_D_PLOT_PROPERTIES_WIDGET_HPP
