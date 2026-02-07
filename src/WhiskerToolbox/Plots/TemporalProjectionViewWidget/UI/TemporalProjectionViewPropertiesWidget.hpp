#ifndef TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP

/**
 * @file TemporalProjectionViewPropertiesWidget.hpp
 * @brief Properties panel for the Temporal Projection View Widget
 *
 * Axis range controls are provided via HorizontalAxisRangeControls and
 * VerticalAxisRangeControls in collapsible sections (set when setPlotWidget is
 * called).
 *
 * @see TemporalProjectionViewWidget, TemporalProjectionViewState,
 * TemporalProjectionViewWidgetRegistration
 */

#include "Core/TemporalProjectionViewState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HorizontalAxisRangeControls;
class TemporalProjectionViewWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class TemporalProjectionViewPropertiesWidget;
}

/**
 * @brief Properties panel for Temporal Projection View Widget
 */
class TemporalProjectionViewPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewPropertiesWidget
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit TemporalProjectionViewPropertiesWidget(
        std::shared_ptr<TemporalProjectionViewState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~TemporalProjectionViewPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the TemporalProjectionViewWidget to connect axis range controls
     * @param plot_widget The TemporalProjectionViewWidget instance
     */
    void setPlotWidget(TemporalProjectionViewWidget * plot_widget);

private:
    void _updateUIFromState();

    Ui::TemporalProjectionViewPropertiesWidget * ui;
    std::shared_ptr<TemporalProjectionViewState> _state;
    std::shared_ptr<DataManager> _data_manager;
    TemporalProjectionViewWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;
};

#endif  // TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
