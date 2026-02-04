#include "LinePlotWidget.hpp"

#include "Core/LinePlotState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_LinePlotWidget.h"

LinePlotWidget::LinePlotWidget(std::shared_ptr<DataManager> data_manager,
                                QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::LinePlotWidget)
{
    ui->setupUi(this);
}

LinePlotWidget::~LinePlotWidget()
{
    delete ui;
}

void LinePlotWidget::setState(std::shared_ptr<LinePlotState> state)
{
    _state = state;
}

LinePlotState * LinePlotWidget::state()
{
    return _state.get();
}
