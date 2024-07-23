
#include "analog_viewer.hpp"

#include "ui_analog_viewer.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include "DigitalTimeSeriesGraph.hpp"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtplines.h"
#include "jkqtplotter/jkqtpgraphsbase.h"
#include <QMainWindow>
#include <QPointer>
#include <QCheckBox>
#include <QPushButton>

#include <iostream>

Analog_Viewer::Analog_Viewer(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Analog_Viewer)
{
    ui->setupUi(this);

    connect(ui->graphchoose_cbox, &QComboBox::currentTextChanged, this, &Analog_Viewer::SetPlotEditor);
    connect(ui->ymult_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::GraphSetLintrans);
    connect(ui->yoffset_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::GraphSetLintrans);
    connect(ui->xwidth_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::SetZoom);
    connect(ui->show_checkbox, &QCheckBox::stateChanged, this, &Analog_Viewer::GraphSetShow);
    connect(ui->delete_pushbtn, &QPushButton::clicked, this, &Analog_Viewer::GraphDelete);
    connect(ui->plot, &JKQTPlotter::plotMouseClicked, this, &Analog_Viewer::ClickEvent);
}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}

void Analog_Viewer::openWidget()
{
    std::cout << "Analog Viewer Opened" << std::endl;

    for (auto name : _data_manager->getAnalogTimeSeriesKeys()) {
        plotAnalog(name);
    }
    for (auto name : _data_manager->getDigitalTimeSeriesKeys()) {
        plotDigital(name);
    }
    _setZoom();

    ui->plot->setContextMenuMode(jkqtpcmmNoContextMenu);
    ui->plot->clearAllRegisteredMouseDoubleClickActions();
    ui->plot->registerMouseDragAction(Qt::LeftButton, Qt::NoModifier, jkqtpmdaPanPlotOnMove);

    this->show();
}

void Analog_Viewer::SetFrame(int i){
    std::cout << "Analog Viewer: Set Frame " << i << std::endl;

    _current_frame = i;
    _setZoom();
}

/**
 * @brief Plot a line on the analog viewer
 * @param data Vector indexed by frame number 
 */
void Analog_Viewer::plotAnalog(std::string name){
    auto data = _data_manager->getAnalogTimeSeries(name)->getAnalogTimeSeries();
    if (_graphs.find(name) != _graphs.end()) {
        std::cout << "Plot element named " << name << " already exists, data has been replaced" << std::endl;
        _graphApplyLintrans(name);
        ui->plot->redrawPlot();
        return;
    }

    GraphInfo graphInfo;
    graphInfo.type = GraphType::analog;

    JKQTPDatastore* ds = ui->plot->getDatastore();
    std::vector<int> frame_numbers(data.size());
    iota(frame_numbers.begin(), frame_numbers.end(), 0);
    size_t x_col=ds->addCopiedColumn(frame_numbers, QString::fromStdString(name+"_x"));
    size_t y_col=ds->addCopiedColumn(data, QString::fromStdString(name+"_y_trans"));

    graphInfo.ds_y_col = y_col;
    graphInfo.mult = 1.0;
    graphInfo.add = 0.0;

    JKQTPXYLineGraph* graph=new JKQTPXYLineGraph(ui->plot);
    graph->setSymbolType(JKQTPNoSymbol);
    graph->setXColumn(x_col);
    graph->setYColumn(y_col);
    graph->setTitle(QObject::tr(name.c_str()));

    graphInfo.graph = graph;

    _graphs[name] = graphInfo;

    ui->graphchoose_cbox->addItem(QString::fromStdString(name));

    ui->plot->addGraph(graph);
}

void Analog_Viewer::plotDigital(std::string name){
    auto data = _data_manager->getDigitalTimeSeries(name)->getDigitalTimeSeries();

    if (_graphs.find(name) != _graphs.end()) {
        std::cout << "Plot element named " << name << " already exists, data will be replaced" << std::endl;
        removeGraph(name);
        plotDigital(name);
        return;
    }

    GraphInfo graphInfo;
    graphInfo.type = GraphType::digital;

    DigitalTimeSeriesGraph* graph = new DigitalTimeSeriesGraph(ui->plot->getPlotter());
    graph->load_digital_vector(data);
    graph->setTitle(QObject::tr(name.c_str()));
    graphInfo.graph = graph;

    _graphs[name] = graphInfo;
    ui->graphchoose_cbox->addItem(QString::fromStdString(name));
    
    ui->plot->addGraph(graph);
}

void Analog_Viewer::removeGraph(std::string name){
    if (_graphs.find(name) == _graphs.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    }

    ui->plot->deleteGraph(_graphs[name].graph);
    _graphs.erase(name);
}

void Analog_Viewer::_graphApplyLintrans(std::string name){
    if (_graphs.find(name) == _graphs.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    } 

    auto ds = ui->plot->getDatastore();
    auto data = _data_manager->getAnalogTimeSeries(name)->getAnalogTimeSeries();
    for (int i=0; i<data.size(); i++) {
        ds->set(_graphs[name].ds_y_col, i, data[i]*_graphs[name].mult+_graphs[name].add);
    }
}

void Analog_Viewer::GraphSetLintrans(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        _graphs[name].mult = ui->ymult_dspinbox->value();
        _graphs[name].add = ui->yoffset_dspinbox->value();
        _graphApplyLintrans(name);
        ui->plot->redrawPlot();
    }
}

void Analog_Viewer::SetPlotEditor(){
    std::string name = _getSelectedGraphName(); 
    if (name.empty()) {
        return;
    }
    if (_graphs[name].type == GraphType::analog){
        ui->ymult_dspinbox->setEnabled(true);
        ui->yoffset_dspinbox->setEnabled(true);
        ui->ymult_dspinbox->setValue(_graphs[name].mult);
        ui->yoffset_dspinbox->setValue(_graphs[name].add);
        ui->show_checkbox->setChecked(_graphs[name].show);
    } else if (_graphs[name].type == GraphType::digital){
        ui->ymult_dspinbox->setEnabled(false);
        ui->yoffset_dspinbox->setEnabled(false);
        ui->show_checkbox->setChecked(_graphs[name].show);
    }
    if (!_prev_graph_highlighted.empty()) {
        _graphs[_prev_graph_highlighted].graph->setHighlighted(false);
    }
    _prev_graph_highlighted = name;
    _graphs[name].graph->setHighlighted(true);

    ui->plot->redrawPlot();
}

void Analog_Viewer::_setZoom(){
    ui->plot->zoom(_current_frame - ui->xwidth_dspinbox->value()/2, _current_frame + ui->xwidth_dspinbox->value()/2, -10, 10);
}

void Analog_Viewer::SetZoom(){
    _setZoom();
}

void Analog_Viewer::GraphSetShow(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        _graphs[name].show = ui->show_checkbox->isChecked();
        _graphs[name].graph->setVisible(_graphs[name].show);
        ui->plot->redrawPlot();
    }
}

void Analog_Viewer::ClickEvent(double x, double y, Qt::KeyboardModifiers modifiers, Qt::MouseButton button){
    if (button == Qt::LeftButton) {
        double min_dist = 1e9;
        std::string min_dist_name = "";
        for (auto& [name, graphInfo] : _graphs) {
            if (graphInfo.show && graphInfo.type == GraphType::analog) {
                double dist = graphInfo.graph->hitTest(QPointF(x, y));
                if (dist < min_dist) {
                    min_dist = dist;
                    min_dist_name = name;
                }
            }
        }
        if (!min_dist_name.empty()) {
            ui->graphchoose_cbox->setCurrentText(QString::fromStdString(min_dist_name));
        }
    }
}

std::string Analog_Viewer::_getSelectedGraphName(){
    return ui->graphchoose_cbox->currentText().toStdString();
}

void Analog_Viewer::GraphDelete(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        removeGraph(name);
        ui->graphchoose_cbox->removeItem(ui->graphchoose_cbox->currentIndex());
    }
}