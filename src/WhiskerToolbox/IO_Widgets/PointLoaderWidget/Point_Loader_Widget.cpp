#include "Point_Loader_Widget.hpp"

#include "ui_Point_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "IO_Widgets/Scaling_Widget/Scaling_Widget.hpp"

#include <QFileDialog>
#include <QComboBox>
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

    ui->stacked_loader_options->setCurrentWidget(ui->csv_point_loader_widget);
}

Point_Loader_Widget::~Point_Loader_Widget() {
    delete ui;
}

void Point_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_point_loader_widget);
    }
}

void Point_Loader_Widget::_handleSingleCSVLoadRequested(QString delimiterText) {
    auto keypoint_filename = QFileDialog::getOpenFileName(
            this,
            tr("Load Keypoints CSV File"),
            QDir::currentPath(),
            tr("CSV files (*.csv);;All files (*.*)"));

    if (keypoint_filename.isNull()) {
        return;
    }
    _loadSingleCSVFile(keypoint_filename.toStdString(), delimiterText);
}

void Point_Loader_Widget::_loadSingleCSVFile(std::string const& filename, QString delimiterText) {
    auto const keypoint_key = ui->data_name_text->text().toStdString();

    char delimiter;
    if (delimiterText == "Space") {
        delimiter = ' ';
    } else if (delimiterText == "Comma") {
        delimiter = ',';
    } else {
        std::cout << "Unsupported delimiter: " << delimiterText.toStdString() << std::endl;
        return;
    }

    auto opts = CSVPointLoaderOptions{.filename = filename,
                                      .frame_column = 0,
                                      .x_column = 1,
                                      .y_column = 2,
                                      .column_delim = delimiter};

    auto keypoints = load_points_from_csv(opts);

    _data_manager->setData<PointData>(keypoint_key);
    auto point_data_ptr = _data_manager->getData<PointData>(keypoint_key);

    ImageSize original_size = ui->scaling_widget->getOriginalImageSize();
    point_data_ptr->setImageSize(original_size);

    if (ui->scaling_widget->isScalingEnabled()) {
        ImageSize scaled_size = ui->scaling_widget->getScaledImageSize();
        point_data_ptr->changeImageSize(scaled_size);
    }

}
