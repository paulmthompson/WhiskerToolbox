#include "HDF5MaskImport_Widget.hpp"
#include "ui_HDF5MaskImport_Widget.h"

#include <QPushButton>
#include <QLineEdit>

HDF5MaskImport_Widget::HDF5MaskImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HDF5MaskImport_Widget) {
    ui->setupUi(this);
    
    // Connect single file load button
    connect(ui->load_single_hdf5_mask_button, &QPushButton::clicked,
            this, &HDF5MaskImport_Widget::loadSingleHDF5MaskRequested);
    
    // Connect multi-file load button with pattern
    connect(ui->load_multi_hdf5_mask_button, &QPushButton::clicked, this, [this]() {
        emit loadMultiHDF5MaskRequested(ui->multi_hdf5_name_pattern_text->text());
    });
}

HDF5MaskImport_Widget::~HDF5MaskImport_Widget() {
    delete ui;
}
