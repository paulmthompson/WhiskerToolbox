#include "HDF5MaskLoader_Widget.hpp"
#include "ui_HDF5MaskLoader_Widget.h"

HDF5MaskLoader_Widget::HDF5MaskLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HDF5MaskLoader_Widget)
{
    ui->setupUi(this);
    connect(ui->load_single_hdf5_mask_button, &QPushButton::clicked,
            this, &HDF5MaskLoader_Widget::loadSingleHDF5MaskRequested);
    connect(ui->load_multi_hdf5_mask_button, &QPushButton::clicked, this, [this]() {
        emit loadMultiHDF5MaskRequested(ui->multi_hdf5_name_pattern_text->text());
    });
}

HDF5MaskLoader_Widget::~HDF5MaskLoader_Widget()
{
    delete ui;
} 