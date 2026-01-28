#include "PointImport_Widget.hpp"

#include "ui_PointImport_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Points/CSV/CSVPointImport_Widget.hpp"
#include "Scaling_Widget/Scaling_Widget.hpp"
#include "DataImportTypeRegistry.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QStackedWidget>
#include <QMessageBox>

#include <iostream>

PointImport_Widget::PointImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PointImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointImport_Widget::_onLoaderTypeChanged);

    connect(ui->csv_point_import_widget, &CSVPointImport_Widget::loadSingleCSVFileRequested,
            this, &PointImport_Widget::_handleSingleCSVLoadRequested);

    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_point_import_widget);
    } else {
        _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
    }
}

PointImport_Widget::~PointImport_Widget() {
    delete ui;
}

void PointImport_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_point_import_widget);
    }
}

void PointImport_Widget::_handleSingleCSVLoadRequested(CSVPointLoaderOptions options) {
    auto keypoint_filename = QFileDialog::getOpenFileName(
            this,
            tr("Load Keypoints CSV File"),
            QDir::currentPath(),
            tr("CSV files (*.csv);;All files (*.*)"));

    if (keypoint_filename.isNull() || keypoint_filename.isEmpty()) {
        return;
    }
    options.filename = keypoint_filename.toStdString();
    _loadSingleCSVFile(options);
}

void PointImport_Widget::_loadSingleCSVFile(CSVPointLoaderOptions const & options) {
    auto const keypoint_key = ui->data_name_text->text().toStdString();
    if (keypoint_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Keypoint name cannot be empty!"));
        return;
    }

    try {
        auto keypoints = load(options);

        if (keypoints.empty()) {
            QMessageBox::warning(this, tr("Import Warning"), 
                tr("No keypoints loaded from %1. The file might be empty or in an incorrect format.")
                    .arg(QString::fromStdString(options.filename)));
            return;
        }
        std::cout << "Loaded " << keypoints.size() << " time points from " << options.filename << std::endl;

        auto point_data = std::make_shared<PointData>(keypoints);
        _data_manager->setData<PointData>(keypoint_key, point_data, TimeKey("time"));

        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        point_data->setImageSize(original_size);

        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                point_data->changeImageSize(scaled_size);
            }
        }

        QMessageBox::information(this, tr("Import Successful"),
            tr("Loaded %1 keypoints into '%2'")
                .arg(keypoints.size())
                .arg(QString::fromStdString(keypoint_key)));

        emit importCompleted(QString::fromStdString(keypoint_key), "PointData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV file " << options.filename << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading CSV file: %1").arg(QString::fromStdString(e.what())));
    }
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct PointImportRegistrar {
    PointImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "PointData",
            ImportWidgetFactory{
                .display_name = "Point Data",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new PointImport_Widget(std::move(dm), parent);
                }
            });
    }
} point_import_registrar;
}
