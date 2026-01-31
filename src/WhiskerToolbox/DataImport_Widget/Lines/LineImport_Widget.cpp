#include "LineImport_Widget.hpp"
#include "ui_LineImport_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "Scaling_Widget/Scaling_Widget.hpp"
#include "DataImportTypeRegistry.hpp"

#include "Lines/HDF5/HDF5LineImport_Widget.hpp"
#include "Lines/CSV/CSVLineImport_Widget.hpp"
#include "Lines/Binary/BinaryLineImport_Widget.hpp"
#include "Lines/LMDB/LMDBLineImport_Widget.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QStackedWidget>
#include <QMessageBox>

#include <filesystem>
#include <iostream>
#include <regex>

LineImport_Widget::LineImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LineImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    // Connect loader type combo box
    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineImport_Widget::_onLoaderTypeChanged);

    // Connect signals from HDF5 import widget
    connect(ui->hdf5_line_import_widget, &HDF5LineImport_Widget::loadSingleHDF5LineRequested,
            this, &LineImport_Widget::_handleSingleHDF5LoadRequested);
    connect(ui->hdf5_line_import_widget, &HDF5LineImport_Widget::loadMultiHDF5LineRequested,
            this, &LineImport_Widget::_handleMultiHDF5LoadRequested);

    // Connect signals from CSV import widget
    connect(ui->csv_line_import_widget, &CSVLineImport_Widget::loadSingleFileCSVRequested,
            this, &LineImport_Widget::_handleSingleCSVLoadRequested);
    connect(ui->csv_line_import_widget, &CSVLineImport_Widget::loadMultiFileCSVRequested,
            this, &LineImport_Widget::_handleMultiCSVLoadRequested);

    // Connect signals from Binary import widget
    connect(ui->binary_line_import_widget, &BinaryLineImport_Widget::loadBinaryFileRequested,
            this, &LineImport_Widget::_handleBinaryLoadRequested);

    // Set initial state of stacked widget based on combo box
    _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
}

LineImport_Widget::~LineImport_Widget() {
    delete ui;
}

void LineImport_Widget::_onLoaderTypeChanged(int index) {
    QString current_text = ui->loader_type_combo->itemText(index);
    if (current_text == "HDF5") {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_line_import_widget);
    } else if (current_text == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_line_import_widget);
    } else if (current_text == "Binary") {
        ui->stacked_loader_options->setCurrentWidget(ui->binary_line_import_widget);
    } else if (current_text == "LMDB") {
        ui->stacked_loader_options->setCurrentWidget(ui->lmdb_line_import_widget);
    } else {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_line_import_widget);
    }
}

void LineImport_Widget::_handleSingleHDF5LoadRequested() {
    auto filename = QFileDialog::getOpenFileName(
            this,
            tr("Load Single HDF5 Line File"),
            QDir::currentPath(),
            tr("HDF5 files (*.h5 *.hdf5);;All files (*.*)"));

    if (filename.isNull() || filename.isEmpty()) {
        return;
    }
    _loadSingleHDF5LineFile(filename.toStdString());
}

void LineImport_Widget::_handleMultiHDF5LoadRequested(QString pattern) {
    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            tr("Select Directory Containing HDF5 Lines"),
            QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }
    _loadMultiHDF5LineFiles(dir_name, pattern);
}

void LineImport_Widget::_loadMultiHDF5LineFiles(QString const & dir_name, QString const & pattern_str) {
    std::filesystem::path const directory(dir_name.toStdString());
    std::vector<std::filesystem::path> line_files;
    std::string filename_pattern = pattern_str.toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5";
    }
    std::regex const regex_pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (auto const & entry : std::filesystem::directory_iterator(directory)) {
        std::string const filename = entry.path().filename().string();
        if (std::regex_match(filename, regex_pattern)) {
            line_files.push_back(entry.path());
        }
    }
    std::sort(line_files.begin(), line_files.end());

    int line_num = 0;
    for (auto const & file : line_files) {
        _loadSingleHDF5LineFile(file.string(), std::to_string(line_num));
        line_num += 1;
    }
}

void LineImport_Widget::_loadSingleHDF5LineFile(std::string const & filename, std::string const & line_suffix) {
    auto line_key = ui->data_name_text->text().toStdString();
    if (line_key.empty()) {
        line_key = std::filesystem::path(filename).stem().string();
        if (line_key.empty()) {
            line_key = "hdf5_line";
        }
    }
    if (!line_suffix.empty()) {
        line_key += "_" + line_suffix;
    }

    try {
        // Use the HDF5 loader through the plugin system
        auto & registry = LoaderRegistry::getInstance();

        if (!registry.isFormatSupported("hdf5", toIODataType(DM_DataType::Line))) {
            QMessageBox::critical(this, "Import Error", 
                "HDF5 loader not found. Please ensure the HDF5 plugin is loaded.");
            return;
        }

        // Configure HDF5 loading parameters
        nlohmann::json config;
        config["format"] = "hdf5";
        config["frame_key"] = "frames";
        config["x_key"] = "y";  // Note: x and y are swapped
        config["y_key"] = "x";

        // Add image size from scaling widget if available
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        if (original_size.width > 0 && original_size.height > 0) {
            config["image_width"] = original_size.width;
            config["image_height"] = original_size.height;
        }

        // Load the data using registry system
        auto result = registry.tryLoad("hdf5", toIODataType(DM_DataType::Line), filename, config);

        if (!result.success) {
            QMessageBox::critical(this, "Import Error", 
                QString::fromStdString("Failed to load HDF5 file: " + result.error_message));
            return;
        }

        // Extract the LineData from the variant
        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Import Error", "Unexpected data type returned from HDF5 loader");
            return;
        }

        auto line_data_ptr = std::get<std::shared_ptr<LineData>>(result.data);

        // Set image size
        line_data_ptr->setImageSize(original_size);

        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data_ptr->changeImageSize(scaled_size);
            }
        }

        // Ensure line_data has identity context
        line_data_ptr->setIdentityContext(line_key, _data_manager->getEntityRegistry());
        line_data_ptr->rebuildAllEntityIds();

        // Store in DataManager
        _data_manager->setData<LineData>(line_key, line_data_ptr, TimeKey("time"));

        QMessageBox::information(this, "Import Successful",
            QString::fromStdString("HDF5 Line data loaded into '" + line_key + "'"));

        emit importCompleted(QString::fromStdString(line_key), "LineData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading HDF5 file " << filename << ": " << e.what() << std::endl;
        QMessageBox::critical(this, "Import Error",
            QString::fromStdString("Error loading HDF5: " + std::string(e.what())));
    }
}

void LineImport_Widget::_handleSingleCSVLoadRequested(QString format, nlohmann::json config) {
    try {
        // Use LoaderRegistry for CSV loading
        LoaderRegistry & registry = LoaderRegistry::getInstance();

        if (!registry.isFormatSupported("csv", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "CSV format loading is not available.");
            return;
        }

        // Extract filepath from config
        std::string filepath = config.value("filepath", "");
        if (filepath.empty()) {
            QMessageBox::critical(this, "Import Error", "No filepath provided in CSV config.");
            return;
        }

        LoadResult result = registry.tryLoad("csv", IODataType::Line, filepath, config);

        if (!result.success) {
            QMessageBox::critical(this, "Import Error", 
                QString("Failed to load CSV file: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }

        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Import Error", "Unexpected data type returned from CSV loader.");
            return;
        }

        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Import Error", "Failed to load CSV line data.");
            return;
        }

        // Determine base key from filepath
        std::filesystem::path file_path(filepath);
        std::string base_key = file_path.stem().string();
        if (base_key.empty()) {
            base_key = "csv_single_file_line";
        }

        // Check if user has specified a data name
        if (!ui->data_name_text->text().isEmpty()) {
            base_key = ui->data_name_text->text().toStdString();
        }

        // Ensure line_data has identity context
        line_data->setIdentityContext(base_key, _data_manager->getEntityRegistry());
        line_data->rebuildAllEntityIds();

        // Set image size from scaling widget
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        line_data->setImageSize(original_size);

        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }

        _data_manager->setData<LineData>(base_key, line_data, TimeKey("time"));
        QMessageBox::information(this, "Import Successful", 
            QString("CSV line data loaded successfully as '%1'.").arg(QString::fromStdString(base_key)));
        
        emit importCompleted(QString::fromStdString(base_key), "LineData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading single CSV file: " << e.what() << std::endl;
        QMessageBox::critical(this, "Import Error", 
            QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void LineImport_Widget::_handleMultiCSVLoadRequested(QString format, nlohmann::json config) {
    try {
        // Use LoaderRegistry for CSV loading
        LoaderRegistry & registry = LoaderRegistry::getInstance();

        if (!registry.isFormatSupported("csv", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "CSV format loading is not available.");
            return;
        }

        // Extract parent_dir from config
        std::string parent_dir = config.value("parent_dir", "");
        if (parent_dir.empty()) {
            QMessageBox::critical(this, "Import Error", "No parent directory provided in CSV config.");
            return;
        }

        LoadResult result = registry.tryLoad("csv", IODataType::Line, parent_dir, config);

        if (!result.success) {
            QMessageBox::critical(this, "Import Error", 
                QString("Failed to load CSV files: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }

        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Import Error", "Unexpected data type returned from CSV loader.");
            return;
        }

        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Import Error", "Failed to load CSV line data.");
            return;
        }

        // Determine base key from parent directory
        std::filesystem::path parent_path(parent_dir);
        std::string base_key = parent_path.filename().string();
        if (base_key.empty() || base_key == "." || base_key == "..") {
            base_key = "csv_multi_file_line";
        }

        // Check if user has specified a data name
        if (!ui->data_name_text->text().isEmpty()) {
            base_key = ui->data_name_text->text().toStdString();
        }

        // Ensure line_data has identity context
        line_data->setIdentityContext(base_key, _data_manager->getEntityRegistry());
        line_data->rebuildAllEntityIds();

        // Set image size from scaling widget
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        line_data->setImageSize(original_size);

        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }

        _data_manager->setData<LineData>(base_key, line_data, TimeKey("time"));
        QMessageBox::information(this, "Import Successful", 
            QString("CSV line data loaded successfully as '%1'.").arg(QString::fromStdString(base_key)));
        
        emit importCompleted(QString::fromStdString(base_key), "LineData");

    } catch (std::exception const & e) {
        std::cerr << "Error loading multi CSV files: " << e.what() << std::endl;
        QMessageBox::critical(this, "Import Error", 
            QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void LineImport_Widget::_handleBinaryLoadRequested(QString filepath) {
    if (filepath.isNull() || filepath.isEmpty()) {
        return;
    }
    _loadSingleBinaryFile(filepath);
}

void LineImport_Widget::_loadSingleBinaryFile(QString const & filepath) {
    std::string const file_path_std = filepath.toStdString();
    auto line_key = ui->data_name_text->text().toStdString();

    if (line_key.empty()) {
        line_key = std::filesystem::path(file_path_std).stem().string();
        if (line_key.empty()) {
            line_key = "binary_line_data";
        }
    }

    try {
        // Use LoaderRegistry for binary loading
        LoaderRegistry & registry = LoaderRegistry::getInstance();

        if (!registry.isFormatSupported("binary", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "Binary format loading is not available. This may require CapnProto to be enabled at build time.");
            return;
        }

        // Create JSON config for binary loading
        nlohmann::json config;
        config["file_path"] = file_path_std;

        LoadResult result = registry.tryLoad("binary", IODataType::Line, file_path_std, config);

        if (!result.success) {
            QMessageBox::critical(this, "Import Error", 
                QString("Failed to load binary line data: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }

        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Import Error", "Unexpected data type returned from binary loader.");
            return;
        }

        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Import Error", "Failed to load binary line data.");
            return;
        }

        // Ensure line_data has identity context
        line_data->setIdentityContext(line_key, _data_manager->getEntityRegistry());
        line_data->rebuildAllEntityIds();

        // Set image size from scaling widget
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        line_data->setImageSize(original_size);

        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }

        _data_manager->setData<LineData>(line_key, line_data, TimeKey("time"));
        
        QMessageBox::information(this, "Import Successful", 
            QString::fromStdString("Binary Line data loaded into " + line_key));

        emit importCompleted(QString::fromStdString(line_key), "LineData");

    } catch (std::exception const & e) {
        std::cerr << "Failed to load binary line data: " << e.what() << std::endl;
        QMessageBox::critical(this, "Import Error", 
            QString("Could not load binary line data: %1").arg(e.what()));
    }
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct LineImportRegistrar {
    LineImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "LineData",
            ImportWidgetFactory{
                .display_name = "Line Data",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new LineImport_Widget(std::move(dm), parent);
                }
            });
    }
} line_import_registrar;
}
