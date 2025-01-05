
#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "DataManager.hpp"

#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "utils/qt_utilities.hpp"

#include <QTableWidget>

#include <iostream>

DataViewer_Widget::DataViewer_Widget(Media_Window *scene,
                                     std::shared_ptr<DataManager> data_manager,
                                     TimeScrollBar* time_scrollbar,
                                     MainWindow* main_window,
                                     QWidget *parent) :
        QMainWindow(parent),
        _scene{scene},
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{main_window},
        ui(new Ui::DataViewer_Widget)
{
    ui->setupUi(this);

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataViewer_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, &DataViewer_Widget::_addFeatureToModel);
    connect(ui->delete_feature_button, &QPushButton::clicked, this, &DataViewer_Widget::_deleteFeatureFromModel);
    //connect(time_scrollbar, &TimeScrollBar::timeChanged, ui->openGLWidget, &OpenGLWidget::updateCanvas);
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
    this->show();
}

void DataViewer_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
}

void DataViewer_Widget::_updatePlot(int time)
{
    //std::cout << "Time is " << time;
    time = _data_manager->getTime("time")->getTimeAtIndex(time);
    //std::cout << ""
    ui->openGLWidget->updateCanvas(time);
}

void DataViewer_Widget::_highlightModelFeature(int row, int column) {
    QTableWidgetItem* item = ui->model_features_table->item(row, column);
    if (item) {
        _highlighted_model_feature = item->text();
    }
}

void DataViewer_Widget::_addFeatureToModel(const QString& feature) {

    // Add the feature to the set of model features
    _model_features.insert(feature.toStdString());

    _refreshModelFeatures();

    // Plot the selected feature
    _plotSelectedFeature(feature.toStdString());
}

void DataViewer_Widget::_refreshModelFeatures()
{
    ui->model_features_table->setRowCount(0);
    QStringList headers = {"Feature", "Type", "Clock"};
    ui->model_features_table->setColumnCount(3);
    ui->model_features_table->setHorizontalHeaderLabels(headers);

    for (const auto& key : _model_features) {
        auto type = _data_manager->getType(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame_key = _data_manager->getTimeFrame(time_key);
        auto row = ui->model_features_table->rowCount();
        ui->model_features_table->insertRow(row);
        ui->model_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));
        ui->model_features_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(type)));
        ui->model_features_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(time_frame_key)));
    }
}

void DataViewer_Widget::_deleteFeatureFromModel() {
    if (!_highlighted_model_feature.isEmpty()) {
        // Find and remove the highlighted feature from the model features table
        QList<QTableWidgetItem*> items = ui->model_features_table->findItems(_highlighted_model_feature, Qt::MatchExactly);
        if (!items.isEmpty()) {
            int row = items.first()->row();
            ui->model_features_table->removeRow(row);
        }

        // Remove the feature from the set of model features
        _model_features.erase(_highlighted_model_feature.toStdString());

        _highlighted_model_feature.clear();

        // Refresh the available features table
        //_refreshAvailableFeatures();
    }
}

void DataViewer_Widget::_plotSelectedFeature(const std::string key) {


    if (_data_manager->getType(key) == "AnalogTimeSeries") {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addAnalogTimeSeries(series, time_frame);

    } else if (_data_manager->getType(key) == "DigitalEventSeries") {

            std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
            auto series = _data_manager->getData<DigitalEventSeries>(key);
            auto time_key = _data_manager->getTimeFrame(key);
            auto time_frame = _data_manager->getTime(time_key);
            ui->openGLWidget->addDigitalEventSeries(series, time_frame);
    } else if (_data_manager->getType(key) == "DigitalIntervalSeries") {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addDigitalIntervalSeries(series, time_frame);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}

void DataViewer_Widget::_handleFeatureSelected(const QString& feature) {
    _highlighted_available_feature = feature;
}

