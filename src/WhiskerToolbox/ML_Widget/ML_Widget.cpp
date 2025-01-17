
#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"
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

    connect(ui->refresh_dm_features, &QPushButton::clicked, this, &ML_Widget::_refreshAvailableFeatures);
    connect(ui->add_feature_to_model, &QPushButton::clicked, this, &ML_Widget::_addFeatureToModel);
    connect(ui->delete_feature_button, &QPushButton::clicked, this, &ML_Widget::_deleteFeatureFromModel);
    connect(ui->add_label_to_model, &QPushButton::clicked, this, &ML_Widget::_addLabelToModel);
    connect(ui->delete_label_button, &QPushButton::clicked, this, &ML_Widget::_deleteLabel);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &ML_Widget::_highlightAvailableFeature);
    connect(ui->model_features_table, &QTableWidget::cellClicked, this, &ML_Widget::_highlightModelFeature);

}

ML_Widget::~ML_Widget() {
    delete ui;
}

void ML_Widget::openWidget() {
    std::cout << "ML Widget Opened" << std::endl;
    this->show();
}

void ML_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
}

void ML_Widget::_insertRows(const std::vector<std::string>& keys) {
    int row = ui->available_features_table->rowCount();
    for (const auto& key : keys) {
        if (_model_features.find(key) == _model_features.end() && key != _selected_label.toStdString()) {
            ui->available_features_table->insertRow(row);
            ui->available_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));
            row++;
        }
    }
}

void ML_Widget::_refreshAvailableFeatures() {
    ui->available_features_table->setRowCount(0);
    QStringList headers = {"Feature"};
    ui->available_features_table->setColumnCount(1);
    ui->available_features_table->setHorizontalHeaderLabels(headers);

    _insertRows(_data_manager->getAllKeys());

}

void ML_Widget::_highlightAvailableFeature(int row, int column) {
    QTableWidgetItem* item = ui->available_features_table->item(row, column);
    if (item) {
        _highlighted_available_feature = item->text();
    }
}

void ML_Widget::_highlightModelFeature(int row, int column) {
    QTableWidgetItem* item = ui->model_features_table->item(row, column);
    if (item) {
        _highlighted_model_feature = item->text();
    }
}

void ML_Widget::_addFeatureToModel() {
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
    }
}

void ML_Widget::_deleteFeatureFromModel() {
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

void ML_Widget::_addLabelToModel() {
    if (_selected_label.isEmpty() && !_highlighted_available_feature.isEmpty()) {

        // Set the selected label
        _selected_label =  _highlighted_available_feature;
        ui->selected_label_label->setText(_selected_label);

        // Remove the feature from the available features table
        QList<QTableWidgetItem*> items = ui->available_features_table->findItems(_highlighted_available_feature, Qt::MatchExactly);
        if (!items.isEmpty()) {
            int row = items.first()->row();
            ui->available_features_table->removeRow(row);
        }

        // Clear the highlighted feature
        _highlighted_available_feature.clear();
    }
}

void ML_Widget::_deleteLabel() {
    if (!_selected_label.isEmpty()) {
        // Clear the selected label
        _selected_label.clear();
        ui->selected_label_label->clear();

        // Refresh the available features table
        _refreshAvailableFeatures();
    }
}

