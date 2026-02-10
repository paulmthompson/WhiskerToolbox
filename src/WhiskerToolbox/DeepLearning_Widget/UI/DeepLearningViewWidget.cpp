#include "DeepLearningViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"

#include "ui_DeepLearningViewWidget.h"

DeepLearningViewWidget::DeepLearningViewWidget(std::shared_ptr<DeepLearningState> state,
                                               std::shared_ptr<DataManager> data_manager,
                                               QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DeepLearningViewWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
}

DeepLearningViewWidget::~DeepLearningViewWidget() {
    delete ui;
}
