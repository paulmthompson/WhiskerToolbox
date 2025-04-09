
#include "Point_Loader_Widget.hpp"

#include "ui_Point_Loader_Widget.h"

#include "../../DataManager/DataManager.hpp"
#include "../../DataManager/Points/Point_Data.hpp"

#include <QFileDialog>

#include <iostream>

Point_Loader_Widget::Point_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Point_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_single_button, &QPushButton::clicked, this, &Point_Loader_Widget::_loadSingleKeypoint);
}

Point_Loader_Widget::~Point_Loader_Widget() {
    delete ui;
}

void Point_Loader_Widget::_loadSingleKeypoint() {
    auto keypoint_filename = QFileDialog::getOpenFileName(
            this,
            "Load Keypoints",
            QDir::currentPath(),
            "All files (*.*)");

    if (keypoint_filename.isNull()) {
        return;
    }

    auto const keypoint_key = ui->data_name_text->toPlainText().toStdString();

    char delimiter;
    if (ui->delimiter_combo->currentText() == "Space") {
        delimiter = ' ';
    } else if (ui->delimiter_combo->currentText() == "Comma") {
        delimiter = ',';
    } else {
        std::cout << "Unsupported delimiter" << std::endl;
        return;
    }

    auto opts = CSVPointLoaderOptions{.filename = keypoint_filename.toStdString(),
                                      .frame_column = 0,
                                      .x_column = 1,
                                      .y_column = 2,
                                      .column_delim = delimiter};

    auto keypoints = load_points_from_csv(opts);

    std::cout << "Loaded " << keypoints.size() << " keypoints" << std::endl;
    auto point_num = _data_manager->getKeys<PointData>().size();

    _data_manager->setData<PointData>(keypoint_key);

    auto point = _data_manager->getData<PointData>(keypoint_key);

    point->setImageSize({ui->width_scaling->value(), ui->height_scaling->value()});

    for (auto & [key, val]: keypoints) {
        point->addPointAtTime(key, val.x, val.y);
    }
}
