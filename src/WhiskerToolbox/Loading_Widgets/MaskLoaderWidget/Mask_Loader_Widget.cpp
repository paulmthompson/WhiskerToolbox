
#include "Mask_Loader_Widget.hpp"

#include "ui_Mask_Loader_Widget.h"

#include "../../DataManager/DataManager.hpp"
#include "../../DataManager/Masks/Mask_Data.hpp"

#include <QFileDialog>

#include <iostream>

Mask_Loader_Widget::Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Mask_Loader_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    connect(ui->load_single_hdf5_mask, &QPushButton::clicked, this, &Mask_Loader_Widget::_loadSingleHdf5Mask);
}

Mask_Loader_Widget::~Mask_Loader_Widget() {
    delete ui;
}

void Mask_Loader_Widget::_loadSingleHdf5Mask()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Mask File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    _loadSingleHDF5Mask(filename.toStdString());
}

void Mask_Loader_Widget::_loadSingleHDF5Mask(std::string filename)
{

    const auto mask_key = ui->data_name_text->toPlainText().toStdString();

    auto frames =  read_array_hdf5(filename, "frames");
    auto probs = read_ragged_hdf5(filename, "probs");
    auto y_coords = read_ragged_hdf5(filename, "heights");
    auto x_coords = read_ragged_hdf5(filename, "widths");

    _data_manager->setData<MaskData>(mask_key);

    auto mask = _data_manager->getData<MaskData>(mask_key);

    for (std::size_t i = 0; i < frames.size(); i ++) {
        mask->addMaskAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    mask->setMaskHeight(ui->height_scaling->value());
    mask->setMaskWidth(ui->width_scaling->value());

}
