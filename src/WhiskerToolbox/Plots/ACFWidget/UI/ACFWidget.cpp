#include "ACFWidget.hpp"

#include "Core/ACFState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_ACFWidget.h"

ACFWidget::ACFWidget(std::shared_ptr<DataManager> data_manager,
                     QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::ACFWidget)
{
    ui->setupUi(this);
}

ACFWidget::~ACFWidget()
{
    delete ui;
}

void ACFWidget::setState(std::shared_ptr<ACFState> state)
{
    _state = state;
}

ACFState * ACFWidget::state()
{
    return _state.get();
}
