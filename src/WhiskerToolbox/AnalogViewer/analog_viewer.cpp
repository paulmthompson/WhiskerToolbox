//
// Created by wanglab on 4/15/2024.
//

#include <QMainWindow>
#include <QPointer>

#include "analog_viewer.hpp"
#include "ui_analog_viewer.h"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtplines.h"
#include "jkqtplotter/jkqtpgraphsbase.h"

#include <iostream>

Analog_Viewer::Analog_Viewer(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
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

    this->show();
}

void Analog_Viewer::SetFrame(int i){
    std::cout << "Analog Viewer: Set Frame " << i << std::endl;

    ui->plot->zoom(i-5, i+5, -10, 10);
}

/**
 * @brief Plot a line on the analog viewer
 * @param data Vector indexed by frame number 
 */
void Analog_Viewer::plotLine(std::string name, std::vector<double>& data){
    if (_plot_elements.find(name) != _plot_elements.end()) {
        std::cout << "Plot element named " << name << " already exists, choose another name" << std::endl;
        return;
    }

    JKQTPDatastore* ds = ui->plot->getDatastore();
    std::vector<int> frame_numbers(data.size());
    iota(frame_numbers.begin(), frame_numbers.end(), 0);
    size_t columnX=ds->addCopiedColumn(frame_numbers, QString::fromStdString(name+"_x"));
    size_t columnY=ds->addCopiedColumn(data, QString::fromStdString(name+"_y"));

    JKQTPXYLineGraph* graph=new JKQTPXYLineGraph(ui->plot);
    graph->setXColumn(columnX);
    graph->setYColumn(columnY);
    graph->setTitle(QObject::tr(name.c_str()));

    _plot_elements[name] = graph;
    ui->plot->addGraph(graph);
}

void Analog_Viewer::removeGraph(std::string name){
    if (_plot_elements.find(name) == _plot_elements.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    }

    JKQTPPlotElement* graph = _plot_elements[name];
    ui->plot->deleteGraph(graph);
    _plot_elements.erase(name);
}