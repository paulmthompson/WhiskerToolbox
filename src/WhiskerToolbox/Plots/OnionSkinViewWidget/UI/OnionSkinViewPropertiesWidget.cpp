#include "OnionSkinViewPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/OnionSkinViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/OnionSkinViewWidget.hpp"

#include "ui_OnionSkinViewPropertiesWidget.h"

OnionSkinViewPropertiesWidget::OnionSkinViewPropertiesWidget(
    std::shared_ptr<OnionSkinViewState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::OnionSkinViewPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr),
      _horizontal_range_controls(nullptr),
      _horizontal_range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr)
{
    ui->setupUi(this);
    if (_state) {
        _updateUIFromState();
    }
}

OnionSkinViewPropertiesWidget::~OnionSkinViewPropertiesWidget()
{
    delete ui;
}

void OnionSkinViewPropertiesWidget::setPlotWidget(OnionSkinViewWidget * plot_widget)
{
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * horizontal_axis_state = _state->horizontalAxisState();
    if (horizontal_axis_state) {
        _horizontal_range_controls_section = new Section(this, "X-Axis Range Controls");
        _horizontal_range_controls =
            new HorizontalAxisRangeControls(horizontal_axis_state, _horizontal_range_controls_section);
        _horizontal_range_controls_section->autoSetContentLayout();
        ui->main_layout->insertWidget(0, _horizontal_range_controls_section);
    }

    auto * vertical_axis_state = _state->verticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls =
            new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _horizontal_range_controls_section
                               ? ui->main_layout->indexOf(_horizontal_range_controls_section) + 1
                               : 0;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

void OnionSkinViewPropertiesWidget::_updateUIFromState()
{
    (void)_state;
}
