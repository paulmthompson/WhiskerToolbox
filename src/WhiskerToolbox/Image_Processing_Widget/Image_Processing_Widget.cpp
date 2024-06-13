
#include "Image_Processing_Widget.hpp"

#include "ui_Image_Processing_Widget.h"

#include <iostream>

Image_Processing_Widget::Image_Processing_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QMainWindow(parent),
    _data_manager{data_manager},
    ui(new Ui::Image_Processing_Widget)
{
    ui->setupUi(this);
}

void Image_Processing_Widget::openWidget() {

    std::cout << "Image Processing Widget Opened" << std::endl;

    this->show();
}
