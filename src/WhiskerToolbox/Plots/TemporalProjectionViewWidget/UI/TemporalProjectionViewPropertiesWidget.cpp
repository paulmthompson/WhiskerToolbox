#include "TemporalProjectionViewPropertiesWidget.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_TemporalProjectionViewPropertiesWidget.h"

TemporalProjectionViewPropertiesWidget::TemporalProjectionViewPropertiesWidget(
    std::shared_ptr<TemporalProjectionViewState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TemporalProjectionViewPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);
}

TemporalProjectionViewPropertiesWidget::~TemporalProjectionViewPropertiesWidget()
{
    delete ui;
}
