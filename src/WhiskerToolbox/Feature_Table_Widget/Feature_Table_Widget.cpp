#include "Feature_Table_Widget.hpp"
#include "ui_Feature_Table_Widget.h"

#include "DataManager.hpp"
#include "DataManager/utils/color.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
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

    // Disable automatic stretching of last column - we'll handle column widths manually
    ui->available_features_table->horizontalHeader()->setStretchLastSection(false);

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
    ui->available_features_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Column widths will be set adaptively in _setAdaptiveColumnWidths()
    // after the table is populated

    // Enable alternating row colors
    ui->available_features_table->setAlternatingRowColors(true);

    // Disable grid lines
    ui->available_features_table->setShowGrid(false);

    // Apply dark mode compatible styling to maintain blue selection highlighting
    // This prevents checkboxes from interfering with row selection colors
    ui->available_features_table->setStyleSheet(
            "QTableWidget {"
            "    gridline-color: transparent;"        // Remove grid lines
            "    alternate-background-color: #313131;"// Light gray for alternating rows
            "    background-color: #2a2a2a;"          // Dark gray for regular rows
            "}"
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

    // Apply custom type name filters
    std::string displayType = type;
    if (type == "video") {
        displayType = "Video";
    } else if (type == "digital_interval") {
        displayType = "Intervals";
    } else if (type == "analog") {
        displayType = "Analog";
    } else if (type == "digital_event") {
        displayType = "Events";
    } else if (type == "images") {
        displayType = "Images";
    }
    // If no match, displayType remains as the original type string

    auto * item = new QTableWidgetItem(QString::fromStdString(displayType));
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

    // DON'T call _updateRowAppearance here - will be called after sorting in populateTable

    // Connect the state change signal
    connect(checkboxItem, &QCheckBox::stateChanged, [this, key](int state) {
        bool const isEnabled = (state == Qt::Checked);
        
        if (isEnabled) {
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
        
        // Find the current row for this feature (row may have changed due to sorting)
        int currentRow = _findRowByFeatureName(QString::fromStdString(key));
        if (currentRow != -1) {
            _updateRowAppearance(currentRow, isEnabled);
        }
    });
}

void Feature_Table_Widget::_updateRowAppearance(int row, bool enabled) {
    // Define colors for enabled and disabled text
    QColor const enabledColor(204, 204, 204);    // Normal text color (light gray)
    QColor const disabledColor(100, 100, 100);   // Faded text color (darker gray)
    
    QColor const textColor = enabled ? enabledColor : disabledColor;
    
    // Update all items in this row (except the Enabled column which has a widget)
    for (int col = 0; col < ui->available_features_table->columnCount(); ++col) {
        QTableWidgetItem * item = ui->available_features_table->item(row, col);
        if (item) {
            item->setForeground(QBrush(textColor));
        }
    }
}

int Feature_Table_Widget::_findRowByFeatureName(QString const & featureName) {
    // Find the Feature column index
    int featureColumnIndex = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Feature") {
            featureColumnIndex = i;
            break;
        }
    }
    
    if (featureColumnIndex == -1) {
        return -1;
    }
    
    // Search for the row with this feature name
    for (int row = 0; row < ui->available_features_table->rowCount(); ++row) {
        QTableWidgetItem * item = ui->available_features_table->item(row, featureColumnIndex);
        if (item && item->text() == featureName) {
            return row;
        }
    }
    
    return -1;
}

void Feature_Table_Widget::_updateAllRowAppearances() {
    // Only apply graying out if the Enabled column is present
    bool hasEnabledColumn = false;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Enabled") {
            hasEnabledColumn = true;
            break;
        }
    }
    
    if (!hasEnabledColumn) {
        // No Enabled column - all rows should appear enabled
        return;
    }
    
    // Find the Feature column index
    int featureColumnIndex = -1;
    for (int i = 0; i < _columns.size(); i++) {
        if (_columns[i] == "Feature") {
            featureColumnIndex = i;
            break;
        }
    }
    
    if (featureColumnIndex == -1) {
        return;
    }
    
    // Update appearance for all rows based on their enabled state
    for (int row = 0; row < ui->available_features_table->rowCount(); ++row) {
        QTableWidgetItem * featureItem = ui->available_features_table->item(row, featureColumnIndex);
        if (featureItem) {
            std::string const featureName = featureItem->text().toStdString();
            bool const isEnabled = (_enabled_features.find(featureName) != _enabled_features.end());
            _updateRowAppearance(row, isEnabled);
        }
    }
}

void Feature_Table_Widget::_setAdaptiveColumnWidths() {
    if (!ui->available_features_table || ui->available_features_table->columnCount() == 0) {
        return;
    }

    // Get the viewport width (this automatically accounts for scrollbar if present)
    int const tableWidth = ui->available_features_table->viewport()->width();
    int const numColumns = ui->available_features_table->columnCount();
    // Calculate the maximum content width for each column
    QFontMetrics const fm(ui->available_features_table->font());
    QFontMetrics const headerFm(ui->available_features_table->horizontalHeader()->font());

    // Track total width used by non-Feature columns
    int totalFixedWidth = 0;
    int featureColumnIndex = -1;

    // First pass: set widths for all columns except "Feature"
    for (int col = 0; col < numColumns; ++col) {
        QString const columnName = _columns[col];

        // Skip "Feature" column for now - it will get the remainder
        if (columnName == "Feature") {
            featureColumnIndex = col;
            continue;
        }

        int maxWidth = 0;

        // Check header width
        QString const headerText = ui->available_features_table->horizontalHeaderItem(col)->text();
        int const headerWidth = headerFm.horizontalAdvance(headerText) + 20;// 20px padding
        maxWidth = std::max(maxWidth, headerWidth);

        // For columns like "Type" and "Clock", also check cell content widths
        if (columnName == "Type" || columnName == "Clock") {
            for (int row = 0; row < ui->available_features_table->rowCount(); ++row) {
                QTableWidgetItem * item = ui->available_features_table->item(row, col);
                if (item) {
                    QString const text = item->text();
                    int const textWidth = fm.horizontalAdvance(text) + 20;// 20px padding
                    maxWidth = std::max(maxWidth, textWidth);
                }
            }
        }

        // IMPORTANT: Set resize mode to Fixed FIRST, then set the width
        // This prevents Qt from adjusting the column automatically
        ui->available_features_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
        ui->available_features_table->horizontalHeader()->resizeSection(col, maxWidth);
        totalFixedWidth += maxWidth;
    }

    // Second pass: give "Feature" column all remaining space
    if (featureColumnIndex != -1) {
        // Set Feature column to Stretch mode so it takes remaining space
        ui->available_features_table->horizontalHeader()->setSectionResizeMode(featureColumnIndex, QHeaderView::Stretch);
    }
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

    // Update row appearances based on enabled state (must be after sorting)
    _updateAllRowAppearances();

    // Set adaptive column widths based on content
    _setAdaptiveColumnWidths();

    // Adjust table height to show up to MAX_VISIBLE_ROWS rows (or fewer if less data)
    constexpr int MAX_VISIBLE_ROWS = 10;
    int rowHeight = ui->available_features_table->verticalHeader()->defaultSectionSize();
    int headerHeight = ui->available_features_table->horizontalHeader()->height();
    int visibleRows = std::min(ui->available_features_table->rowCount(), MAX_VISIBLE_ROWS);
    int totalHeight = (rowHeight * visibleRows) + headerHeight;
    ui->available_features_table->setMinimumHeight(totalHeight);
    ui->available_features_table->setMaximumHeight(totalHeight);

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

    // Don't set a fixed width on the table - let it use the layout
    // The table will automatically fit within the widget's bounds
    // ui->available_features_table->setFixedWidth(this->width());

    // Calculate table height to determine if scrollbar will be visible
    constexpr int MAX_VISIBLE_ROWS = 10;
    int rowHeight = ui->available_features_table->verticalHeader()->defaultSectionSize();
    int headerHeight = ui->available_features_table->horizontalHeader()->height();
    int visibleRows = std::min(ui->available_features_table->rowCount(), MAX_VISIBLE_ROWS);
    int totalHeight = (rowHeight * visibleRows) + headerHeight;

    // Set table height
    ui->available_features_table->setMinimumHeight(totalHeight);
    ui->available_features_table->setMaximumHeight(totalHeight);

    // Recalculate adaptive column widths based on new width
    // This needs to happen after setting height so scrollbar visibility is determined
    _setAdaptiveColumnWidths();

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
