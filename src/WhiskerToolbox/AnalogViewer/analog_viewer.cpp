//
// Created by wanglab on 4/15/2024.
//

#include "analog_viewer.hpp"
#include "ui_analog_viewer.h"

#include <iostream>

Analog_Viewer::Analog_Viewer(Media_Window* scene, std::shared_ptr<TimeFrame> time, QWidget *parent) :
    QWidget(parent),
    _scene{scene},
    _time{time},
    ui(new Ui::Analog_Viewer)
{
    ui->setupUi(this);
}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}

void Analog_Viewer::openWidget()
{
    std::cout << "Analog Viewer Opened" << std::endl;

    // connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    /*
    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this,
            SLOT(_ClickedInVideo(qreal, qreal)));
    connect(ui->saveLabelsButton, SIGNAL(clicked()), this, SLOT(_saveButton()));

    connect(ui->label_name_box, SIGNAL(textChanged()), this,
            SLOT(_changeLabelName()));
    */
    this->show();
}
