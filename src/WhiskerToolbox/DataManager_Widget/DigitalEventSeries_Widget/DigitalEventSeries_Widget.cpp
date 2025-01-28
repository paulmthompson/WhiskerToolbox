#include "DigitalEventSeries_Widget.hpp"
#include "ui_DigitalEventSeries_Widget.h"

DigitalEventSeries_Widget::DigitalEventSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DigitalEventSeries_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    // Connect signals and slots here
}

DigitalEventSeries_Widget::~DigitalEventSeries_Widget() {
    delete ui;
}

void DigitalEventSeries_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}
