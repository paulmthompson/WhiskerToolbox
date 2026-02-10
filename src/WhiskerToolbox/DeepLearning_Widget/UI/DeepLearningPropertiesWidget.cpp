#include "DeepLearningPropertiesWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"

#include "ui_DeepLearningPropertiesWidget.h"

DeepLearningPropertiesWidget::DeepLearningPropertiesWidget(std::shared_ptr<DeepLearningState> state,
                                                           std::shared_ptr<DataManager> data_manager,
                                                           QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DeepLearningPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
}

DeepLearningPropertiesWidget::~DeepLearningPropertiesWidget() {
    delete ui;
}
