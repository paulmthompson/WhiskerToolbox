
#include "Mask_Loader_Widget.hpp"

#include "ui_Mask_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/loaders/hdf5_loaders.hpp"

#include <QFileDialog>

#include <filesystem>
#include <iostream>
#include <regex>

Mask_Loader_Widget::Mask_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_single_hdf5_mask, &QPushButton::clicked, this, &Mask_Loader_Widget::_loadSingleHdf5Mask);
    connect(ui->load_multi_hdf5_mask, &QPushButton::clicked, this, &Mask_Loader_Widget::_loadMultiHdf5Mask);
}

Mask_Loader_Widget::~Mask_Loader_Widget() {
    delete ui;
}

void Mask_Loader_Widget::_loadSingleHdf5Mask() {
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

void Mask_Loader_Widget::_loadMultiHdf5Mask() {
    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            "Select Directory",
            QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    std::filesystem::path const directory(dir_name.toStdString());

    // Store the paths of all files that match the criteria
    std::vector<std::filesystem::path> mask_files;

    std::string filename_pattern = ui->multi_hdf5_name_pattern->toPlainText().toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5";
    }
    std::regex const pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (auto const & entry: std::filesystem::directory_iterator(directory)) {
        std::string const filename = entry.path().filename().string();
        std::cout << filename << std::endl;
        if (std::regex_match(filename, pattern)) {
            mask_files.push_back(entry.path());
        } else {
            std::cout << "File " << filename << " does not match pattern " << filename_pattern << std::endl;
        }
    }

    // Sort the files based on their names
    std::sort(mask_files.begin(), mask_files.end());

    // Load the files in sorted order
    int mask_num = 0;
    for (auto const & file: mask_files) {
        _loadSingleHDF5Mask(file.string(), std::to_string(mask_num));
        mask_num += 1;
    }
}

void Mask_Loader_Widget::_loadSingleHDF5Mask(std::string const & filename, std::string const & mask_suffix) {

    auto mask_key = ui->data_name_text->toPlainText().toStdString();

    if (mask_key.empty()) {
        mask_key = "mask";
    }
    if (!mask_suffix.empty()) {
        mask_key += "_" + mask_suffix;
    }

    auto frames = Loader::read_array_hdf5({filename,  "frames"});
    auto probs = Loader::read_ragged_hdf5({filename, "probs"});
    auto y_coords = Loader::read_ragged_hdf5({filename, "heights"});
    auto x_coords = Loader::read_ragged_hdf5({filename, "widths"});

    _data_manager->setData<MaskData>(mask_key);

    auto mask = _data_manager->getData<MaskData>(mask_key);

    for (std::size_t i = 0; i < frames.size(); i++) {
        mask->addMaskAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    mask->setImageSize({ui->width_scaling->value(), ui->height_scaling->value()});
}
