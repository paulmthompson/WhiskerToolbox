
#include "Mask_Widget.hpp"
#include "ui_Mask_Widget.h"

Mask_Widget::Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Mask_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

}

Mask_Widget::~Mask_Widget() {
    delete ui;
}


