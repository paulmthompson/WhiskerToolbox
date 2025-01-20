
#include "Line_Loader_Widget.hpp"

#include "ui_Line_Loader_Widget.h"

#include "../../DataManager/DataManager.hpp"
#include "../../DataManager/Lines/Line_Data.hpp"

#include <QFileDialog>

#include <filesystem>
#include <iostream>
#include <regex>

Line_Loader_Widget::Line_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Line_Loader_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    connect(ui->load_single_hdf5_line, &QPushButton::clicked, this, &Line_Loader_Widget::_loadSingleHdf5Line);
    connect(ui->load_multi_hdf5_line, &QPushButton::clicked, this, &Line_Loader_Widget::_loadMultiHdf5Line);
}

Line_Loader_Widget::~Line_Loader_Widget() {
    delete ui;
}

void Line_Loader_Widget::_loadSingleHdf5Line()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Line File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    _loadSingleHDF5Line(filename.toStdString());
}

void Line_Loader_Widget::_loadMultiHdf5Line()
{
    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    std::filesystem::path directory(dir_name.toStdString());

    // Store the paths of all files that match the criteria
    std::vector<std::filesystem::path> line_files;

    std::string filename_pattern = ui->multi_hdf5_name_pattern->toPlainText().toStdString();

    if (filename_pattern.empty()) {
        filename_pattern = "*.h5";
    }
    std::regex pattern(std::regex_replace(filename_pattern, std::regex("\\*"), ".*"));

    for (const auto & entry : std::filesystem::directory_iterator(directory)) {
        std::string filename = entry.path().filename().string();
        std::cout << filename << std::endl;
        if (std::regex_match(filename, pattern)) {
            line_files.push_back(entry.path());
        } else {
            std::cout << "File " << filename << " does not match pattern " << filename_pattern << std::endl;
        }
    }

    // Sort the files based on their names
    std::sort(line_files.begin(), line_files.end());

    // Load the files in sorted order
    int line_num = 0;
    for (const auto & file : line_files) {
        _loadSingleHDF5Line(file.string(), std::to_string(line_num));
        line_num += 1;
    }
}

void Line_Loader_Widget::_loadSingleHDF5Line(std::string filename, std::string line_suffix)
{

    auto line_key = ui->data_name_text->toPlainText().toStdString();

    if (line_key.empty()) {
        line_key = "line";
    }
    if (!line_suffix.empty()) {
        line_key += "_" + line_suffix;
    }

    auto frames =  read_array_hdf5(filename, "frames");
    auto y_coords = read_ragged_hdf5(filename, "x");
    auto x_coords = read_ragged_hdf5(filename, "y");

    auto line = _data_manager->getData<LineData>(line_key);

    for (std::size_t i = 0; i < frames.size(); i ++) {
        line->addLineAtTime(frames[i], x_coords[i], y_coords[i]);
    }

}
