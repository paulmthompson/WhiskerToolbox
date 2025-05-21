
#include "Line_Loader_Widget.hpp"

#include "ui_Line_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/loaders/hdf5_loaders.hpp"
#include "Loading_Widgets/LineLoaderWidget/HDF5LineLoader_Widget/HDF5LineLoader_Widget.hpp"

#include <QFileDialog>

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
    connect(ui->enable_image_scaling, &QCheckBox::clicked, this, &Line_Loader_Widget::_enableImageScaling);
    ui->scaled_width_spin->setEnabled(false);
    ui->scaled_height_spin->setEnabled(false);
}

Line_Loader_Widget::~Line_Loader_Widget() {
    delete ui;
}

void Line_Loader_Widget::_enableImageScaling(bool enable) {
    if (enable) {
        ui->scaled_height_spin->setEnabled(true);
        ui->scaled_width_spin->setEnabled(true);
    } else {
        ui->scaled_height_spin->setEnabled(false);
        ui->scaled_width_spin->setEnabled(false);
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

    // Store the paths of all files that match the criteria
    std::vector<std::filesystem::path> line_files;

    std::string filename_pattern = pattern.toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5";
    }
    std::regex const regex_pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (auto const & entry: std::filesystem::directory_iterator(directory)) {
        std::string const filename = entry.path().filename().string();
        std::cout << filename << std::endl;
        if (std::regex_match(filename, regex_pattern)) {
            line_files.push_back(entry.path());
        } else {
            std::cout << "File " << filename << " does not match pattern " << filename_pattern << std::endl;
        }
    }

    // Sort the files based on their names
    std::sort(line_files.begin(), line_files.end());

    // Load the files in sorted order
    int line_num = 0;
    for (auto const & file: line_files) {
        _loadSingleHDF5Line(file.string(), std::to_string(line_num));
        line_num += 1;
    }
}

void Line_Loader_Widget::_loadSingleHDF5Line(std::string const & filename, std::string const & line_suffix) {

    auto line_key = ui->data_name_text->text().toStdString();

    if (line_key.empty()) {
        line_key = "line";
    }
    if (!line_suffix.empty()) {
        line_key += "_" + line_suffix;
    }

    auto frames = Loader::read_array_hdf5({filename,  "frames"});
    auto y_coords = Loader::read_ragged_hdf5({filename, "x"});
    auto x_coords = Loader::read_ragged_hdf5({filename, "y"});

    _data_manager->setData<LineData>(line_key);

    auto line = _data_manager->getData<LineData>(line_key);

    for (std::size_t i = 0; i < frames.size(); i++) {
        line->addLineAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    auto const height = ui->original_height_spin->value();
    auto const width = ui->original_width_spin->value();

    line->setImageSize({.width=width, .height=height});

    if (ui->enable_image_scaling->isChecked()) {
        auto const scaled_height = ui->scaled_height_spin->value();
        auto const scaled_width = ui->scaled_width_spin->value();

        line->changeImageSize({scaled_width, scaled_height});
    } else {
        line->changeImageSize({width, height});
    }

}
