#include "Point_Loader_Widget.hpp"

#include "ui_Point_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Points/CSV/CSVPointLoader_Widget.hpp"
#include "Scaling_Widget/Scaling_Widget.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QStackedWidget>

#include <iostream>

Point_Loader_Widget::Point_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Point_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Point_Loader_Widget::_onLoaderTypeChanged);

    connect(ui->csv_point_loader_widget, &CSVPointLoader_Widget::loadSingleCSVFileRequested,
            this, &Point_Loader_Widget::_handleSingleCSVLoadRequested);

    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_point_loader_widget);
    } else {
        _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
    }
}

Point_Loader_Widget::~Point_Loader_Widget() {
    delete ui;
}

void Point_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_point_loader_widget);
    }
}

void Point_Loader_Widget::_handleSingleCSVLoadRequested(CSVPointLoaderOptions options) {
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

void Point_Loader_Widget::_loadSingleCSVFile(CSVPointLoaderOptions const & options) {
    auto const keypoint_key = ui->data_name_text->text().toStdString();
    if (keypoint_key.empty()) {
        std::cerr << "Keypoint name cannot be empty!" << std::endl;
        return;
    }

    try {
        auto keypoints = load(options);

        if (keypoints.empty()) {
            std::cout << "No keypoints loaded from " << options.filename << ". The file might be empty or in an incorrect format." << std::endl;
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

    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV file " << options.filename << ": " << e.what() << std::endl;
    }
}
