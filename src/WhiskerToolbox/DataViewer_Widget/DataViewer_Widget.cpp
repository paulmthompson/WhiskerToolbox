
#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "DataManager.hpp"

#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "utils/qt_utilities.hpp"
#include "OpenGLWidget.hpp"

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

    connect(ui->refresh_dm_features, &QPushButton::clicked, this, &DataViewer_Widget::_refreshAvailableFeatures);
    connect(ui->add_feature_to_model, &QPushButton::clicked, this, &DataViewer_Widget::_addFeatureToModel);
    connect(ui->delete_feature_button, &QPushButton::clicked, this, &DataViewer_Widget::_deleteFeatureFromModel);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &DataViewer_Widget::_highlightAvailableFeature);
    connect(time_scrollbar, &TimeScrollBar::timeChanged, ui->openGLWidget, &OpenGLWidget::updateCanvas);

    //We should alwasy get the master clock because we plot
    // Check for master clock
    auto time_keys = _data_manager->getTimeFrameKeys();
    // if timekeys doesn't have master, we should throw an error
    if (std::find(time_keys.begin(), time_keys.end(), "master") == time_keys.end()) {
        std::cout << "No master clock found in DataManager" << std::endl;
        ui->openGLWidget->setXLimit(_data_manager->getTime("master")->getTotalFrameCount());
    } else {
        ui->openGLWidget->setXLimit(_data_manager->getTime("master")->getTotalFrameCount());
    }

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

void DataViewer_Widget::_insertRows(const std::vector<std::string>& keys) {

    int row = ui->available_features_table->rowCount();
    for (const auto& key : keys) {
        if (_model_features.find(key) == _model_features.end()) {
            ui->available_features_table->insertRow(row);
            ui->available_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));

            // Retrieve the type of the feature from the _data_manager
            std::string type = _data_manager->getType(key);
            ui->available_features_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(type)));

            std::string clock = _data_manager->getTimeFrame(key);
            ui->available_features_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(clock)));
            row++;
        }
    }

}

void DataViewer_Widget::_refreshAvailableFeatures() {
    ui->available_features_table->setRowCount(0);
    QStringList headers = {"Feature", "Type", "Clock"};
    ui->available_features_table->setColumnCount(3);
    ui->available_features_table->setHorizontalHeaderLabels(headers);

    _insertRows(_data_manager->getAllKeys());

}

void DataViewer_Widget::_highlightAvailableFeature(int row, int column) {
    QTableWidgetItem* item = ui->available_features_table->item(row, column);
    if (item) {
        _highlighted_available_feature = item->text();
    }
}

void DataViewer_Widget::_highlightModelFeature(int row, int column) {
    QTableWidgetItem* item = ui->model_features_table->item(row, column);
    if (item) {
        _highlighted_model_feature = item->text();
    }
}

void DataViewer_Widget::_addFeatureToModel() {
    if (!_highlighted_available_feature.isEmpty()) {
        // Ensure the model features table has the same column headers as the available features table
        build_feature_table(ui->model_features_table);

        // Add the highlighted feature to the model features table
        int row = ui->model_features_table->rowCount();
        ui->model_features_table->insertRow(row);
        ui->model_features_table->setItem(row, 0, new QTableWidgetItem(_highlighted_available_feature));

        // Add the feature to the set of model features
        _model_features.insert(_highlighted_available_feature.toStdString());

        // Remove the feature from the available features table
        QList<QTableWidgetItem*> items = ui->available_features_table->findItems(_highlighted_available_feature, Qt::MatchExactly);
        if (!items.isEmpty()) {
            int row = items.first()->row();
            ui->available_features_table->removeRow(row);
        }

        // Clear the highlighted feature
        _highlighted_available_feature.clear();

        // Plot the selected feature
        _plotSelectedFeature();
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
        _refreshAvailableFeatures();
    }
}

void DataViewer_Widget::_plotSelectedFeature() {

    // Loop through model_features
    for (const auto& key : _model_features) {
        if (_data_manager->getType(key) == "AnalogTimeSeries") {
            std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
            auto series = _data_manager->getData<AnalogTimeSeries>(key);
            auto time_key = _data_manager->getTimeFrame(key);
            auto time_frame = _data_manager->getTime(time_key);
            ui->openGLWidget->addAnalogTimeSeries(series, time_frame);
        }
    }
}

