#include "EventPlotPropertiesWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_EventPlotPropertiesWidget.h"

EventPlotPropertiesWidget::EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget()
{
    delete ui;
}
