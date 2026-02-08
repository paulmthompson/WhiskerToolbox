#ifndef SCATTER_PLOT_PROPERTIES_WIDGET_HPP
#define SCATTER_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file ScatterPlotPropertiesWidget.hpp
 * @brief Properties panel for the Scatter Plot Widget
 *
 * Axis range controls are provided via HorizontalAxisRangeControls and
 * VerticalAxisRangeControls in collapsible sections (set when setPlotWidget is called).
 *
 * @see ScatterPlotWidget, ScatterPlotState, ScatterPlotWidgetRegistration
 */

#include "Core/ScatterPlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HorizontalAxisRangeControls;
class ScatterPlotWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class ScatterPlotPropertiesWidget;
}

/**
 * @brief Properties panel for Scatter Plot Widget
 */
class ScatterPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                          std::shared_ptr<DataManager> data_manager,
                                          QWidget * parent = nullptr);
    ~ScatterPlotPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the ScatterPlotWidget to connect axis range controls
     * @param plot_widget The ScatterPlotWidget instance
     */
    void setPlotWidget(ScatterPlotWidget * plot_widget);

private:
    void _updateUIFromState();

    Ui::ScatterPlotPropertiesWidget * ui;
    std::shared_ptr<ScatterPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    ScatterPlotWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;
};

#endif  // SCATTER_PLOT_PROPERTIES_WIDGET_HPP
