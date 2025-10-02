#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"

#include "DataManager.hpp"
#include "DataManager/utils/color.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <qcheckbox.h>

#include <iostream>


Feature_Table_Widget::Feature_Table_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Feature_Table_Widget) {
    ui->setupUi(this);

    // Disable horizontal scrollbar
    ui->available_features_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set font sizes - increase table content font for better readability
    QFont headerFont = ui->available_features_table->horizontalHeader()->font();
    headerFont.setPointSize(8);// Increased from 6 to 8
    ui->available_features_table->horizontalHeader()->setFont(headerFont);

    // Set larger font for table content
    QFont tableFont = ui->available_features_table->font();
    tableFont.setPointSize(9);// Set table content font to 9pt
    ui->available_features_table->setFont(tableFont);

    // Set uniform row spacing - use Fixed instead of Stretch to allow scrolling
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
            "    background-color: transparent;"
            "    color: white;"
            "    spacing: 0px;"
            "}"
            "QCheckBox::indicator {"
            "    width: 12px;"
            "    height: 12px;"
            "    border-radius: 2px;"
            "    border: 1px solid #ffffff;"
            "}"
            "QCheckBox::indicator:unchecked {"
            "    background-color: #2a2a2a;"
            "}"
            "QCheckBox::indicator:checked {"
            "    background-color: #ffffff;"
            "}");

    //connect(ui->refresh_dm_features, &QPushButton::clicked, this, &Feature_Table_Widget::_refreshFeatures);
    connect(ui->available_features_table, &QTableWidget::cellClicked, this, &Feature_Table_Widget::_highlightFeature);
}

Feature_Table_Widget::~Feature_Table_Widget() {
    delete ui;
}

void Feature_Table_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {

    if (!data_manager) {
        std::cout << "Feature_Table_Widget::setDataManager - Data manager is null" << std::endl;
        return;
    }

    _data_manager = std::move(data_manager);
    // I want to add a callback to the data manager to be called when the data manager is updated

    _data_manager->addObserver([this]() {
        _refreshFeatures();
    });
}

void Feature_Table_Widget::_addFeatureName(std::string const & key, int row, int col) {
    auto * item = new QTableWidgetItem(QString::fromStdString(key));
    item->setTextAlignment(Qt::AlignCenter);
    ui->available_features_table->setItem(row, col, item);
}

void Feature_Table_Widget::_addFeatureType(std::string const & key, int row, int col) {

    if (!_data_manager) {
        std::cout << "Feature_Table_Widget::_addFeatureType - Data manager is null" << std::endl;
        return;
    }

    std::string const type = convert_data_type_to_string(_data_manager->getType(key));
    auto * item = new QTableWidgetItem(QString::fromStdString(type));
    item->setTextAlignment(Qt::AlignCenter);
    ui->available_features_table->setItem(row, col, item);
}

void Feature_Table_Widget::_addFeatureClock(std::string const & key, int row, int col) {

    if (!_data_manager) {
        std::cout << "Feature_Table_Widget::_addFeatureClock - Data manager is null" << std::endl;
        return;
    }

    std::string const clock = _data_manager->getTimeKey(key).str();
    auto * item = new QTableWidgetItem(QString::fromStdString(clock));
    item->setTextAlignment(Qt::AlignCenter);
    ui->available_features_table->setItem(row, col, item);
}

void Feature_Table_Widget::_addFeatureElements(std::string const & key, int row, int col) {

    static_cast<void>(key);
    auto * item = new QTableWidgetItem("1");
    item->setTextAlignment(Qt::AlignCenter);
    ui->available_features_table->setItem(row, col, item);
}

void Feature_Table_Widget::_addFeatureEnabled(std::string const & key, int row, int col) {
    // Create a widget to center the checkbox
    QWidget * centerWidget = new QWidget();
    QHBoxLayout * layout = new QHBoxLayout(centerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);

    // Create the checkbox - standard QCheckBox with no text
    auto checkboxItem = new QCheckBox();

    // Check if this feature should be enabled based on saved state
    bool const shouldBeEnabled{_enabled_features.find(key) != _enabled_features.end()};
    checkboxItem->setCheckState(shouldBeEnabled ? Qt::Checked : Qt::Unchecked);

    // Remove any text - we only want the checkbox indicator
    checkboxItem->setText("");

    // Add checkbox to the layout
    layout->addWidget(checkboxItem);

    // Set the widget in the cell
    ui->available_features_table->setCellWidget(row, col, centerWidget);

    // Connect the state change signal
    connect(checkboxItem, &QCheckBox::stateChanged, [this, key, checkboxItem](int state) {
        if (state == Qt::Checked) {
            // Feature is enabled
            _enabled_features.insert(key);
            emit addFeature(QString::fromStdString(key));

            // Select the feature when checked
            _highlighted_feature = QString::fromStdString(key);
            _selected_feature_for_restoration = key;
            emit featureSelected(_highlighted_feature);
        } else {
            // Feature is disabled
            _enabled_features.erase(key);
            emit removeFeature(QString::fromStdString(key));

            // We don't change selection when unchecked
        }
    });
}

void Feature_Table_Widget::populateTable() {

    if (!_data_manager) {
        std::cout << "Feature_Table_Widget::populateTable - Data manager is null" << std::endl;
        return;
    }

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

    // Don't adjust table height - let it use the constraints from the UI file
    // This allows the scrollbar to appear when there are too many rows to fit

    // Restore state after rebuilding the table
    _restoreState();

    // Force layout update
    updateGeometry();
}

void Feature_Table_Widget::_refreshFeatures() {
    _saveCurrentState();
    populateTable();
}

void Feature_Table_Widget::_highlightFeature(int row, int column) {

    static_cast<void>(column);

    // Verify row is valid
    if (row < 0 || row >= ui->available_features_table->rowCount()) {
        std::cout << "Invalid row index: " << row << std::endl;
        return;
    }

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
        // Verify column index is valid
        if (featureColumnIndex >= ui->available_features_table->columnCount()) {
            std::cout << "Feature column index out of range: " << featureColumnIndex << std::endl;
            return;
        }

        QTableWidgetItem * featureItem = ui->available_features_table->item(row, featureColumnIndex);
        if (featureItem) {
            _highlighted_feature = featureItem->text();
            _selected_feature_for_restoration = _highlighted_feature.toStdString();
            std::cout << "Emitting featureSelected: " << _highlighted_feature.toStdString() << std::endl;
            emit featureSelected(_highlighted_feature);
        } else {
            std::cout << "Feature item is null at row " << row << ", column " << featureColumnIndex << std::endl;
        }
    } else {
        std::cout << "Feature column not found in columns list" << std::endl;
    }
}


void Feature_Table_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);

    // Prevent infinite resize loops
    if (_is_resizing) {
        return;
    }
    _is_resizing = true;

    // Make the table widget take up the full width of the widget minus margins
    ui->available_features_table->setFixedWidth(this->width());

    // Don't recalculate height - let the UI constraints handle it
    // This allows the scrollbar to appear when there are too many rows to fit

    _is_resizing = false;
}

void Feature_Table_Widget::_saveCurrentState() {
    // Clear previous state
    _enabled_features.clear();
    _selected_feature_for_restoration.clear();

    // Save currently highlighted feature
    if (!_highlighted_feature.isEmpty()) {
        _selected_feature_for_restoration = _highlighted_feature.toStdString();
    }

    // Save enabled features by iterating through the table
    for (int row = 0; row < ui->available_features_table->rowCount(); ++row) {
        // Find the feature name column
        int featureColumnIndex = -1;
        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Feature") {
                featureColumnIndex = i;
                break;
            }
        }

        // Find the enabled column
        int enabledColumnIndex = -1;
        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Enabled") {
                enabledColumnIndex = i;
                break;
            }
        }

        if (featureColumnIndex != -1 && enabledColumnIndex != -1) {
            QTableWidgetItem * featureItem = ui->available_features_table->item(row, featureColumnIndex);
            QWidget * cellWidget = ui->available_features_table->cellWidget(row, enabledColumnIndex);

            if (featureItem && cellWidget) {
                // Find the checkbox within the cell widget
                QCheckBox * checkbox = cellWidget->findChild<QCheckBox *>();
                if (checkbox && checkbox->isChecked()) {
                    _enabled_features.insert(featureItem->text().toStdString());
                }
            }
        }
    }
}

void Feature_Table_Widget::_restoreState() {
    // Restore enabled checkboxes
    for (int row = 0; row < ui->available_features_table->rowCount(); ++row) {
        // Find the feature name column
        int featureColumnIndex = -1;
        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Feature") {
                featureColumnIndex = i;
                break;
            }
        }

        // Find the enabled column
        int enabledColumnIndex = -1;
        for (int i = 0; i < _columns.size(); i++) {
            if (_columns[i] == "Enabled") {
                enabledColumnIndex = i;
                break;
            }
        }

        if (featureColumnIndex != -1 && enabledColumnIndex != -1) {
            QTableWidgetItem * featureItem = ui->available_features_table->item(row, featureColumnIndex);
            QWidget * cellWidget = ui->available_features_table->cellWidget(row, enabledColumnIndex);

            if (featureItem && cellWidget) {
                std::string const featureName = featureItem->text().toStdString();

                // Find the checkbox within the cell widget
                QCheckBox * checkbox = cellWidget->findChild<QCheckBox *>();
                if (checkbox) {
                    // Restore checkbox state without triggering signals
                    bool const wasEnabled{_enabled_features.find(featureName) != _enabled_features.end()};
                    checkbox->blockSignals(true);
                    checkbox->setChecked(wasEnabled);
                    checkbox->blockSignals(false);
                }

                // Restore selection state
                if (featureName == _selected_feature_for_restoration) {
                    ui->available_features_table->selectRow(row);
                    _highlighted_feature = QString::fromStdString(featureName);
                }
            }
        }
    }
}
