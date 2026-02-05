#include "ScatterPlotPropertiesWidget.hpp"

#include "Core/ScatterPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_ScatterPlotPropertiesWidget.h"

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                                          std::shared_ptr<DataManager> data_manager,
                                                          QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget()
{
    delete ui;
}
