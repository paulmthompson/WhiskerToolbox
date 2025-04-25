#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"

#include <QFileDialog>
#include <QPlainTextEdit>
#include <QPushButton>

#include <fstream>
#include <iostream>

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Point_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _point_table_model = new PointTableModel(this);
    ui->tableView->setModel(_point_table_model);

    connect(ui->save_csv_button, &QPushButton::clicked, this, &Point_Widget::_saveKeypointCSV);
}

Point_Widget::~Point_Widget() {
    delete ui;
}

void Point_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

void Point_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    updateTable();
}

void Point_Widget::updateTable() {
    auto points = _data_manager->getData<PointData>(_active_key)->getData();
    _point_table_model->setPoints(points);
}

void Point_Widget::_saveKeypointCSV() {
    auto const filename = ui->save_filename->text().toStdString();

    std::fstream fout;

    auto frame_by_frame_output = _data_manager->getOutputPath();

    fout.open(frame_by_frame_output.append(filename).string(), std::fstream::out);

    auto point_data = _data_manager->getData<PointData>(_active_key)->getData();

    for (auto & [key, val]: point_data) {
        fout << key << "," << std::to_string(val[0].x) << "," << std::to_string(val[0].y) << "\n";
    }

    fout.close();
}

void Point_Widget::assignPoint(qreal x_media, qreal y_media) {

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    switch (_selection_mode) {

        case Point_Select: {

            auto point = _data_manager->getData<PointData>(_active_key);
            point->overwritePointAtTime(frame_id, static_cast<float>(y_media), static_cast<float>(x_media));

            break;
        }
        default:
            break;
    }
}

void Point_Widget::loadFrame(int frame_id) {
    if (ui->propagate_checkbox->isChecked()) {
        _propagateLabel(frame_id);
    }

    _previous_frame = frame_id;
}

void Point_Widget::_propagateLabel(int frame_id) {

    auto point_data = _data_manager->getData<PointData>(_active_key);

    auto prev_points = point_data->getPointsAtTime(_previous_frame);

    for (int i = _previous_frame + 1; i <= frame_id; i++) {
        point_data->overwritePointsAtTime(i, prev_points);
    }
}
