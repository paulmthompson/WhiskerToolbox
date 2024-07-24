
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

    connect(ui->graphchoose_cbox, &QComboBox::currentTextChanged, this, &Analog_Viewer::SetGraphEditor);
    connect(ui->yheight_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::GraphSetLintrans);
    connect(ui->yoffset_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::GraphSetLintrans);
    connect(ui->xwidth_dspinbox, &QDoubleSpinBox::valueChanged, this, &Analog_Viewer::SetZoom);
    connect(ui->show_checkbox, &QCheckBox::stateChanged, this, &Analog_Viewer::GraphSetShow);
    connect(ui->showaxis_checkbox, &QCheckBox::stateChanged, this, &Analog_Viewer::GraphSetShowAxis);
    connect(ui->delete_pushbtn, &QPushButton::clicked, this, &Analog_Viewer::GraphDelete);
    connect(ui->plot, &JKQTPlotter::plotMouseClicked, this, &Analog_Viewer::ClickEvent);
}

Analog_Viewer::~Analog_Viewer() {
    delete ui;
}

/**
 * @brief Open the analog viewer
 */
void Analog_Viewer::openWidget()
{
    std::cout << "Analog Viewer Opened" << std::endl;

    // Plot all graphs added before analog viewer was created
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

    ui->plot->getXAxis()->setTickLabelType(JKQTPCALTdefault);

    // Delete the default y axis
    ui->plot->getYAxis()->setDrawMode1(JKQTPCADMnone);
    ui->plot->getYAxis()->setDrawMode2(JKQTPCADMnone);
    ui->plot->getYAxis()->setDrawMode2(JKQTPCADMnone);

    this->show();
}

/**
 * @brief Set the current frame of the analog viewer
 * @param i Frame number
 */
void Analog_Viewer::SetFrame(int i){
    std::cout << "Analog Viewer: Set Frame " << i << std::endl;
    _current_frame = i;
    _setZoom();
}

/**
 * @brief Plot a line on the analog viewer
 * @param name Name of data sotred in DataManager
 */
void Analog_Viewer::plotAnalog(std::string name){
    if (_graphs.find(name) != _graphs.end()) {
        std::cout << "Plot element named " << name << " already exists, data has been replaced" << std::endl;
        ui->plot->redrawPlot();
        return;
    }

    auto data = _data_manager->getAnalogTimeSeries(name)->getAnalogTimeSeries();

    // Loading data into JKQTPlotter datastore
    JKQTPDatastore* ds = ui->plot->getDatastore();
    std::vector<int> frame_numbers(data.size());
    iota(frame_numbers.begin(), frame_numbers.end(), 0);
    size_t x_col=ds->addCopiedColumn(frame_numbers, QString::fromStdString(name+"_x"));
    size_t y_col=ds->addCopiedColumn(data, QString::fromStdString(name+"_y_trans"));

    // Configure JKQTPlotter graph object
    JKQTPXYLineGraph* graph=new JKQTPXYLineGraph(ui->plot);
    graph->setSymbolType(JKQTPNoSymbol);
    graph->setXColumn(x_col);
    graph->setYColumn(y_col);
    graph->setTitle(QObject::tr(name.c_str()));

    auto axis_ref = ui->plot->getPlotter()->addSecondaryYAxis(new JKQTPVerticalAxis(ui->plot->getPlotter(), JKQTPPrimaryAxis));
    ui->plot->getYAxis(axis_ref)->setDrawGrid(false);
    ui->plot->getYAxis(axis_ref)->setDrawMode1(JKQTPCADMcomplete);
    ui->plot->getYAxis(axis_ref)->setDrawMode2(JKQTPCADMnone);
    ui->plot->getYAxis(axis_ref)->setDrawMode0(JKQTPCADMnone);
    ui->plot->getYAxis(axis_ref)->setShowZeroAxis(false);
    ui->plot->getYAxis(axis_ref)->setRange(-5, 5);
    ui->plot->getYAxis(axis_ref)->setColor(graph->getLineColor());

    graph->setYAxis(axis_ref);

    // Configure interal graph object
    GraphInfo graphInfo;
    graphInfo.type = GraphType::analog;
    graphInfo.ds_y_col = y_col;
    graphInfo.height = 10.0;
    graphInfo.offset = 0.0;
    graphInfo.graph = graph;
    graphInfo.axis = ui->plot->getYAxis(axis_ref);
    _graphs[name] = graphInfo;

    ui->graphchoose_cbox->addItem(QString::fromStdString(name));
    ui->plot->addGraph(graph);

}

/**
 * @brief Plot a digital time series on the analog viewer
 * @param name Name of data stored in DataManager
 */
void Analog_Viewer::plotDigital(std::string name){
    if (_graphs.find(name) != _graphs.end()) {
        std::cout << "Plot element named " << name << " already exists, data will be replaced" << std::endl;
        removeGraph(name);
        plotDigital(name);
        return;
    }

    auto data = _data_manager->getDigitalTimeSeries(name)->getDigitalTimeSeries();

    // Configure JKQTPlotter graph object
    DigitalTimeSeriesGraph* graph = new DigitalTimeSeriesGraph(ui->plot->getPlotter());
    graph->load_digital_vector(data);
    graph->setTitle(QObject::tr(name.c_str()));

    // Configure internal graph object
    GraphInfo graphInfo;
    graphInfo.type = GraphType::digital;
    graphInfo.graph = graph;
    _graphs[name] = graphInfo;

    ui->graphchoose_cbox->addItem(QString::fromStdString(name));
    ui->plot->addGraph(graph);
}

/**
 * @brief Remove a graph from the analog viewer
 * @param name Name of graph to remove
 */
void Analog_Viewer::removeGraph(std::string name){
    if (_graphs.find(name) == _graphs.end()) {
        std::cout << "Plot element named " << name << " does not exist" << std::endl;
        return;
    }
    ui->plot->deleteGraph(_graphs[name].graph);
    // So I don't think there's a way to delete an axis after adding one, I guess hiding it forever will work.
    _graphs[name].axis->setDrawMode1(JKQTPCADMnone);
    _graphs.erase(name);
}

/**
 * @brief Slot to apply linear transformation to analog graph when yheight or yoffset is changed in GUI
 */
void Analog_Viewer::GraphSetLintrans(){
    std::string name = _getSelectedGraphName();
    if (name.empty()) {
        return;
    }
    if (_graphs.find(name) == _graphs.end()) {
        return;
    }
    if (_graphs[name].type != GraphType::analog) {
        return;
    }
    _graphs[name].height = ui->yheight_dspinbox->value();

    double cur_offset = (_graphs[name].axis->getMax() + _graphs[name].axis->getMin())/2;
    double final_offset = cur_offset + _graphs[name].offset - ui->yoffset_dspinbox->value();

    _graphs[name].axis->setRange(final_offset - _graphs[name].height/2, final_offset + _graphs[name].height/2);

    _graphs[name].offset = ui->yoffset_dspinbox->value();    //_scaleYAxis();
    ui->plot->redrawPlot();
}

/**
 * @brief Slot to initialize plot editor when a graph is selected in the GUI
 */
void Analog_Viewer::SetGraphEditor(){
    std::string name = _getSelectedGraphName(); 
    if (name.empty()) {
        return;
    }
    if (_graphs[name].type == GraphType::analog){
        ui->yheight_dspinbox->setEnabled(true);
        ui->yoffset_dspinbox->setEnabled(true);
        ui->showaxis_checkbox->setEnabled(true);
        ui->yheight_dspinbox->setValue(_graphs[name].height);
        ui->yoffset_dspinbox->setValue(_graphs[name].offset);
        ui->show_checkbox->setChecked(_graphs[name].show);
        ui->showaxis_checkbox->setChecked(_graphs[name].show_axis);
        //_scaleYAxis();
    } else if (_graphs[name].type == GraphType::digital){
        ui->yheight_dspinbox->setEnabled(false);
        ui->yoffset_dspinbox->setEnabled(false);
        ui->showaxis_checkbox->setEnabled(false);
        ui->show_checkbox->setChecked(_graphs[name].show);
    }
    if (!_prev_graph_highlighted.empty() && _graphs.find(_prev_graph_highlighted) != _graphs.end()) {
        _graphs[_prev_graph_highlighted].graph->setHighlighted(false);
    }
    _prev_graph_highlighted = name;
    _graphs[name].graph->setHighlighted(true);

    ui->plot->redrawPlot();
}

/**
 * @brief Set the zoom of the plot with accordance to x width specified in GUI and current frame
 */
void Analog_Viewer::_setZoom(){
    ui->plot->getXAxis()->setRange(_current_frame - ui->xwidth_dspinbox->value()/2, _current_frame + ui->xwidth_dspinbox->value()/2);
}

/**
 * @brief Slot to call _setZoom
 */
void Analog_Viewer::SetZoom(){
    _setZoom();
}

/**
 * @brief Slot to set the visibility of current graph when visibility checkbox is used
 */
void Analog_Viewer::GraphSetShow(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        _graphs[name].show = ui->show_checkbox->isChecked();
        _graphs[name].graph->setVisible(_graphs[name].show);
        ui->plot->redrawPlot();
    }
}

void Analog_Viewer::GraphSetShowAxis(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        if (ui->showaxis_checkbox->isChecked()){
            _graphs[name].axis->setDrawMode1(JKQTPCADMcomplete);
        } else {
            _graphs[name].axis->setDrawMode1(JKQTPCADMnone);
        };
        _graphs[name].show_axis = ui->showaxis_checkbox->isChecked();
        ui->plot->redrawPlot();
    } 
}

/**
 * @brief Click event handler to select a graph when clicked using JKQTPlotter Hit Test. Does not work on digital graphs.
 * @param x X coordinate of click
 * @param y Y coordinate of click
 * @param modifiers Keyboard modifiers
 * @param button Mouse button clicked
 */
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

/**
 * @brief Get the name of the currently selected graph in the GUI
 */
std::string Analog_Viewer::_getSelectedGraphName(){
    return ui->graphchoose_cbox->currentText().toStdString();
}

/**
 * @brief Slot to delete a graph when delete button is clicked
 */
void Analog_Viewer::GraphDelete(){
    std::string name = _getSelectedGraphName();
    if (!name.empty()) {
        removeGraph(name);
        ui->graphchoose_cbox->removeItem(ui->graphchoose_cbox->currentIndex());
    }
}

void Analog_Viewer::Alert(){
    std::cout << "Alert" << std::endl;
}