#include "SpectrogramPropertiesWidget.hpp"

#include "Core/SpectrogramState.hpp"
#include "DataManager/DataManager.hpp"
#include "UI/SpectrogramWidget.hpp"

#include "ui_SpectrogramPropertiesWidget.h"

SpectrogramPropertiesWidget::SpectrogramPropertiesWidget(std::shared_ptr<SpectrogramState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::SpectrogramPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr) {
    ui->setupUi(this);
}

void SpectrogramPropertiesWidget::setPlotWidget(SpectrogramWidget * plot_widget) {
    _plot_widget = plot_widget;
}

SpectrogramPropertiesWidget::~SpectrogramPropertiesWidget() {
    delete ui;
}
