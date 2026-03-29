#include "TensorImport_Widget.hpp"
#include "ui_TensorImport_Widget.h"

#include "DataImportTypeRegistry.hpp"
#include "DataManager.hpp"

// https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "IO/formats/CSV/tensors/Tensor_Data_CSV.hpp"
#include "IO/formats/Numpy/tensordata/Tensor_Data_numpy.hpp"
#include "Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include "Tensors/CSV/CSVTensorImport_Widget.hpp"

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

#include "StateManagement/AppFileDialog.hpp"

#include <iostream>

TensorImport_Widget::TensorImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TensorImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TensorImport_Widget::_onLoaderTypeChanged);

    connect(ui->load_numpy_button, &QPushButton::clicked, this, &TensorImport_Widget::_loadNumpyArray);

    connect(ui->csv_tensor_import_widget, &CSVTensorImport_Widget::loadTensorCSVRequested,
            this, &TensorImport_Widget::_handleCSVLoadRequested);

    _onLoaderTypeChanged(0);
}

TensorImport_Widget::~TensorImport_Widget() {
    delete ui;
}

void TensorImport_Widget::_onLoaderTypeChanged(int index) {
    static_cast<void>(index);

    QString const current_text = ui->loader_type_combo->currentText();
    if (current_text == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_tensor_import_widget);
    } else {
        ui->stacked_loader_options->setCurrentWidget(ui->numpy_page);
    }
}

void TensorImport_Widget::_loadNumpyArray() {
    auto numpy_filename = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("import_numpy"),
            tr("Load Numpy Array"),
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

        //auto loaded_size = _data_manager->getData<TensorData>(tensor_key)->size();
        //std::cout << "Loaded tensor with " << loaded_size << " elements" << std::endl;

        //QMessageBox::information(this, tr("Import Successful"),
        //    tr("Loaded tensor with %1 elements into '%2'")
        //        .arg(loaded_size)
        //        .arg(QString::fromStdString(tensor_key)));

        emit importCompleted(QString::fromStdString(tensor_key), "TensorData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading numpy file: " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
                              tr("Error loading numpy file: %1").arg(QString::fromStdString(e.what())));
    }
}

void TensorImport_Widget::_handleCSVLoadRequested(const CSVTensorLoaderOptions& options) {
    auto const tensor_key = ui->data_name_text->text().toStdString();
    if (tensor_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Tensor name cannot be empty!"));
        return;
    }

    try {
        auto tensor = load(options);

        _data_manager->setData<TensorData>(tensor_key, tensor, TimeKey("time"));

        auto const num_rows = tensor->numRows();
        auto const num_cols = tensor->numColumns();
        QMessageBox::information(this, tr("Import Successful"),
                                 tr("Loaded tensor (%1 rows x %2 cols) into '%3'")
                                         .arg(num_rows)
                                         .arg(num_cols)
                                         .arg(QString::fromStdString(tensor_key)));

        emit importCompleted(QString::fromStdString(tensor_key), "TensorData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV tensor: " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
                              tr("Error loading CSV file: %1").arg(QString::fromStdString(e.what())));
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
                        }});
    }
} tensor_import_registrar;
}// namespace
