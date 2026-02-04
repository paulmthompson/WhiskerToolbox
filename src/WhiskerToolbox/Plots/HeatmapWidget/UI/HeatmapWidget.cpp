#include "HeatmapWidget.hpp"

#include "Core/HeatmapState.hpp"
#include "DataManager/DataManager.hpp"

#include <QVBoxLayout>

#include "ui_HeatmapWidget.h"

HeatmapWidget::HeatmapWidget(std::shared_ptr<DataManager> data_manager,
                             QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::HeatmapWidget)
{
    ui->setupUi(this);
}

HeatmapWidget::~HeatmapWidget()
{
    delete ui;
}

void HeatmapWidget::setState(std::shared_ptr<HeatmapState> state)
{
    _state = state;
}

HeatmapState * HeatmapWidget::state()
{
    return _state.get();
}
