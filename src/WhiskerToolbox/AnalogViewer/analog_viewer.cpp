//
// Created by wanglab on 4/15/2024.
//

#include <QMainWindow>
#include <QPointer>

#include "analog_viewer.hpp"
#include "ui_analog_viewer.h"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtplines.h"
#include "jkqtplotter/graphs/jkqtpparsedfunction.h"

#include <iostream>

Analog_Viewer::Analog_Viewer(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Analog_Viewer)
{
    ui->setupUi(this);

    JKQTPXParsedFunctionLineGraph* graph = new JKQTPXParsedFunctionLineGraph(ui->plot);
    graph->setFunction("y=x^3-4*x");
    graph->setTitle("Some function");
    ui->plot->addGraph(graph);
    ui->plot->setXY(-10,10,-10,10);
}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}

void Analog_Viewer::openWidget()
{
    std::cout << "Analog Viewer Opened" << std::endl;

    this->show();
}

void Analog_Viewer::SetFrame(int i){
    std::cout << "Analog Viewer: Set Frame " << i << std::endl;
}