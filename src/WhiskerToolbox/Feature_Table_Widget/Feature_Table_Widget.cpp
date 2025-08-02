#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"

#include "DataManager/utils/color.hpp"
#include "DataManager.hpp"

#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <qcheckbox.h>

#include <iostream>


Feature_Table_Widget::Feature_Table_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Feature_Table_Widget) {
    ui->setupUi(this);

    // Set font sizes - increase table content font for better readability
    QFont headerFont = ui->available_features_table->horizontalHeader()->font();
    headerFont.setPointSize(8);// Increased from 6 to 8
    ui->available_features_table->horizontalHeader()->setFont(headerFont);

    // Set larger font for table content
    QFont tableFont = ui->available_features_table->font();
    tableFont.setPointSize(9);// Set table content font to 9pt
    ui->available_features_table->setFont(tableFont);

    // Set uniform row spacing
    ui->available_features_table->verticalHeader()->setDefaultSectionSize(25);
    ui->available_features_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // Set equal column widths
    ui->available_features_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Apply dark mode compatible styling to maintain blue selection highlighting
    // This prevents checkboxes from interfering with row selection colors
    ui->available_features_table->setStyleSheet(
            "QTableWidget::item:selected {"
            "    background-color: #0078d4;"// Blue selection background
            "    color: white;"
            "}"
            "QTableWidget::item:selected:focus {"
            "    background-color: #106ebe;"// Slightly darker blue when focused
            "    color: white;"
            "}"
            "QCheckBox {"
            "    background-color: transparent;"// Transparent background for checkboxes
            "    color: white;"
            "}"
            "QCheckBox:checked {"
            "    background-color: transparent;"
            "}"
            "QCheckBox:unchecked {"
            "    background-color: transparent;"
            "}"
            "QCheckBox::indicator {"
            "    width: 13px;"
            "    height: 13px;"
            "}"
            "QCheckBox::indicator:unchecked {"
            "    border: 1px solid #cccccc;"
            "    background-color: #2a2a2a;"
            "}"
            "QCheckBox::indicator:checked {"
            "    border: 1px solid #0078d4;"
            "    background-color: #0078d4;"
            "}");

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

void Feature_Table_Widget::_addFeatureName(std::string const & key, int row, int col) {
    ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(key)));
}

void Feature_Table_Widget::_addFeatureType(std::string const & key, int row, int col) {

    std::string const type = convert_data_type_to_string(_data_manager->getType(key));
    ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(type)));
}

void Feature_Table_Widget::_addFeatureClock(std::string const & key, int row, int col) {

    std::string const clock = _data_manager->getTimeFrame(key);
    ui->available_features_table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(clock)));
}

void Feature_Table_Widget::_addFeatureElements(std::string const & key, int row, int col) {

    static_cast<void>(key);

    ui->available_features_table->setItem(row, col, new QTableWidgetItem("1"));
}

void Feature_Table_Widget::_addFeatureEnabled(std::string const & key, int row, int col) {
    auto checkboxItem = new QCheckBox();
    checkboxItem->setCheckState(Qt::Unchecked);

    // Center the checkbox in the cell - using a simpler approach
    checkboxItem->setAttribute(Qt::WA_TranslucentBackground);

    ui->available_features_table->setCellWidget(row, col, checkboxItem);

    connect(checkboxItem, &QCheckBox::stateChanged, [this, key](int state) {
        if (state == Qt::Checked) {
            emit addFeature(QString::fromStdString(key));
        } else {
            emit removeFeature(QString::fromStdString(key));
        }

        // Also emit featureSelected signal to ensure the stacked widget shows the correct pane
        // when user directly clicks the checkbox
        _highlighted_feature = QString::fromStdString(key);
        emit featureSelected(_highlighted_feature);
    });
}

void Feature_Table_Widget::populateTable() {
    ui->available_features_table->setRowCount(0);
    ui->available_features_table->setColumnCount(static_cast<int>(_columns.size()));
    ui->available_features_table->setHorizontalHeaderLabels(_columns);

    // Get all keys
    auto const allKeys = _data_manager->getAllKeys();

    for (auto const & key: allKeys) {

        if (!_type_filters.empty()) {
            auto const type = _data_manager->getType(key);
            if (std::find(_type_filters.begin(), _type_filters.end(), type) == _type_filters.end()) {
                continue;
            }
        }

        int const row = ui->available_features_table->rowCount();
        ui->available_features_table->insertRow(row);
        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Feature") {
                _addFeatureName(key, row, i);
            } else if (_columns[i] == "Type") {
                _addFeatureType(key, row, i);
            } else if (_columns[i] == "Clock") {
                _addFeatureClock(key, row, i);
            } else if (_columns[i] == "Elements") {
                _addFeatureElements(key, row, i);
            } else if (_columns[i] == "Enabled") {
                _addFeatureEnabled(key, row, i);
            }
        }
    }

    // Sort the table by feature name (assuming the "Feature" column is the first column)
    int featureColumnIndex = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Feature") {
            featureColumnIndex = i;
            break;
        }
    }

    if (featureColumnIndex != -1) {
        ui->available_features_table->sortItems(featureColumnIndex, Qt::AscendingOrder);
    }
}

void Feature_Table_Widget::_refreshFeatures() {
    populateTable();
}

void Feature_Table_Widget::_highlightFeature(int row, int column) {

    static_cast<void>(column);

    // Always get the feature name from the "Feature" column (typically column 0)
    // regardless of which column was clicked
    int featureColumnIndex = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Feature") {
            featureColumnIndex = i;
            break;
        }
    }

    if (featureColumnIndex != -1) {
        QTableWidgetItem * featureItem = ui->available_features_table->item(row, featureColumnIndex);
        if (featureItem) {
            _highlighted_feature = featureItem->text();
            emit featureSelected(_highlighted_feature);
        }
    }
}
