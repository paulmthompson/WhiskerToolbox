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

    connect(ui->linechoose_cbox, &QComboBox::currentTextChanged, this, &Analog_Viewer::ResetLineEditor);
    connect(ui->ymult_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::ElementSetLintrans);
    connect(ui->yoffset_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::ElementSetLintrans);
}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}

void Analog_Viewer::openWidget()
{
    std::cout << "Analog Viewer Opened" << std::endl;

    for (auto name : _data_manager->getAnalogTimeSeriesKeys()) {
        plotLine(name);
    }

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
void Analog_Viewer::plotLine(std::string name){
    auto data = _data_manager->getAnalogTimeSeries(name)->getAnalogTimeSeries();
    if (_plot_elements.find(name) != _plot_elements.end()) {
        std::cout << "Plot element named " << name << " already exists, ignoring" << std::endl;
        return;
    }

    PlotElementInfo element;

    JKQTPDatastore* ds = ui->plot->getDatastore();
    std::vector<int> frame_numbers(data.size());
    iota(frame_numbers.begin(), frame_numbers.end(), 0);
    size_t x_col=ds->addCopiedColumn(frame_numbers, QString::fromStdString(name+"_x"));
    size_t y_col=ds->addCopiedColumn(data, QString::fromStdString(name+"_y_trans"));

    element.ds_y_col = y_col;
    element.mult = 1.0;
    element.add = 0.0;

    JKQTPXYLineGraph* graph=new JKQTPXYLineGraph(ui->plot);
    graph->setSymbolType(JKQTPNoSymbol);
    graph->setXColumn(x_col);
    graph->setYColumn(y_col);
    graph->setTitle(QObject::tr(name.c_str()));

    element.element = graph;

    _plot_elements[name] = element;

    ui->linechoose_cbox->addItem(QString::fromStdString(name));

    ui->plot->addGraph(graph);
}

void Analog_Viewer::removeGraph(std::string name){
    if (_plot_elements.find(name) == _plot_elements.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    }

    JKQTPPlotElement* graph = _plot_elements[name].element;
    ui->plot->deleteGraph(graph);
    _plot_elements.erase(name);
}

void Analog_Viewer::_element_apply_lintrans(std::string name){
    if (_plot_elements.find(name) == _plot_elements.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    } 

    auto ds = ui->plot->getDatastore();
    auto data = _data_manager->getAnalogTimeSeries(name)->getAnalogTimeSeries();
    for (int i=0; i<data.size(); i++) {
        ds->set(_plot_elements[name].ds_y_col, i, data[i]*_plot_elements[name].mult+_plot_elements[name].add);
    }
}

void Analog_Viewer::ElementSetLintrans(){
    std::string name = ui->linechoose_cbox->currentText().toStdString();
    if (!name.empty()) {
        _plot_elements[name].mult = ui->ymult_dspinbox->value();
        _plot_elements[name].add = ui->yoffset_dspinbox->value();
        _element_apply_lintrans(name);
        ui->plot->redrawPlot();
    }
}

void Analog_Viewer::ResetLineEditor(){
    std::string name = ui->linechoose_cbox->currentText().toStdString();
    ui->ymult_dspinbox->setValue(_plot_elements[name].mult);
    ui->yoffset_dspinbox->setValue(_plot_elements[name].add);
}