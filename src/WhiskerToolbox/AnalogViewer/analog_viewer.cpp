//
// Created by wanglab on 4/15/2024.
//

#include "analog_viewer.hpp"

#include "ui_analog_viewer.h"

Analog_Viewer::Analog_Viewer(Media_Window* scene, std::shared_ptr<TimeFrame> time, QWidget *parent) :
    QWidget(parent),  ui(new Ui::Analog_Viewer)
{
    ui->setupUi(this);

    _scene = scene;
    _time = time;

}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}
