#ifndef ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP
#define ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP

/**
 * @file OnionSkinViewPropertiesWidget.hpp
 * @brief Properties panel for the Onion Skin View Widget
 *
 * Axis range controls are provided via HorizontalAxisRangeControls and
 * VerticalAxisRangeControls in collapsible sections (set when setPlotWidget is
 * called).
 *
 * @see OnionSkinViewWidget, OnionSkinViewState,
 * OnionSkinViewWidgetRegistration
 */

#include "Core/OnionSkinViewState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HorizontalAxisRangeControls;
class OnionSkinViewWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class OnionSkinViewPropertiesWidget;
}

/**
 * @brief Properties panel for Onion Skin View Widget
 */
class OnionSkinViewPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewPropertiesWidget
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit OnionSkinViewPropertiesWidget(
        std::shared_ptr<OnionSkinViewState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~OnionSkinViewPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<OnionSkinViewState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the OnionSkinViewWidget to connect axis range controls
     * @param plot_widget The OnionSkinViewWidget instance
     */
    void setPlotWidget(OnionSkinViewWidget * plot_widget);

private:
    void _updateUIFromState();

    Ui::OnionSkinViewPropertiesWidget * ui;
    std::shared_ptr<OnionSkinViewState> _state;
    std::shared_ptr<DataManager> _data_manager;
    OnionSkinViewWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;
};

#endif  // ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP
