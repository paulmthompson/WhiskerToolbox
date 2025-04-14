
#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "DataManager.hpp"

#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QTableWidget>
#include <QWheelEvent>

#include <iostream>

DataViewer_Widget::DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     TimeScrollBar * time_scrollbar,
                                     MainWindow * main_window,
                                     QWidget * parent)
    : QMainWindow(parent),
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      _main_window{main_window},
      ui(new Ui::DataViewer_Widget) {

    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Color", "Enabled", "Type", "Clock", "Elements"});
    ui->feature_table_widget->setTypeFilter({"AnalogTimeSeries", "DigitalEventSeries", "DigitalIntervalSeries"});

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataViewer_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](QString const & feature) {
        DataViewer_Widget::_addFeatureToModel(feature, true);
    });
    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](QString const & feature) {
        DataViewer_Widget::_addFeatureToModel(feature, false);
    });

    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleXAxisSamplesChanged);

    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged, this, &DataViewer_Widget::_updateGlobalScale);


    connect(time_scrollbar, &TimeScrollBar::timeChanged, this, &DataViewer_Widget::_updatePlot);

    //We should alwasy get the master clock because we plot
    // Check for master clock
    auto time_keys = _data_manager->getTimeFrameKeys();
    // if timekeys doesn't have master, we should throw an error
    if (std::find(time_keys.begin(), time_keys.end(), "master") == time_keys.end()) {
        std::cout << "No master clock found in DataManager" << std::endl;
        _time_frame = _data_manager->getTime("time");
    } else {
        _time_frame = _data_manager->getTime("master");
    }

    std::cout << "Setting GL limit to " << _time_frame->getTotalFrameCount() << std::endl;
    ui->openGLWidget->setXLimit(_time_frame->getTotalFrameCount());
}

DataViewer_Widget::~DataViewer_Widget() {
    delete ui;
}

void DataViewer_Widget::openWidget() {
    std::cout << "DataViewer Widget Opened" << std::endl;
    ui->feature_table_widget->populateTable();
    this->show();

    _updateLabels();
}

void DataViewer_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;
}

void DataViewer_Widget::_updatePlot(int time) {
    //std::cout << "Time is " << time;
    time = _data_manager->getTime("time")->getTimeAtIndex(time);
    //std::cout << ""
    ui->openGLWidget->updateCanvas(time);

    _updateLabels();
}


void DataViewer_Widget::_addFeatureToModel(QString const & feature, bool enabled) {

    if (enabled) {
        _plotSelectedFeature(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void DataViewer_Widget::_plotSelectedFeature(std::string const & key) {

    auto color = ui->feature_table_widget->getFeatureColor(key);

    if (_data_manager->getType(key) == "AnalogTimeSeries") {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);

    } else if (_data_manager->getType(key) == "DigitalEventSeries") {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);
    } else if (_data_manager->getType(key) == "DigitalIntervalSeries") {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}

void DataViewer_Widget::_removeSelectedFeature(std::string const & key) {
    if (_data_manager->getType(key) == "AnalogTimeSeries") {
        ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (_data_manager->getType(key) == "DigitalEventSeries") {
        ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (_data_manager->getType(key) == "DigitalIntervalSeries") {
        ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}

void DataViewer_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;
}

void DataViewer_Widget::_handleXAxisSamplesChanged(int value) {
    ui->openGLWidget->changeZoom(value);
}

void DataViewer_Widget::updateXAxisSamples(int value) {
    ui->x_axis_samples->blockSignals(true);
    ui->x_axis_samples->setValue(value);
    ui->x_axis_samples->blockSignals(false);
}

void DataViewer_Widget::_updateGlobalScale(double scale) {
    ui->openGLWidget->setGlobalScale(static_cast<float>(scale));
}

void DataViewer_Widget::wheelEvent(QWheelEvent * event) {
    int const numDegrees = event->angleDelta().y() / 8;
    int const numSteps = numDegrees / 15;
    int const zoomFactor = _time_frame->getTotalFrameCount() / 10000;

    auto curent_zoom = ui->x_axis_samples->value();
    ui->openGLWidget->changeZoom(static_cast<int64_t>(numSteps) * zoomFactor);

    auto new_zoom = -1 * zoomFactor * numSteps + curent_zoom;

    if (new_zoom < 1) {
        new_zoom = 1;
    }

    updateXAxisSamples(new_zoom);

    _updateLabels();
}

void DataViewer_Widget::_updateLabels() {
    auto x_axis = ui->openGLWidget->getXAxis();
    ui->neg_x_label->setText(QString::number(x_axis.getStart()));
    ui->pos_x_label->setText(QString::number(x_axis.getEnd()));
}
