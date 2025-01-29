
#include "Tracking_Widget.hpp"

#include "ui_Tracking_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
#include <iostream>

Tracking_Widget::Tracking_Widget(Media_Window *scene,
                               std::shared_ptr<DataManager> data_manager,
                               TimeScrollBar* time_scrollbar,
                               MainWindow* mainwindow,
                               QWidget *parent) :
        QMainWindow(parent),
        _scene{scene},
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{mainwindow},
        ui(new Ui::Tracking_Widget)
{
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    _current_tracking_key = "tracking_point";

}

Tracking_Widget::~Tracking_Widget() {
    delete ui;
}

void Tracking_Widget::openWidget() {

    std::cout << "Tracking Widget Opened" << std::endl;

    connect(_scene, SIGNAL(leftClickMedia(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    if (_data_manager->getData<PointData>(_current_tracking_key))
    {
        _data_manager->setData<PointData>(_current_tracking_key);
    }

    auto media = _data_manager->getData<MediaData>("media");

    auto point = _data_manager->getData<PointData>(_current_tracking_key);

    point->setImageSize({media->getWidth(), media->getHeight()});

    _scene->addPointDataToScene(_current_tracking_key);

    _data_manager->addCallbackToData(_current_tracking_key, [this]() {
        _scene->UpdateCanvas();
    });

    this->show();
}

void Tracking_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(_scene, SIGNAL(leftClickMedia(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
}

void Tracking_Widget::_clickedInVideo(qreal x_media, qreal y_media) {

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    switch (_selection_mode) {

    case Tracking_Select: {
        std::string tracking_label =
            "(" + std::to_string(static_cast<int>(x_media)) + " , " + std::to_string(static_cast<int>(y_media)) +
            ")";
        ui->location_label->setText(QString::fromStdString(tracking_label));

        auto point = _data_manager->getData<PointData>(_current_tracking_key);
        point->clearPointsAtTime(frame_id);
        point->addPointAtTime(frame_id, y_media, x_media);

        break;
    }
    default:
        break;
    }
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

        std::string tracking_label =
            "(" + x + " , " + y + ")";
        ui->location_label->setText(QString::fromStdString(tracking_label));
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
