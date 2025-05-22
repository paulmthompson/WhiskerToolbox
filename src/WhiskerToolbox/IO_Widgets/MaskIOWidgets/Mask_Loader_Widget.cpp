#include "Mask_Loader_Widget.hpp"
#include "ui_Mask_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/IO/HDF5/Mask_Data_HDF5.hpp"
#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QFileDialog>
#include <QStackedWidget> // Required for ui->stacked_loader_options
#include <QComboBox> // Required for ui->loader_type_combo

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

    // Set initial state of stacked widget
    ui->stacked_loader_options->setCurrentWidget(ui->hdf5_mask_loader_widget);
}

Mask_Loader_Widget::~Mask_Loader_Widget() {
    delete ui;
}

void Mask_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "HDF5") {
        ui->stacked_loader_options->setCurrentWidget(ui->hdf5_mask_loader_widget);
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
        mask_key = "mask"; 
    }
    if (!mask_suffix.empty()) {
        mask_key += "_" + mask_suffix;
    }

    auto opts = HDF5MaskLoaderOptions();
    opts.filename = filename;

    auto mask_data_ptr = load_mask_from_hdf5(opts);

    _data_manager->setData<MaskData>(mask_key, mask_data_ptr);

    ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
    mask_data_ptr->setImageSize(original_size);

    if (ui->scaling_widget->isScalingEnabled()) {
        ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
        mask_data_ptr->changeImageSize(scaled_size);
    }
} 
