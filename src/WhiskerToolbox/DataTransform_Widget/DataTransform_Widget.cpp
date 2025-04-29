

#include "DataTransform_Widget.hpp"
#include "ui_DataTransform_Widget.h"

#include "DataManager.hpp"


DataTransform_Widget::DataTransform_Widget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataTransform_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);
}

DataTransform_Widget::~DataTransform_Widget() {
    delete ui;
}

void DataTransform_Widget::openWidget() {
}
