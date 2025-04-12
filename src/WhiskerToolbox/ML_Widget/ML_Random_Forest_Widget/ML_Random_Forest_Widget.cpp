

#include "ML_Random_Forest_Widget.hpp"
#include "ui_ML_Random_Forest_Widget.h"

#include "DataManager.hpp"

#include <fstream>
#include <iostream>

ML_Random_Forest_Widget::ML_Random_Forest_Widget(std::shared_ptr<DataManager> data_manager,
                                                 QWidget * parent)
    : _data_manager{std::move(data_manager)},
      ui(new Ui::ML_Random_Forest_Widget) {
    ui->setupUi(this);
}

ML_Random_Forest_Widget::~ML_Random_Forest_Widget() {
    delete ui;
}
