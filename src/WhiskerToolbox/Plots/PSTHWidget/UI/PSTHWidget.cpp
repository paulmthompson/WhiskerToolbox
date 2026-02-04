#include "PSTHWidget.hpp"

#include "Core/PSTHState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_PSTHWidget.h"

PSTHWidget::PSTHWidget(std::shared_ptr<DataManager> data_manager,
                       QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::PSTHWidget)
{
    ui->setupUi(this);
}

PSTHWidget::~PSTHWidget()
{
    delete ui;
}

void PSTHWidget::setState(std::shared_ptr<PSTHState> state)
{
    _state = state;
}

PSTHState * PSTHWidget::state()
{
    return _state.get();
}
