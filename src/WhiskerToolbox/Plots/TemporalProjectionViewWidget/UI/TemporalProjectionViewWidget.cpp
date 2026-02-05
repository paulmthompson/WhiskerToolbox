#include "TemporalProjectionViewWidget.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_TemporalProjectionViewWidget.h"

TemporalProjectionViewWidget::TemporalProjectionViewWidget(std::shared_ptr<DataManager> data_manager,
                                                            QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::TemporalProjectionViewWidget)
{
    ui->setupUi(this);
}

TemporalProjectionViewWidget::~TemporalProjectionViewWidget()
{
    delete ui;
}

void TemporalProjectionViewWidget::setState(std::shared_ptr<TemporalProjectionViewState> state)
{
    _state = state;
}

TemporalProjectionViewState * TemporalProjectionViewWidget::state()
{
    return _state.get();
}
