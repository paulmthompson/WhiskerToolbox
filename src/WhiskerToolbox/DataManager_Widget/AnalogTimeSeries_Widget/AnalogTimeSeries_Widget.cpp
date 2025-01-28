#include "AnalogTimeSeries_Widget.hpp"
#include "ui_AnalogTimeSeries_Widget.h"

AnalogTimeSeries_Widget::AnalogTimeSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AnalogTimeSeries_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    // Connect signals and slots here
}

AnalogTimeSeries_Widget::~AnalogTimeSeries_Widget() {
    delete ui;
}

void AnalogTimeSeries_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}
