#include "ML_Naive_Bayes_Widget.hpp"
#include "ui_ML_Naive_Bayes_Widget.h"

#include "DataManager.hpp"
// #include "ML_Widget/MLModelParameters.hpp" // Removed redundant include

#include <fstream>
#include <iostream>

ML_Naive_Bayes_Widget::ML_Naive_Bayes_Widget(std::shared_ptr<DataManager> data_manager,
                                             QWidget * parent)
    : MLParameterWidgetBase(parent),
      _data_manager{std::move(data_manager)},
      ui(new Ui::ML_Naive_Bayes_Widget) {
    ui->setupUi(this);
}

ML_Naive_Bayes_Widget::~ML_Naive_Bayes_Widget() {
    delete ui;
}

std::unique_ptr<MLModelParametersBase> ML_Naive_Bayes_Widget::getParameters() const {
    auto params = std::make_unique<NaiveBayesParameters>();
    // params->incremental = ui->checkBox->isChecked(); // mlpack's NBC doesn't directly use this for Train
    params->epsilon = ui->doubleSpinBox->value();
    return params;
}
