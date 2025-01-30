#include "Tensor_Loader_Widget.hpp"
#include "ui_Tensor_Loader_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QFileDialog>
#include <QPushButton>

#include <iostream>

Tensor_Loader_Widget::Tensor_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Tensor_Loader_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    connect(ui->load_numpy_button, &QPushButton::clicked, this, &Tensor_Loader_Widget::_loadNumpyArray);
}

Tensor_Loader_Widget::~Tensor_Loader_Widget() {
    delete ui;
}

void Tensor_Loader_Widget::_loadNumpyArray()
{
    auto numpy_filename = QFileDialog::getOpenFileName(
        this,
        "Load Numpy Array",
        QDir::currentPath(),
        "Numpy files (*.npy)");

    if (numpy_filename.isNull()) {
        return;
    }

    const auto tensor_key = ui->data_name_text->toPlainText().toStdString();

    TensorData tensor_data;
    loadNpyToTensorData(numpy_filename.toStdString(), tensor_data);

    _data_manager->setData<TensorData>(tensor_key, std::make_shared<TensorData>(tensor_data));

    std::cout << "Loaded tensor with " << _data_manager->getData<TensorData>(tensor_key)->size() << " elements" << std::endl;
}
