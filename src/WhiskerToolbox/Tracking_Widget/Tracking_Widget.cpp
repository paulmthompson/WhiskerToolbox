
#include "Tracking_Widget.hpp"

#include "ui_Tracking_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"


#include <iostream>

Tracking_Widget::Tracking_Widget(std::shared_ptr<DataManager> data_manager,
                               QWidget *parent) :
        QMainWindow(parent),
        _data_manager{data_manager},
        ui(new Ui::Tracking_Widget)
{
    ui->setupUi(this);


}

Tracking_Widget::~Tracking_Widget() {
    delete ui;
}

void Tracking_Widget::openWidget() {

    this->show();
}

void Tracking_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

}

void Tracking_Widget::LoadFrame(int frame_id)
{
    if (ui->propagate_checkbox->isChecked())
    {
        _propagateLabel(frame_id);
    }

    auto points = _data_manager->getData<PointData>(_current_tracking_key)->getPointsAtTime(frame_id);

    if (!points.empty()) {

        std::string x = "";
        std::string y = "";

        if (std::isnan(points[0].x))
        {
            x = "nan";
        } else {
            x = std::to_string(static_cast<int>(points[0].x));
        }

        if (std::isnan(points[0].y))
        {
            y="nan";
        } else {
            y = std::to_string(static_cast<int>(points[0].y));
        }

    }
    _previous_frame = frame_id;
}



void Tracking_Widget::_propagateLabel(int frame_id)
{

    auto prev_points = _data_manager->getData<PointData>(_current_tracking_key)->getPointsAtTime(_previous_frame);

    for (int i = _previous_frame + 1; i <= frame_id; i ++)
    {
        _data_manager->getData<PointData>(_current_tracking_key)->clearPointsAtTime(i);
        _data_manager->getData<PointData>(_current_tracking_key)->addPointAtTime(i, prev_points[0].x, prev_points[0].y);
    }
}
