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


void Feature_Table_Widget::populateTable() {
    ui->available_features_table->setRowCount(0);
    QStringList headers = {"Feature", "Type", "Clock", "Elements"};
    ui->available_features_table->setColumnCount(4);
    ui->available_features_table->setHorizontalHeaderLabels(headers);

    // Get all keys and group names
    auto allKeys = _data_manager->getAllKeys();
    auto groupNames = _data_manager->getDataGroupNames();

    // Add group names to the table
    for (const auto& groupName : groupNames) {
        int row = ui->available_features_table->rowCount();
        ui->available_features_table->insertRow(row);
        ui->available_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(groupName)));
        ui->available_features_table->setItem(row, 1, new QTableWidgetItem("Group"));
        ui->available_features_table->setItem(row, 2, new QTableWidgetItem(""));
        ui->available_features_table->setItem(row, 3, new QTableWidgetItem(QString::number(_data_manager->getDataGroup(groupName).size())));
    }

    // Add keys not in groups to the table
    for (const auto& key : allKeys) {
        bool isInGroup = false;
        for (const auto& groupName : groupNames) {
            auto groupKeys = _data_manager->getDataGroup(groupName);
            if (std::find(groupKeys.begin(), groupKeys.end(), key) != groupKeys.end()) {
                isInGroup = true;
                break;
            }
        }
        if (!isInGroup) {
            int row = ui->available_features_table->rowCount();
            ui->available_features_table->insertRow(row);
            ui->available_features_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));
            std::string type = _data_manager->getType(key);
            ui->available_features_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(type)));
            std::string clock = _data_manager->getTimeFrame(key);
            ui->available_features_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(clock)));
            ui->available_features_table->setItem(row, 3, new QTableWidgetItem("1"));
        }
    }
}

void Feature_Table_Widget::_refreshFeatures() {
    populateTable();
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
