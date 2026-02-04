#ifndef HEATMAP_PROPERTIES_WIDGET_HPP
#define HEATMAP_PROPERTIES_WIDGET_HPP

/**
 * @file HeatmapPropertiesWidget.hpp
 * @brief Properties panel for the Heatmap Widget
 * 
 * HeatmapPropertiesWidget is the properties/inspector panel for HeatmapWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see HeatmapWidget for the view component
 * @see HeatmapState for shared state
 * @see HeatmapWidgetRegistration for factory registration
 */

#include "Core/HeatmapState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class PlotAlignmentWidget;

namespace Ui {
class HeatmapPropertiesWidget;
}

/**
 * @brief Properties panel for Heatmap Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with HeatmapWidget (view) via HeatmapState.
 */
class HeatmapPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a HeatmapPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                     std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent = nullptr);

    ~HeatmapPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to HeatmapState
     */
    [[nodiscard]] std::shared_ptr<HeatmapState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

private:
    Ui::HeatmapPropertiesWidget * ui;
    std::shared_ptr<HeatmapState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;
};

#endif// HEATMAP_PROPERTIES_WIDGET_HPP
