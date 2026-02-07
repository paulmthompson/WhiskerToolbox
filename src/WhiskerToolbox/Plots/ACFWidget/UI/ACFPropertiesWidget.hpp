#ifndef ACF_PROPERTIES_WIDGET_HPP
#define ACF_PROPERTIES_WIDGET_HPP

/**
 * @file ACFPropertiesWidget.hpp
 * @brief Properties panel for the ACF Widget
 *
 * Axis range controls are provided via HorizontalAxisRangeControls and
 * VerticalAxisRangeControls in collapsible sections (set when setPlotWidget
 * is called).
 *
 * @see ACFWidget, ACFState, ACFWidgetRegistration
 */

#include "Core/ACFState.hpp"

#include <QWidget>

#include <memory>

class ACFWidget;
class DataManager;
class HorizontalAxisRangeControls;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class ACFPropertiesWidget;
}

/**
 * @brief Properties panel for ACF Widget
 */
class ACFPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit ACFPropertiesWidget(std::shared_ptr<ACFState> state,
                                  std::shared_ptr<DataManager> data_manager,
                                  QWidget * parent = nullptr);

    ~ACFPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<ACFState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the ACFWidget to connect axis range controls
     * @param plot_widget The ACFWidget instance
     */
    void setPlotWidget(ACFWidget * plot_widget);

private slots:
    void _onEventKeyComboChanged(int index);

private:
    void _populateEventKeyComboBox();
    void _updateUIFromState();

    Ui::ACFPropertiesWidget * ui;
    std::shared_ptr<ACFState> _state;
    std::shared_ptr<DataManager> _data_manager;
    ACFWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;

    int _dm_observer_id = -1;
};

#endif  // ACF_PROPERTIES_WIDGET_HPP
