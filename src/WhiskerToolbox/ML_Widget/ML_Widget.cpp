
#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"
#include "mlpack_conversion.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "utils/qt_utilities.hpp"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
#include <iostream>

ML_Widget::ML_Widget(std::shared_ptr<DataManager> data_manager,
                     TimeScrollBar* time_scrollbar,
                     MainWindow* mainwindow,
                     QWidget *parent) :
        QMainWindow(parent),
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{mainwindow},
        ui(new Ui::ML_Widget)
{
    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({"AnalogTimeSeries", "DigitalIntervalSeries","PointData"});

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](const QString& feature) {
        ML_Widget::_addFeatureToModel(feature, true);
    });
    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](const QString& feature) {
        ML_Widget::_addFeatureToModel(feature, false);
    });
}

ML_Widget::~ML_Widget() {
    delete ui;
}

void ML_Widget::openWidget() {
    std::cout << "ML Widget Opened" << std::endl;
    ui->feature_table_widget->populateTable();
    this->show();
}

void ML_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
}

void ML_Widget::_handleFeatureSelected(const QString& feature)
{
    _highlighted_available_feature = feature;
}

void ML_Widget::_addFeatureToModel(const QString& feature, bool enabled)
{
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedFeature(const std::string key) {
    if (_data_manager->getType(key) == "AnalogTimeSeries") {
        //ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (_data_manager->getType(key) == "PointData") {
        //ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (_data_manager->getType(key) == "DigitalIntervalSeries") {
        //ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}
