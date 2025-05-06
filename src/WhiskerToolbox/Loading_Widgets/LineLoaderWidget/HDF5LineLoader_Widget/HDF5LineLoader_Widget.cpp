
#include "HDF5LineLoader_Widget.hpp"

#include "ui_HDF5LineLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>

HDF5LineLoader_Widget::HDF5LineLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HDF5LineLoader_Widget){
    ui->setupUi(this);

    connect(ui->load_single_hdf5_line, &QPushButton::clicked, this, &HDF5LineLoader_Widget::_loadSingleHdf5Line);
}

HDF5LineLoader_Widget::~HDF5LineLoader_Widget() {
    delete ui;
}

void HDF5LineLoader_Widget::_loadSingleHdf5Line() {
    auto filename = QFileDialog::getOpenFileName(
            this,
            "Load Line File",
            QDir::currentPath(),
            "All files (*.*)");

    emit newHDF5Filename(filename);
}

void HDF5LineLoader_Widget::_loadMultiHdf5Line() {
    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            "Select Directory",
            QDir::currentPath());

    auto pattern = ui->multi_filename_pattern->text();

    emit newHDF5MultiFilename(dir_name, pattern);
}

