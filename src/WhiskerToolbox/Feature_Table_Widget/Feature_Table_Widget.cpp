#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"
#include "DataManager.hpp"

#include <QTableWidget>
#include <QPushButton>
#include <QStringList>

Feature_Table_Widget::Feature_Table_Widget(QWidget *parent)
        : QWidget(parent), ui(new Ui::Feature_Table_Widget) {
    ui->setupUi(this);

    connect(ui->refresh_dm_features, &QPushButton::clicked, this, &Feature_Table_Widget::_refreshFeatures);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &Feature_Table_Widget::_highlightFeature);
    connect(ui->add_feature_to_model, &QPushButton::clicked, this, &Feature_Table_Widget::_addFeature);
}

Feature_Table_Widget::~Feature_Table_Widget() {
    delete ui;
}

void Feature_Table_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {_data_manager = data_manager;}

void Feature_Table_Widget::populateTable(const std::vector<std::string>& keys) {
    ui->available_features_table->setRowCount(0);
    QStringList headers = {"Feature", "Type", "Clock"};
    ui->available_features_table->setColumnCount(3);
    ui->available_features_table->setHorizontalHeaderLabels(headers);

    int row = 0;
    for (const auto& key : keys) {
        ui->available_features_table->insertRow(row);
        ui->available_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));

        std::string type = _data_manager->getType(key);
        ui->available_features_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(type)));

        std::string clock = _data_manager->getTimeFrame(key);
        ui->available_features_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(clock)));
        row++;
    }
}

void Feature_Table_Widget::_refreshFeatures() {
    populateTable(_data_manager->getAllKeys());
}

void Feature_Table_Widget::_highlightFeature(int row, int column) {
    QTableWidgetItem* item = ui->available_features_table->item(row, column);
    if (item) {
        _highlighted_feature = item->text();
        emit featureSelected(_highlighted_feature);
    }
}

void Feature_Table_Widget::_addFeature()
{
    if (!_highlighted_feature.isEmpty()) {

        // Remove the feature from the available features table
        QList<QTableWidgetItem*> items = ui->available_features_table->findItems(_highlighted_feature, Qt::MatchExactly);
        if (!items.isEmpty()) {
            int row = items.first()->row();
            ui->available_features_table->removeRow(row);
        }

        emit addFeature(_highlighted_feature);

        // Clear the highlighted feature
        _highlighted_feature.clear();
    }
}
