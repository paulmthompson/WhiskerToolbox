

#include "ML_Naive_Bayes_Widget.hpp"
#include "ui_ML_Naive_Bayes_Widget.h"

#include "DataManager.hpp"

#include <fstream>
#include <iostream>

ML_Naive_Bayes_Widget::ML_Naive_Bayes_Widget(std::shared_ptr<DataManager> data_manager,
                                                 QWidget *parent) :
    _data_manager{data_manager},
    ui(new Ui::ML_Naive_Bayes_Widget)
{
    ui->setupUi(this);

}

ML_Naive_Bayes_Widget::~ML_Naive_Bayes_Widget() {
    delete ui;
}

