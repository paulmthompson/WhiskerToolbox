#include "DigitalIntervalSeries_Widget.hpp"
#include "ui_DigitalIntervalSeries_Widget.h"

#include "DataManager_Widget/DataManager_Widget.hpp"
#include "DataManager.hpp"

#include <QPushButton>

#include <filesystem>
#include <iostream>

DigitalIntervalSeries_Widget::DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DigitalIntervalSeries_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    connect(ui->save_csv, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_saveCSV);
}

DigitalIntervalSeries_Widget::~DigitalIntervalSeries_Widget() {
    delete ui;
}

void DigitalIntervalSeries_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();

}

void DigitalIntervalSeries_Widget::_saveCSV()
{
    auto output_path = _data_manager->getOutputPath();
    std::cout << output_path.string() << std::endl;
}
