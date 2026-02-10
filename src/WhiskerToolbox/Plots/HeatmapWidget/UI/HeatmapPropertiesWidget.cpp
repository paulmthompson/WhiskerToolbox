#include "HeatmapPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/HeatmapState.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/HeatmapWidget.hpp"

#include "ui_HeatmapPropertiesWidget.h"

HeatmapPropertiesWidget::HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                                 std::shared_ptr<DataManager> data_manager,
                                                 QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HeatmapPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _alignment_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr)
{
    ui->setupUi(this);

    // Create and add PlotAlignmentWidget
    _alignment_widget = new PlotAlignmentWidget(_state->alignmentState(), _data_manager, this);
    int alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);
}

HeatmapPropertiesWidget::~HeatmapPropertiesWidget()
{
    delete ui;
}

void HeatmapPropertiesWidget::setPlotWidget(HeatmapWidget * plot_widget)
{
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * time_axis_state = _state->relativeTimeAxisState();
    if (time_axis_state) {
        _range_controls_section = new Section(this, "Time Axis Range Controls");
        _range_controls =
            new RelativeTimeAxisRangeControls(time_axis_state, _range_controls_section);
        _range_controls_section->autoSetContentLayout();
        int insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _range_controls_section);
    }

    auto * vertical_axis_state = _plot_widget->getVerticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls =
            new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _range_controls_section
                               ? ui->main_layout->indexOf(_range_controls_section) + 1
                               : ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}
