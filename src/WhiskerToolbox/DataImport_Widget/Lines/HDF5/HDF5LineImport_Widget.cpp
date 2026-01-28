#include "HDF5LineImport_Widget.hpp"
#include "ui_HDF5LineImport_Widget.h"

#include <QPushButton>
#include <QLineEdit>

HDF5LineImport_Widget::HDF5LineImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HDF5LineImport_Widget) {
    ui->setupUi(this);
    
    // Connect single file load button
    connect(ui->load_single_hdf5_line_button, &QPushButton::clicked,
            this, &HDF5LineImport_Widget::loadSingleHDF5LineRequested);
    
    // Connect multi-file load button with pattern
    connect(ui->load_multi_hdf5_line_button, &QPushButton::clicked, this, [this]() {
        emit loadMultiHDF5LineRequested(ui->multi_filename_pattern_text->text());
    });
}

HDF5LineImport_Widget::~HDF5LineImport_Widget() {
    delete ui;
}
