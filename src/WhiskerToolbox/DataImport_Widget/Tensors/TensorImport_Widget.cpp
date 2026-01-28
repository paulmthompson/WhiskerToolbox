#include "TensorImport_Widget.hpp"
#include "ui_TensorImport_Widget.h"

#include "DataManager.hpp"
#include "DataImportTypeRegistry.hpp"

// https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#include "DataManager/Tensors/IO/numpy/Tensor_Data_numpy.hpp"
#define slots Q_SLOTS

#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>

#include <iostream>

TensorImport_Widget::TensorImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TensorImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_numpy_button, &QPushButton::clicked, this, &TensorImport_Widget::_loadNumpyArray);
}

TensorImport_Widget::~TensorImport_Widget() {
    delete ui;
}

void TensorImport_Widget::_loadNumpyArray() {
    auto numpy_filename = QFileDialog::getOpenFileName(
            this,
            tr("Load Numpy Array"),
            QDir::currentPath(),
            tr("Numpy files (*.npy)"));

    if (numpy_filename.isNull()) {
        return;
    }

    auto const tensor_key = ui->data_name_text->text().toStdString();
    if (tensor_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Tensor name cannot be empty!"));
        return;
    }

    try {
        TensorData tensor_data;
        loadNpyToTensorData(numpy_filename.toStdString(), tensor_data);

        _data_manager->setData<TensorData>(tensor_key, std::make_shared<TensorData>(tensor_data), TimeKey("time"));

        auto loaded_size = _data_manager->getData<TensorData>(tensor_key)->size();
        std::cout << "Loaded tensor with " << loaded_size << " elements" << std::endl;

        QMessageBox::information(this, tr("Import Successful"),
            tr("Loaded tensor with %1 elements into '%2'")
                .arg(loaded_size)
                .arg(QString::fromStdString(tensor_key)));

        emit importCompleted(QString::fromStdString(tensor_key), "TensorData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading numpy file: " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading numpy file: %1").arg(QString::fromStdString(e.what())));
    }
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct TensorImportRegistrar {
    TensorImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "TensorData",
            ImportWidgetFactory{
                .display_name = "Tensor Data",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new TensorImport_Widget(std::move(dm), parent);
                }
            });
    }
} tensor_import_registrar;
}
