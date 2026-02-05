#include "HeatmapPropertiesWidget.hpp"

#include "Core/HeatmapState.hpp"

#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_HeatmapPropertiesWidget.h"

HeatmapPropertiesWidget::HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                                  std::shared_ptr<DataManager> data_manager,
                                                  QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HeatmapPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);

    // Create and add PlotAlignmentWidget
    _alignment_widget = new PlotAlignmentWidget(_state->alignmentState(), _data_manager, this);
    // Replace the placeholder widget with the alignment widget
    int alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);
}

HeatmapPropertiesWidget::~HeatmapPropertiesWidget()
{
    delete ui;
}
