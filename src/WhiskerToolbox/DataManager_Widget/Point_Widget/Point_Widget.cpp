#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"

#include <QFileDialog>
#include <QPushButton>

#include <fstream>
#include <iostream>

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Point_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    _point_table_model = new PointTableModel(this);
    ui->tableView->setModel(_point_table_model);

    connect(ui->save_csv_button, &QPushButton::clicked, this, &Point_Widget::_saveKeypointCSV);

}

Point_Widget::~Point_Widget() {
    delete ui;
}

void Point_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}

void Point_Widget::setActiveKey(const std::string &key)
{
    _active_key = key;
    auto points = _data_manager->getData<PointData>(_active_key)->getData();
    _point_table_model->setPoints(points);
}

void Point_Widget::_saveKeypointCSV()
{
    const auto filename = "keypoint.csv";

    std::fstream fout;

    auto frame_by_frame_output = _data_manager->getOutputPath();

    fout.open(frame_by_frame_output.append(filename).string(), std::fstream::out);

    auto point_data =_data_manager->getData<PointData>(_active_key)->getData();

    for (auto& [key, val] : point_data)
    {
        fout << key << "," << std::to_string(val[0].x) << "," << std::to_string(val[0].y) << "\n";
    }

    fout.close();
}

void Point_Widget::assignPoint(qreal x_media, qreal y_media) {

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    switch (_selection_mode) {

    case Point_Select: {

        auto point = _data_manager->getData<PointData>(_active_key);
        point->clearPointsAtTime(frame_id);
        point->addPointAtTime(frame_id, y_media, x_media);

        std::cout << "Point added at " << x_media << "," << y_media << std::endl;

        break;
    }
    default:
        break;
    }
}
