#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Point_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    // Connect signals and slots here
}

Point_Widget::~Point_Widget() {
    delete ui;
}

void Point_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}
