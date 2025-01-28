
#include "Line_Widget.hpp"
#include "ui_Line_Widget.h"

Line_Widget::Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Line_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    // Connect signals and slots here
}

Line_Widget::~Line_Widget() {
    delete ui;
}

void Line_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}
