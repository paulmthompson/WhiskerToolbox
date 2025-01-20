#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"
#include "DataManager.hpp"

#include "Color_Widget.hpp"

#include <QTableWidget>
#include <QPushButton>
#include <QStringList>
#include <qcheckbox.h>

#include <iostream>

Feature_Table_Widget::Feature_Table_Widget(QWidget *parent)
        : QWidget(parent),
        ui(new Ui::Feature_Table_Widget) {
    ui->setupUi(this);

    QFont font = ui->available_features_table->horizontalHeader()->font();
    font.setPointSize( 6 );
    ui->available_features_table->horizontalHeader()->setFont( font );

    connect(ui->refresh_dm_features, &QPushButton::clicked, this, &Feature_Table_Widget::_refreshFeatures);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &Feature_Table_Widget::_highlightFeature);
}

Feature_Table_Widget::~Feature_Table_Widget() {
    delete ui;
}

void Feature_Table_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    // I want to add a callback to the data manager to be called when the data manager is updated

    _data_manager->addObserver([this]() {
        _refreshFeatures();
    });
}

void Feature_Table_Widget::_addFeatureName(std::string key, int row, int col, bool group)
{
    ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(key)));
}

void Feature_Table_Widget::_addFeatureType(std::string key, int row, int col, bool group)
{
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem("Group"));
    } else {
        std::string type = _data_manager->getType(key);
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(type)));
    }
}

void Feature_Table_Widget::_addFeatureClock(std::string key, int row, int col, bool group)
{
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(""));
    } else {
        std::string clock = _data_manager->getTimeFrame(key);
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(clock)));
    }
}

void Feature_Table_Widget::_addFeatureElements(std::string key, int row, int col, bool group)
{
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::number(_data_manager->getDataGroup(key).size())));
    } else {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem("1"));
    }
}

void Feature_Table_Widget::_addFeatureEnabled(std::string key, int row, int col, bool group)
{
    auto checkboxItem = new QCheckBox();
    checkboxItem->setCheckState(Qt::Unchecked);
    ui->available_features_table->setCellWidget(row, col, checkboxItem);

    connect(checkboxItem, &QCheckBox::stateChanged, [this, key](int state) {
        if (state == Qt::Checked) {
            emit addFeature(QString::fromStdString(key));
        } else {
            emit removeFeature(QString::fromStdString(key));
        }
    });
}

void Feature_Table_Widget::_addFeatureColor(std::string key, int row, int col, bool group) {

    auto colorWidget = new ColorWidget();
    colorWidget->setText("#000000"); // Default color
    ui->available_features_table->setCellWidget(row, col, colorWidget);

    connect(colorWidget, &ColorWidget::colorChanged, [this, key](const QString &color) {

        std::cout << "Color received as " << color.toStdString() << std::endl;
        colorChange(QString::fromStdString(key), color);
    });

}

void Feature_Table_Widget::populateTable() {
    ui->available_features_table->setRowCount(0);
    ui->available_features_table->setColumnCount(_columns.size());
    ui->available_features_table->setHorizontalHeaderLabels(_columns);

    // Get all keys and group names
    auto allKeys = _data_manager->getAllKeys();
    auto groupNames = _data_manager->getDataGroupNames();

    // Add group names to the table
    for (const auto& groupName : groupNames) {
        int row = ui->available_features_table->rowCount();
        ui->available_features_table->insertRow(row);

        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Feature") {
                _addFeatureName(groupName, row, i, true);
            } else if (_columns[i] == "Type") {
                _addFeatureType(groupName, row, i, true);
            } else if (_columns[i] == "Clock") {
                _addFeatureClock(groupName, row, i, true);
            } else if (_columns[i] == "Elements") {
                _addFeatureElements(groupName, row, i, true);
            } else if (_columns[i] == "Enabled") {
                _addFeatureEnabled(groupName, row, i, true);
            } else if (_columns[i] == "Color") {
                _addFeatureColor(groupName, row, i, true);
            }
        }
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
            for (int i = 0; i < _columns.size(); i++) {
                if (_columns[i] == "Feature") {
                    _addFeatureName(key, row, i, false);
                } else if (_columns[i] == "Type") {
                    _addFeatureType(key, row, i, false);
                } else if (_columns[i] == "Clock") {
                    _addFeatureClock(key, row, i, false);
                } else if (_columns[i] == "Elements") {
                    _addFeatureElements(key, row, i, false);
                } else if (_columns[i] == "Enabled") {
                    _addFeatureEnabled(key, row, i, false);
                } else if (_columns[i] == "Color") {
                    _addFeatureColor(key, row, i, false);
                }
            }
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
