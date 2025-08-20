#include "Line_Loader_Widget.hpp"

#include "ui_Line_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "DataManager/DataManagerTypes.hpp"
// Remove direct IO includes - use registry pattern instead
#include "IO_Widgets/Lines/HDF5/HDF5LineLoader_Widget.hpp"
#include "IO_Widgets/Lines/CSV/CSVLineLoader_Widget.hpp"
#include "IO_Widgets/Lines/LMDB/LMDBLineLoader_Widget.hpp"
#include "IO_Widgets/Lines/Binary/BinaryLineLoader_Widget.hpp"
#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QFileDialog>
#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>

#include <filesystem>
#include <iostream>
#include <regex>

Line_Loader_Widget::Line_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Line_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->hdf5_line_loader, &HDF5LineLoader_Widget::newHDF5Filename, this, &Line_Loader_Widget::_loadSingleHdf5Line);
    connect(ui->hdf5_line_loader, &HDF5LineLoader_Widget::newHDF5MultiFilename, this, &Line_Loader_Widget::_loadMultiHdf5Line);
    
    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Line_Loader_Widget::_onLoaderTypeChanged);

    connect(ui->binary_line_loader, &BinaryLineLoader_Widget::loadBinaryFileRequested,
            this, &Line_Loader_Widget::_handleLoadBinaryFileRequested);

    // Connect CSV loader signals
    connect(ui->csv_line_loader, &CSVLineLoader_Widget::loadSingleFileCSVRequested,
            this, &Line_Loader_Widget::_handleLoadSingleFileCSVRequested);
    connect(ui->csv_line_loader, &CSVLineLoader_Widget::loadMultiFileCSVRequested,
            this, &Line_Loader_Widget::_handleLoadMultiFileCSVRequested);

    _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
}

Line_Loader_Widget::~Line_Loader_Widget() {
    delete ui;
}

void Line_Loader_Widget::_onLoaderTypeChanged(int index) {
    QString current_text = ui->loader_type_combo->itemText(index);
    if (current_text == "HDF5") {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_line_loader);
    } else if (current_text == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_line_loader);
    } else if (current_text == "LMDB") {
        ui->stacked_loader_options->setCurrentWidget(ui->lmdb_line_loader);
    } else if (current_text == "Binary") {
        ui->stacked_loader_options->setCurrentWidget(ui->binary_line_loader);
    } else {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_line_loader);
    }
}

void Line_Loader_Widget::_handleLoadBinaryFileRequested(QString filepath) {
    if (filepath.isNull() || filepath.isEmpty()) {
        return;
    }
    _loadSingleBinaryFile(filepath);
}

void Line_Loader_Widget::_loadSingleBinaryFile(QString const & filepath) {
    std::string const file_path_std = filepath.toStdString();
    auto line_key = ui->data_name_text->text().toStdString();

    if (line_key.empty()) {
        line_key = std::filesystem::path(file_path_std).stem().string();
        if (line_key.empty()) {
            line_key = "binary_line_data";
        }
    }

    try {
        // Use LoaderRegistry instead of direct binary loading
        LoaderRegistry& registry = LoaderRegistry::getInstance();
        
        if (!registry.isFormatSupported("binary", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "Binary format loading is not available. This may require CapnProto to be enabled at build time.");
            return;
        }
        
        // Create JSON config for binary loading
        nlohmann::json config;
        config["file_path"] = file_path_std;
        
        ConcreteDataFactory factory;
        LoadResult result = registry.tryLoad("binary", IODataType::Line, file_path_std, config, &factory);
        
        if (!result.success) {
            QMessageBox::critical(this, "Load Error", QString("Failed to load binary line data: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }
        
        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Load Error", "Unexpected data type returned from binary loader.");
            return;
        }
        
        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Load Error", "Failed to load binary line data.");
            return;
        }

        // Ensure line_data has identity context
        line_data->setIdentityContext(line_key, _data_manager->getEntityRegistry());
        line_data->rebuildAllEntityIds();

        _data_manager->setData<LineData>(line_key, line_data, TimeKey("time"));
        std::cout << "Successfully loaded binary line data: " << file_path_std << " into key: " << line_key << std::endl;

        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        line_data->setImageSize(original_size);

        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }        
        QMessageBox::information(this, "Load Successful", QString::fromStdString("Binary Line data loaded into " + line_key));
        
    } catch (std::exception const & e) {
        std::cerr << "Failed to load binary line data: " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Failed", QString("Could not load binary line data: %1").arg(e.what()));
    }
}

void Line_Loader_Widget::_loadSingleHdf5Line(QString filename) {

    if (filename.isNull()) {
        return;
    }

    _loadSingleHDF5Line(filename.toStdString());
}

void Line_Loader_Widget::_loadMultiHdf5Line(QString dir_name, QString pattern) {

    if (dir_name.isEmpty()) {
        return;
    }

    std::filesystem::path const directory(dir_name.toStdString());

    std::vector<std::filesystem::path> line_files;
    std::string filename_pattern = pattern.toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5";
    }
    std::regex const regex_pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (auto const & entry: std::filesystem::directory_iterator(directory)) {
        std::string const entry_filename = entry.path().filename().string();
        if (std::regex_match(entry_filename, regex_pattern)) {
            line_files.push_back(entry.path());
        } else {
            // Optional: std::cout << "File " << entry_filename << " does not match pattern " << filename_pattern << std::endl;
        }
    }

    std::sort(line_files.begin(), line_files.end());

    int line_num = 0;
    for (auto const & file: line_files) {
        _loadSingleHDF5Line(file.string(), std::to_string(line_num));
        line_num += 1;
    }
}

void Line_Loader_Widget::_loadSingleHDF5Line(std::string const & filename, std::string const & line_suffix) {

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
        // Use the new HDF5 loader through the plugin system
        auto& registry = LoaderRegistry::getInstance();
        
        if (!registry.isFormatSupported("hdf5", toIODataType(DM_DataType::Line))) {
            QMessageBox::critical(this, "Load Error", "HDF5 loader not found. Please ensure the HDF5 plugin is loaded.");
            return;
        }
        
        // Create factory for data creation
        ConcreteDataFactory factory;
        
        // Configure HDF5 loading parameters
        nlohmann::json config;
        config["format"] = "hdf5";  // Required by new system
        config["frame_key"] = "frames";
        config["x_key"] = "y";  // Note: x and y are swapped in the original implementation
        config["y_key"] = "x";
        
        // Add image size from scaling widget if available
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        if (original_size.width > 0 && original_size.height > 0) {
            config["image_width"] = original_size.width;
            config["image_height"] = original_size.height;
        }
        
        // Load the data using new registry system
        auto result = registry.tryLoad("hdf5", toIODataType(DM_DataType::Line), filename, config, &factory);
        
        if (!result.success) {
            QMessageBox::critical(this, "Load Error", QString::fromStdString("Failed to load HDF5 file: " + result.error_message));
            return;
        }
        
        // Extract the LineData from the variant
        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Load Error", "Unexpected data type returned from HDF5 loader");
            return;
        }
        
        auto line_data_ptr = std::get<std::shared_ptr<LineData>>(result.data);
        
        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data_ptr->changeImageSize(scaled_size);
            }
        }
        
        // Store in DataManager
        _data_manager->setData<LineData>(line_key, TimeKey("time"));
        auto data_manager_line_data = _data_manager->getData<LineData>(line_key);
        
        // Copy the loaded data to the DataManager's LineData
        for (auto const& time : line_data_ptr->getTimesWithData()) {
            auto const& lines = line_data_ptr->getAtTime(time);
            for (auto const& line : lines) {
                data_manager_line_data->addAtTime(time, line, false);
            }
        }
        
        // Set image size
        data_manager_line_data->setImageSize(line_data_ptr->getImageSize());
        
        QMessageBox::information(this, "Load Successful", QString::fromStdString("HDF5 Line data loaded into " + line_key));

    } catch (std::exception const& e) {
        std::cerr << "Error loading HDF5 file " << filename << ": " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading HDF5: " + std::string(e.what())));
    }
}

void Line_Loader_Widget::_handleLoadSingleFileCSVRequested(QString format, nlohmann::json config) {
    try {
        // Use LoaderRegistry instead of direct CSV loading
        LoaderRegistry& registry = LoaderRegistry::getInstance();
        
        if (!registry.isFormatSupported("csv", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "CSV format loading is not available. This should not happen as CSV is an internal loader.");
            return;
        }
        
        // Extract filepath from config for determining key
        std::string filepath = config.value("filepath", "");
        if (filepath.empty()) {
            QMessageBox::critical(this, "Load Error", "No filepath provided in CSV config.");
            return;
        }
        
        ConcreteDataFactory factory;
        LoadResult result = registry.tryLoad("csv", IODataType::Line, filepath, config, &factory);
        
        if (!result.success) {
            QMessageBox::critical(this, "Load Error", QString("Failed to load CSV file: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }
        
        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Load Error", "Unexpected data type returned from CSV loader.");
            return;
        }
        
        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Load Error", "Failed to load CSV line data.");
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
        
        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }
        
        _data_manager->setData<LineData>(base_key, line_data, TimeKey("time"));
        QMessageBox::information(this, "Load Successful", QString("CSV line data loaded successfully as '%1'.").arg(QString::fromStdString(base_key)));
        std::cout << "CSV line data loaded: " << base_key << std::endl;
        
    } catch (std::exception const& e) {
        std::cerr << "Error loading single CSV file: " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void Line_Loader_Widget::_handleLoadMultiFileCSVRequested(QString format, nlohmann::json config) {
    try {
        // Use LoaderRegistry instead of direct CSV loading
        LoaderRegistry& registry = LoaderRegistry::getInstance();
        
        if (!registry.isFormatSupported("csv", IODataType::Line)) {
            QMessageBox::warning(this, "Format Not Supported", 
                "CSV format loading is not available. This should not happen as CSV is an internal loader.");
            return;
        }
        
        // Extract parent_dir from config for determining key
        std::string parent_dir = config.value("parent_dir", "");
        if (parent_dir.empty()) {
            QMessageBox::critical(this, "Load Error", "No parent directory provided in CSV config.");
            return;
        }
        
        ConcreteDataFactory factory;
        LoadResult result = registry.tryLoad("csv", IODataType::Line, parent_dir, config, &factory);
        
        if (!result.success) {
            QMessageBox::critical(this, "Load Error", QString("Failed to load CSV files: %1").arg(QString::fromStdString(result.error_message)));
            return;
        }
        
        if (!std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
            QMessageBox::critical(this, "Load Error", "Unexpected data type returned from CSV loader.");
            return;
        }
        
        auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
        if (!line_data) {
            QMessageBox::critical(this, "Load Error", "Failed to load CSV line data.");
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
        
        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data->changeImageSize(scaled_size);
            }
        }
        
        _data_manager->setData<LineData>(base_key, line_data, TimeKey("time"));
        QMessageBox::information(this, "Load Successful", QString("CSV line data loaded successfully as '%1'.").arg(QString::fromStdString(base_key)));
        std::cout << "Multi-file CSV line data loaded: " << base_key << std::endl;
        
    } catch (std::exception const& e) {
        std::cerr << "Error loading multi CSV files: " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void Line_Loader_Widget::_loadCSVData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data_map, std::string const & base_key) {
    // Get the line key from the UI, or use the base key
    auto line_key = ui->data_name_text->text().toStdString();
    if (line_key.empty()) {
        line_key = base_key;
    }
    
    // Create a new LineData object in the data manager
    _data_manager->setData<LineData>(line_key, TimeKey("time"));
    auto line_data_ptr = _data_manager->getData<LineData>(line_key);
    
    // Add all the loaded data to the LineData object
    int total_lines = 0;
    for (auto const & [time, lines] : data_map) {
        for (auto const & line : lines) {
            line_data_ptr->addAtTime(time, line, false); // Don't notify for each line
            total_lines++;
        }
    }
    
    // Set up scaling
    ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
    line_data_ptr->setImageSize(original_size);
    
    if (ui->scaling_widget->isScalingEnabled()) {
        ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
        if (scaled_size.width > 0 && scaled_size.height > 0) {
            line_data_ptr->changeImageSize(scaled_size);
        }
    }
    
    // Notify observers once at the end
    line_data_ptr->notifyObservers();
    
    std::cout << "Successfully loaded " << total_lines << " lines from CSV data into key: " << line_key << std::endl;
    QMessageBox::information(this, "Load Successful", 
                            QString::fromStdString("CSV Line data loaded into " + line_key + 
                                                  " (" + std::to_string(total_lines) + " lines, " + 
                                                  std::to_string(data_map.size()) + " timestamps)"));
}
