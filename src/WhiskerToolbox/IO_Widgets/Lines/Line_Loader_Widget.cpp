#include "Line_Loader_Widget.hpp"

#include "ui_Line_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/loaders/hdf5_loaders.hpp"
#include "DataManager/Lines/IO/Binary/Line_Data_Binary.hpp"
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

    auto opts = BinaryLineLoaderOptions();
    opts.file_path = file_path_std;
    std::shared_ptr<LineData> line_data = load(opts);

    if (line_data) {
        _data_manager->setData<LineData>(line_key, line_data);
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
    } else {
        std::cerr << "Failed to load binary line data: " << file_path_std << std::endl;
        QMessageBox::critical(this, "Load Failed", QString::fromStdString("Could not load binary line data from " + file_path_std));
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
        auto frames = Loader::read_array_hdf5({filename,  "frames"});
        auto y_coords = Loader::read_ragged_hdf5({filename, "x"}); 
        auto x_coords = Loader::read_ragged_hdf5({filename, "y"});

        if (frames.empty() && x_coords.empty() && y_coords.empty()){
            std::cout << "No data loaded from HDF5 file: " << filename << ". File might be empty or datasets not found." << std::endl;
            QMessageBox::warning(this, "Load Warning", QString::fromStdString("No data found in HDF5 file: " + filename));
            return;
        }

        _data_manager->setData<LineData>(line_key);
        auto line_data_ptr = _data_manager->getData<LineData>(line_key);

        for (std::size_t i = 0; i < frames.size(); i++) {
            if (i < x_coords.size() && i < y_coords.size()) {
                line_data_ptr->addLineAtTime(frames[i], x_coords[i], y_coords[i]);
            } else {
                std::cerr << "Warning: Missing x/y coordinates for frame " << frames[i] << " in file " << filename << std::endl;
            }
        }

        ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
        line_data_ptr->setImageSize(original_size);

        if (ui->scaling_widget->isScalingEnabled()) {
            ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
            if (scaled_size.width > 0 && scaled_size.height > 0) {
                line_data_ptr->changeImageSize(scaled_size);
            }
        }
        QMessageBox::information(this, "Load Successful", QString::fromStdString("HDF5 Line data loaded into " + line_key));

    } catch (std::exception const& e) {
        std::cerr << "Error loading HDF5 file " << filename << ": " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading HDF5: " + std::string(e.what())));
    }
}
