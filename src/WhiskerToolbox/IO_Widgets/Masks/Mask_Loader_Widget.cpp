#include "Mask_Loader_Widget.hpp"
#include "ui_Mask_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/IO/LoaderRegistry.hpp"
#include "DataManager/ConcreteDataFactory.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DataManager/Masks/IO/Image/Mask_Data_Image.hpp"
#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QFileDialog>
#include <QStackedWidget> // Required for ui->stacked_loader_options
#include <QComboBox> // Required for ui->loader_type_combo
#include <QMessageBox>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <regex>

Mask_Loader_Widget::Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    // Connect loader type combo box
    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Mask_Loader_Widget::_onLoaderTypeChanged);

    // Connect signals from HDF5 loader widget
    connect(ui->hdf5_mask_loader_widget, &HDF5MaskLoader_Widget::loadSingleHDF5MaskRequested,
            this, &Mask_Loader_Widget::_handleSingleHDF5LoadRequested);
    connect(ui->hdf5_mask_loader_widget, &HDF5MaskLoader_Widget::loadMultiHDF5MaskRequested,
            this, &Mask_Loader_Widget::_handleMultiHDF5LoadRequested);

    // Connect signals from Image loader widget
    connect(ui->image_mask_loader_widget, &ImageMaskLoader_Widget::loadImageMaskRequested,
            this, &Mask_Loader_Widget::_handleImageMaskLoadRequested);

    // Set initial state of stacked widget
    ui->stacked_loader_options->setCurrentWidget(ui->hdf5_mask_loader_widget);
}

Mask_Loader_Widget::~Mask_Loader_Widget() {
    delete ui;
}

void Mask_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "HDF5") {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_mask_loader_widget);
    } else if (ui->loader_type_combo->itemText(index) == "Image") {
        ui->stacked_loader_options->setCurrentWidget(ui->image_mask_loader_widget);
    }
    // Add else-if for other types when implemented
}

void Mask_Loader_Widget::_handleSingleHDF5LoadRequested() {
    auto filename = QFileDialog::getOpenFileName(
            this,
            tr("Load Single HDF5 Mask File"),
            QDir::currentPath(),
            tr("HDF5 files (*.h5 *.hdf5);;All files (*.*)"));

    if (filename.isNull()) {
        return;
    }
    _loadSingleHDF5MaskFile(filename.toStdString());
}

void Mask_Loader_Widget::_handleMultiHDF5LoadRequested(QString pattern) {
    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            tr("Select Directory Containing HDF5 Masks"),
            QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }
    _loadMultiHDF5MaskFiles(dir_name, pattern);
}

void Mask_Loader_Widget::_loadMultiHDF5MaskFiles(QString const& dir_name, QString const& pattern_str) {
    std::filesystem::path const directory(dir_name.toStdString());
    std::vector<std::filesystem::path> mask_files;
    std::string filename_pattern = pattern_str.toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5"; 
    }
    std::regex const regex_pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (auto const & entry: std::filesystem::directory_iterator(directory)) {
        std::string const filename = entry.path().filename().string();
        if (std::regex_match(filename, regex_pattern)) {
            mask_files.push_back(entry.path());
        }
    }
    std::sort(mask_files.begin(), mask_files.end());

    int mask_num = 0;
    for (auto const & file: mask_files) {
        _loadSingleHDF5MaskFile(file.string(), std::to_string(mask_num));
        mask_num += 1;
    }
}

void Mask_Loader_Widget::_loadSingleHDF5MaskFile(std::string const & filename, std::string const & mask_suffix) {
    auto mask_key = ui->data_name_text->text().toStdString();
    if (mask_key.empty()) {
        mask_key = std::filesystem::path(filename).stem().string();
        if (mask_key.empty()) {
            mask_key = "hdf5_mask"; 
        }
    }
    if (!mask_suffix.empty()) {
        mask_key += "_" + mask_suffix;
    }

    try {
        // Use the HDF5 loader through the plugin system
        auto& registry = LoaderRegistry::getInstance();
        auto const* loader = registry.findLoader("hdf5", toIODataType(DM_DataType::Mask));
        
        if (!loader) {
            QMessageBox::critical(this, "Load Error", "HDF5 loader not found. Please ensure the HDF5 plugin is loaded.");
            return;
        }
        
        // Create factory for data creation
        ConcreteDataFactory factory;
        
        // Configure HDF5 loading parameters
        nlohmann::json config;
        config["frame_key"] = "frames";
        config["x_key"] = "widths";
        config["y_key"] = "heights";
        
        // Add image size from scaling widget if available
        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        if (original_size.width > 0 && original_size.height > 0) {
            config["image_width"] = original_size.width;
            config["image_height"] = original_size.height;
        }
        
        // Load the data
        auto result = loader->loadData(filename, toIODataType(DM_DataType::Mask), config, &factory);
        
        if (!result.success) {
            QMessageBox::critical(this, "Load Error", QString::fromStdString("Failed to load HDF5 file: " + result.error_message));
            return;
        }
        
        // Extract the MaskData from the variant
        if (!std::holds_alternative<std::shared_ptr<MaskData>>(result.data)) {
            QMessageBox::critical(this, "Load Error", "Unexpected data type returned from HDF5 loader");
            return;
        }
        
        auto mask_data_ptr = std::get<std::shared_ptr<MaskData>>(result.data);
        
        // Apply scaling if enabled
        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                mask_data_ptr->changeImageSize(scaled_size);
            }
        }
        
        // Store in DataManager
        _data_manager->setData<MaskData>(mask_key, mask_data_ptr, TimeKey("time"));
        
        QMessageBox::information(this, "Load Successful", QString::fromStdString("HDF5 Mask data loaded into " + mask_key));

    } catch (std::exception const& e) {
        std::cerr << "Error loading HDF5 file " << filename << ": " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading HDF5: " + std::string(e.what())));
    }
}

void Mask_Loader_Widget::_handleImageMaskLoadRequested(ImageMaskLoaderOptions options) {
    auto mask_key = ui->data_name_text->text().toStdString();
    if (mask_key.empty()) {
        mask_key = "mask";
    }

    auto mask_data_ptr = load(options);

    _data_manager->setData<MaskData>(mask_key, mask_data_ptr, TimeKey("time"));

    ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
    mask_data_ptr->setImageSize(original_size);

    if (ui->scaling_widget->isScalingEnabled()) {
        ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
        mask_data_ptr->changeImageSize(scaled_size);
    }
} 
