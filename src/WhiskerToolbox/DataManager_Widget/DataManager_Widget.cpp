
#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"


DataManager_Widget::DataManager_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataManager_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);


}

DataManager_Widget::~DataManager_Widget() {
    delete ui;
}
