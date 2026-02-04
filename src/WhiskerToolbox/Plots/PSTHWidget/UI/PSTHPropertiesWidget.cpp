#include "PSTHPropertiesWidget.hpp"

#include "Core/PSTHState.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"

#include "ui_PSTHPropertiesWidget.h"

PSTHPropertiesWidget::PSTHPropertiesWidget(std::shared_ptr<PSTHState> state,
                                            std::shared_ptr<DataManager> data_manager,
                                            QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PSTHPropertiesWidget),
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

PSTHPropertiesWidget::~PSTHPropertiesWidget()
{
    delete ui;
}
