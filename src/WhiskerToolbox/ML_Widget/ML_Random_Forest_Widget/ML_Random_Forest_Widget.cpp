#include "ML_Random_Forest_Widget.hpp"
#include "ui_ML_Random_Forest_Widget.h"

#include "DataManager.hpp"

#include <fstream>
#include <iostream>

ML_Random_Forest_Widget::ML_Random_Forest_Widget(std::shared_ptr<DataManager> data_manager,
                                                 QWidget * parent)
    : MLParameterWidgetBase(parent),
      _data_manager{std::move(data_manager)},
      ui(new Ui::ML_Random_Forest_Widget) {
    ui->setupUi(this);
}

ML_Random_Forest_Widget::~ML_Random_Forest_Widget() {
    delete ui;
}

std::unique_ptr<MLModelParametersBase> ML_Random_Forest_Widget::getParameters() const {
    auto params = std::make_unique<RandomForestParameters>();
    params->numTrees = ui->spinBox->value();         // Corresponds to numTrees in UI
    params->minimumLeafSize = ui->spinBox_2->value(); // Corresponds to minLeafSize in UI
    params->minimumGainSplit = ui->doubleSpinBox->value(); // Corresponds to minGainSplit in UI
    params->maximumDepth = ui->spinBox_3->value();    // Corresponds to maxDepth in UI
    // params->warmStart = ui->checkBox->isChecked(); // mlpack RF doesn't have direct warm_start
    return params;
}
