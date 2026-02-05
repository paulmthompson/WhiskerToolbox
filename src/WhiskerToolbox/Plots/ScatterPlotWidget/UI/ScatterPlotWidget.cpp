#include "ScatterPlotWidget.hpp"

#include "Core/ScatterPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_ScatterPlotWidget.h"

ScatterPlotWidget::ScatterPlotWidget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::ScatterPlotWidget)
{
    ui->setupUi(this);
}

ScatterPlotWidget::~ScatterPlotWidget()
{
    delete ui;
}

void ScatterPlotWidget::setState(std::shared_ptr<ScatterPlotState> state)
{
    _state = state;
}

ScatterPlotState * ScatterPlotWidget::state()
{
    return _state.get();
}
