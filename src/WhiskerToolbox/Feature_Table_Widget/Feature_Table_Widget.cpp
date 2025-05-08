#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"

#include "Color_Widget/Color_Widget.hpp"
#include "DataManager.hpp"
#include "utils/color.hpp"

#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <qcheckbox.h>

#include <iostream>

std::vector<std::string> default_colors = {"#ff0000", // Red
                                           "#008000", // Green
                                           "#00ffff", // Cyan
                                           "#ff00ff", // Magenta
                                           "#ffff00"};// Yellow

Feature_Table_Widget::Feature_Table_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Feature_Table_Widget) {
    ui->setupUi(this);

    QFont font = ui->available_features_table->horizontalHeader()->font();
    font.setPointSize(6);
    ui->available_features_table->horizontalHeader()->setFont(font);

    //connect(ui->refresh_dm_features, &QPushButton::clicked, this, &Feature_Table_Widget::_refreshFeatures);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &Feature_Table_Widget::_highlightFeature);
}

Feature_Table_Widget::~Feature_Table_Widget() {
    delete ui;
}

void Feature_Table_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    // I want to add a callback to the data manager to be called when the data manager is updated

    _data_manager->addObserver([this]() {
        _refreshFeatures();
    });
}

void Feature_Table_Widget::_addFeatureName(std::string const & key, int row, int col, bool group) {
    ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(key)));
}

void Feature_Table_Widget::_addFeatureType(std::string const & key, int row, int col, bool group) {
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem("Group"));
    } else {
        std::string const type = convert_data_type_to_string(_data_manager->getType(key));
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(type)));
    }
}

void Feature_Table_Widget::_addFeatureClock(std::string const & key, int row, int col, bool group) {
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(""));
    } else {
        std::string const clock = _data_manager->getTimeFrame(key);
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(clock)));
    }
}

void Feature_Table_Widget::_addFeatureElements(std::string const & key, int row, int col, bool group) {
    if (group) {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::number(_data_manager->getDataGroup(key).size())));
    } else {
        ui->available_features_table->setItem(row, col, new QTableWidgetItem("1"));
    }
}

void Feature_Table_Widget::_addFeatureEnabled(std::string const & key, int row, int col, bool group) {
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

void Feature_Table_Widget::_addFeatureColor(std::string const & key, int row, int col, bool group) {

    auto colorWidget = new ColorWidget();
    if (row < default_colors.size()) {
        colorWidget->setText(QString::fromStdString(default_colors[row]));
    } else {
        colorWidget->setText(QString::fromStdString(generateRandomColor()));
    }
    ui->available_features_table->setCellWidget(row, col, colorWidget);

    connect(colorWidget, &ColorWidget::colorChanged, [this, key](QString const & color) {
        std::cout << "Color received as " << color.toStdString() << std::endl;
        colorChange(QString::fromStdString(key), color);
    });
}

//If there is a column named "Color" in the table, this function should return the color of the feature
std::string Feature_Table_Widget::getFeatureColor(std::string const & key) {
    // Get the color of the feature

    //Find row by key
    int row = -1;
    for (int i = 0; i < ui->available_features_table->rowCount(); i++) {
        if (ui->available_features_table->item(i, 0)->text() == QString::fromStdString(key)) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        return "";
    }
    int col = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Color") {
            col = i;
            break;
        }
    }
    auto colorWidget = ui->available_features_table->cellWidget(row, col);
    if (colorWidget) {
        return dynamic_cast<ColorWidget *>(colorWidget)->text().toStdString();
    }
    return "";
}

void Feature_Table_Widget::setFeatureColor(std::string const & key, std::string const & hex_color) {
    //Find row by key
    int row = -1;
    for (int i = 0; i < ui->available_features_table->rowCount(); i++) {
        if (ui->available_features_table->item(i, 0)->text() == QString::fromStdString(key)) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        return;
    }
    int col = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Color") {
            col = i;
            break;
        }
    }
    auto colorWidget = ui->available_features_table->cellWidget(row, col);
    if (colorWidget) {
        dynamic_cast<ColorWidget *>(colorWidget)->setText(QString::fromStdString(hex_color));
    }
}

void Feature_Table_Widget::populateTable() {
    ui->available_features_table->setRowCount(0);
    ui->available_features_table->setColumnCount(static_cast<int>(_columns.size()));
    ui->available_features_table->setHorizontalHeaderLabels(_columns);

    // Get all keys and group names
    auto allKeys = _data_manager->getAllKeys();
    auto groupNames = _data_manager->getDataGroupNames();

    // Add group names to the table
    for (auto const & groupName: groupNames) {

        if (!_type_filters.empty()) {
            std::string const type = convert_data_type_to_string(_data_manager->getType(groupName));
            if (std::find(_type_filters.begin(), _type_filters.end(), type) == _type_filters.end()) {
                continue;
            }
        }

        int const row = ui->available_features_table->rowCount();
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
    for (auto const & key: allKeys) {
        bool isInGroup = false;
        for (auto const & groupName: groupNames) {
            auto groupKeys = _data_manager->getDataGroup(groupName);
            if (std::find(groupKeys.begin(), groupKeys.end(), key) != groupKeys.end()) {
                isInGroup = true;
                break;
            }
        }


        if (!_type_filters.empty()) {
            std::string const type = convert_data_type_to_string(_data_manager->getType(key));
            if (std::find(_type_filters.begin(), _type_filters.end(), type) == _type_filters.end()) {
                continue;
            }
        }

        if (!isInGroup) {
            int const row = ui->available_features_table->rowCount();
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
    QTableWidgetItem * item = ui->available_features_table->item(row, column);
    if (item) {
        _highlighted_feature = item->text();
        emit featureSelected(_highlighted_feature);
    }
}
