#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_EventPlotWidget.h"

EventPlotWidget::EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::EventPlotWidget)
{
    ui->setupUi(this);
}

EventPlotWidget::~EventPlotWidget()
{
    delete ui;
}

void EventPlotWidget::setState(std::shared_ptr<EventPlotState> state)
{
    _state = state;
}

EventPlotState * EventPlotWidget::state()
{
    return _state.get();
}
