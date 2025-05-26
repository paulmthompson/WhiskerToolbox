#include "HDF5MaskSaver_Widget.hpp"
#include "ui_HDF5MaskSaver_Widget.h"

HDF5MaskSaver_Widget::HDF5MaskSaver_Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HDF5MaskSaver_Widget)
{
    ui->setupUi(this);
}

HDF5MaskSaver_Widget::~HDF5MaskSaver_Widget()
{
    delete ui;
} 