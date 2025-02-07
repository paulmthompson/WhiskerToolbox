
#include "Export_Video_Widget.hpp"

#include "ui_Export_Video_Widget.h"

#include "../../DataManager/DataManager.hpp"
#include "../../DataManager/Lines/Line_Data.hpp"

#include <QFileDialog>

#include <filesystem>
#include <iostream>
#include <regex>

Export_Video_Widget::Export_Video_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Export_Video_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);


}

Export_Video_Widget::~Export_Video_Widget() {
    delete ui;
}

void Export_Video_Widget::openWidget()
{
    this->show();
}
