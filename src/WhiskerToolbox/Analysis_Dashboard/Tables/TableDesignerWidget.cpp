#include "TableDesignerWidget.hpp"
#include "TableManager.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/interfaces/IColumnComputer.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

TableDesignerWidget::TableDesignerWidget(TableManager* table_manager, std::shared_ptr<DataManager> data_manager, QWidget* parent)
    : QWidget(parent),
      ui(nullptr),
      _table_manager(table_manager),
      _data_manager(data_manager),
      _main_layout(nullptr),
      _table_selection_group(nullptr),
      _table_combo(nullptr),
      _new_table_btn(nullptr),
      _delete_table_btn(nullptr),
      _table_info_group(nullptr),
      _table_name_edit(nullptr),
      _table_description_edit(nullptr),
      _save_info_btn(nullptr),
      _row_source_group(nullptr),
      _row_data_source_combo(nullptr),
      _row_info_label(nullptr),
      _column_design_group(nullptr),
      _column_splitter(nullptr),
      _column_list_widget(nullptr),
      _column_list(nullptr),
      _add_column_btn(nullptr),
      _remove_column_btn(nullptr),
      _move_up_btn(nullptr),
      _move_down_btn(nullptr),
      _column_config_widget(nullptr),
      _column_data_source_combo(nullptr),
      _column_computer_combo(nullptr),
      _column_name_edit(nullptr),
      _column_description_edit(nullptr),
      _build_group(nullptr),
      _build_table_btn(nullptr),
      _build_status_label(nullptr) {
    
    initializeUI();
    connectSignals();
    refreshTableCombo();
    refreshRowDataSourceCombo();
    clearUI();
    
    qDebug() << "TableDesignerWidget initialized";
}

TableDesignerWidget::~TableDesignerWidget() {
    // Note: Qt will handle cleanup of child widgets
}

void TableDesignerWidget::refreshAllDataSources() {
    qDebug() << "Manually refreshing all data sources...";
    refreshRowDataSourceCombo();
    refreshColumnDataSourceCombo();
    
    // If we have a selected table, refresh its info
    if (!_current_table_id.isEmpty()) {
        loadTableInfo(_current_table_id);
    }
}

void TableDesignerWidget::initializeUI() {
    _main_layout = new QVBoxLayout(this);
    _main_layout->setSpacing(8);
    _main_layout->setContentsMargins(8, 8, 8, 8);
    
    // Table Selection Section
    _table_selection_group = new QGroupBox("Table Selection", this);
    auto* table_selection_layout = new QVBoxLayout(_table_selection_group);
    
    auto* table_selection_row = new QHBoxLayout();
    _table_combo = new QComboBox(this);
    _table_combo->setMinimumWidth(150);
    _new_table_btn = new QPushButton("New", this);
    _new_table_btn->setMaximumWidth(60);
    _delete_table_btn = new QPushButton("Delete", this);
    _delete_table_btn->setMaximumWidth(60);
    _delete_table_btn->setEnabled(false);
    
    table_selection_row->addWidget(_table_combo);
    table_selection_row->addWidget(_new_table_btn);
    table_selection_row->addWidget(_delete_table_btn);
    table_selection_layout->addLayout(table_selection_row);
    
    _main_layout->addWidget(_table_selection_group);
    
    // Table Info Section
    _table_info_group = new QGroupBox("Table Information", this);
    auto* table_info_layout = new QVBoxLayout(_table_info_group);
    
    auto* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Name:", this));
    _table_name_edit = new QLineEdit(this);
    name_row->addWidget(_table_name_edit);
    table_info_layout->addLayout(name_row);
    
    table_info_layout->addWidget(new QLabel("Description:", this));
    _table_description_edit = new QTextEdit(this);
    _table_description_edit->setMaximumHeight(60);
    table_info_layout->addWidget(_table_description_edit);
    
    _save_info_btn = new QPushButton("Save Info", this);
    _save_info_btn->setEnabled(false);
    table_info_layout->addWidget(_save_info_btn);
    
    _main_layout->addWidget(_table_info_group);
    
    // Row Data Source Section
    _row_source_group = new QGroupBox("Row Data Source", this);
    auto* row_source_layout = new QVBoxLayout(_row_source_group);
    
    auto* row_combo_row = new QHBoxLayout();
    row_combo_row->addWidget(new QLabel("Source:", this));
    _row_data_source_combo = new QComboBox(this);
    _row_data_source_combo->setMinimumWidth(200);
    row_combo_row->addWidget(_row_data_source_combo);
    row_source_layout->addLayout(row_combo_row);
    
    _row_info_label = new QLabel("No row source selected", this);
    _row_info_label->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    row_source_layout->addWidget(_row_info_label);
    
    _main_layout->addWidget(_row_source_group);
    
    // Column Design Section
    _column_design_group = new QGroupBox("Column Design", this);
    auto* column_design_layout = new QVBoxLayout(_column_design_group);
    
    _column_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side - Column list
    _column_list_widget = new QWidget(this);
    auto* column_list_layout = new QVBoxLayout(_column_list_widget);
    column_list_layout->addWidget(new QLabel("Columns:", this));
    
    _column_list = new QListWidget(this);
    _column_list->setMinimumHeight(150);
    column_list_layout->addWidget(_column_list);
    
    auto* column_buttons_layout = new QHBoxLayout();
    _add_column_btn = new QPushButton("Add", this);
    _remove_column_btn = new QPushButton("Remove", this);
    _move_up_btn = new QPushButton("↑", this);
    _move_down_btn = new QPushButton("↓", this);
    
    _move_up_btn->setMaximumWidth(30);
    _move_down_btn->setMaximumWidth(30);
    
    column_buttons_layout->addWidget(_add_column_btn);
    column_buttons_layout->addWidget(_remove_column_btn);
    column_buttons_layout->addStretch();
    column_buttons_layout->addWidget(_move_up_btn);
    column_buttons_layout->addWidget(_move_down_btn);
    column_list_layout->addLayout(column_buttons_layout);
    
    _column_splitter->addWidget(_column_list_widget);
    
    // Right side - Column configuration
    _column_config_widget = new QWidget(this);
    auto* column_config_layout = new QVBoxLayout(_column_config_widget);
    column_config_layout->addWidget(new QLabel("Column Configuration:", this));
    
    auto* col_name_row = new QHBoxLayout();
    col_name_row->addWidget(new QLabel("Name:", this));
    _column_name_edit = new QLineEdit(this);
    col_name_row->addWidget(_column_name_edit);
    column_config_layout->addLayout(col_name_row);
    
    auto* col_source_row = new QHBoxLayout();
    col_source_row->addWidget(new QLabel("Data Source:", this));
    _column_data_source_combo = new QComboBox(this);
    col_source_row->addWidget(_column_data_source_combo);
    column_config_layout->addLayout(col_source_row);
    
    auto* col_computer_row = new QHBoxLayout();
    col_computer_row->addWidget(new QLabel("Computer:", this));
    _column_computer_combo = new QComboBox(this);
    col_computer_row->addWidget(_column_computer_combo);
    column_config_layout->addLayout(col_computer_row);
    
    column_config_layout->addWidget(new QLabel("Description:", this));
    _column_description_edit = new QTextEdit(this);
    _column_description_edit->setMaximumHeight(60);
    column_config_layout->addWidget(_column_description_edit);
    
    column_config_layout->addStretch();
    
    _column_splitter->addWidget(_column_config_widget);
    _column_splitter->setSizes({250, 300});
    
    column_design_layout->addWidget(_column_splitter);
    _main_layout->addWidget(_column_design_group);
    
    // Build Section
    _build_group = new QGroupBox("Build Table", this);
    auto* build_layout = new QVBoxLayout(_build_group);
    
    _build_table_btn = new QPushButton("Build Table", this);
    _build_table_btn->setEnabled(false);
    build_layout->addWidget(_build_table_btn);
    
    _build_status_label = new QLabel("No table selected", this);
    _build_status_label->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    build_layout->addWidget(_build_status_label);
    
    _main_layout->addWidget(_build_group);
    
    _main_layout->addStretch();
}

void TableDesignerWidget::connectSignals() {
    // Table selection signals
    connect(_table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onTableSelectionChanged);
    connect(_new_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onCreateNewTable);
    connect(_delete_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onDeleteTable);
    
    // Table info signals
    connect(_save_info_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onSaveTableInfo);
    
    // Row source signals
    connect(_row_data_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onRowDataSourceChanged);
    
    // Column design signals
    connect(_add_column_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onAddColumn);
    connect(_remove_column_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onRemoveColumn);
    connect(_move_up_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onMoveColumnUp);
    connect(_move_down_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onMoveColumnDown);
    
    connect(_column_data_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onColumnDataSourceChanged);
    connect(_column_computer_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onColumnComputerChanged);
    
    // Column list selection
    connect(_column_list, &QListWidget::currentRowChanged,
            this, &TableDesignerWidget::onColumnSelectionChanged);
    
    // Column configuration editing
    connect(_column_name_edit, &QLineEdit::textChanged,
            this, &TableDesignerWidget::onColumnNameChanged);
    connect(_column_description_edit, &QTextEdit::textChanged,
            this, &TableDesignerWidget::onColumnDescriptionChanged);
    
    // Build signals
    connect(_build_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onBuildTable);
    
    // TableManager signals
    if (_table_manager) {
        connect(_table_manager, &TableManager::tableCreated,
                this, &TableDesignerWidget::onTableManagerTableCreated);
        connect(_table_manager, &TableManager::tableRemoved,
                this, &TableDesignerWidget::onTableManagerTableRemoved);
        connect(_table_manager, &TableManager::tableInfoUpdated,
                this, &TableDesignerWidget::onTableManagerTableInfoUpdated);
    }
}

void TableDesignerWidget::onTableSelectionChanged() {
    int current_index = _table_combo->currentIndex();
    if (current_index < 0) {
        clearUI();
        return;
    }
    
    QString table_id = _table_combo->itemData(current_index).toString();
    if (table_id.isEmpty()) {
        clearUI();
        return;
    }
    
    _current_table_id = table_id;
    loadTableInfo(table_id);
    
    // Enable/disable controls
    _delete_table_btn->setEnabled(true);
    _save_info_btn->setEnabled(true);
    _build_table_btn->setEnabled(true);
    
    updateBuildStatus("Table selected: " + table_id);
    
    qDebug() << "Selected table:" << table_id;
}

void TableDesignerWidget::onCreateNewTable() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Table", "Enter table name:", QLineEdit::Normal, "New Table", &ok);
    
    if (!ok || name.isEmpty()) {
        return;
    }
    
    QString table_id = _table_manager->generateUniqueTableId("Table");
    
    if (_table_manager->createTable(table_id, name)) {
        // The combo will be refreshed by the signal handler
        // Set the new table as selected
        for (int i = 0; i < _table_combo->count(); ++i) {
            if (_table_combo->itemData(i).toString() == table_id) {
                _table_combo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to create table with ID: " + table_id);
    }
}

void TableDesignerWidget::onDeleteTable() {
    if (_current_table_id.isEmpty()) {
        return;
    }
    
    auto reply = QMessageBox::question(this, "Delete Table", 
                                      QString("Are you sure you want to delete table '%1'?").arg(_current_table_id),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (_table_manager->removeTable(_current_table_id)) {
            // The combo will be refreshed by the signal handler
            clearUI();
        } else {
            QMessageBox::warning(this, "Error", "Failed to delete table: " + _current_table_id);
        }
    }
}

void TableDesignerWidget::onRowDataSourceChanged() {
    QString selected = _row_data_source_combo->currentText();
    if (selected.isEmpty()) {
        _row_info_label->setText("No row source selected");
        return;
    }
    
    // Save the row source selection to the current table
    // Only save if we have a current table and we're not loading table info
    if (!_current_table_id.isEmpty() && _table_manager) {
        _table_manager->updateTableRowSource(_current_table_id, selected);
    }
    
    // Update the info label
    updateRowInfoLabel(selected);
    
    // Refresh column computer options since they depend on row selector type
    refreshColumnComputerCombo();
    
    qDebug() << "Row data source changed to:" << selected;
}

void TableDesignerWidget::onAddColumn() {
    if (_current_table_id.isEmpty()) {
        return;
    }
    
    QString column_name = QString("Column_%1").arg(_column_list->count() + 1);
    
    // Create column info and add to table manager
    ColumnInfo column_info(column_name);
    if (_table_manager->addTableColumn(_current_table_id, column_info)) {
        // Add to UI list
        auto* item = new QListWidgetItem(column_name, _column_list);
        item->setData(Qt::UserRole, _column_list->count() - 1); // Store column index
        
        _column_list->setCurrentItem(item);
        _column_name_edit->setText(column_name);
        _column_name_edit->selectAll();
        _column_name_edit->setFocus();
        
        qDebug() << "Added column:" << column_name;
    } else {
        QMessageBox::warning(this, "Error", "Failed to add column to table");
    }
}

void TableDesignerWidget::onRemoveColumn() {
    auto* current_item = _column_list->currentItem();
    if (!current_item || _current_table_id.isEmpty()) {
        return;
    }
    
    int column_index = _column_list->currentRow();
    QString column_name = current_item->text();
    
    if (_table_manager->removeTableColumn(_current_table_id, column_index)) {
        delete current_item;
        
        // Clear column configuration if no items left
        if (_column_list->count() == 0) {
            clearColumnConfiguration();
        } else {
            // Select next item or previous if at end
            int new_index = column_index;
            if (new_index >= _column_list->count()) {
                new_index = _column_list->count() - 1;
            }
            if (new_index >= 0) {
                _column_list->setCurrentRow(new_index);
            }
        }
        
        qDebug() << "Removed column:" << column_name;
    } else {
        QMessageBox::warning(this, "Error", "Failed to remove column from table");
    }
}

void TableDesignerWidget::onMoveColumnUp() {
    int current_row = _column_list->currentRow();
    if (current_row <= 0 || _current_table_id.isEmpty()) {
        return;
    }
    
    if (_table_manager->moveTableColumnUp(_current_table_id, current_row)) {
        auto* item = _column_list->takeItem(current_row);
        _column_list->insertItem(current_row - 1, item);
        _column_list->setCurrentRow(current_row - 1);
    }
}

void TableDesignerWidget::onMoveColumnDown() {
    int current_row = _column_list->currentRow();
    if (current_row < 0 || current_row >= _column_list->count() - 1 || _current_table_id.isEmpty()) {
        return;
    }
    
    if (_table_manager->moveTableColumnDown(_current_table_id, current_row)) {
        auto* item = _column_list->takeItem(current_row);
        _column_list->insertItem(current_row + 1, item);
        _column_list->setCurrentRow(current_row + 1);
    }
}

void TableDesignerWidget::onColumnDataSourceChanged() {
    refreshColumnComputerCombo();
    saveCurrentColumnConfiguration();
    qDebug() << "Column data source changed to:" << _column_data_source_combo->currentText();
}

void TableDesignerWidget::onColumnComputerChanged() {
    saveCurrentColumnConfiguration();
    qDebug() << "Column computer changed to:" << _column_computer_combo->currentText();
}

void TableDesignerWidget::onColumnSelectionChanged() {
    int current_row = _column_list->currentRow();
    if (current_row >= 0 && !_current_table_id.isEmpty()) {
        loadColumnConfiguration(current_row);
    } else {
        clearColumnConfiguration();
    }
}

void TableDesignerWidget::onColumnNameChanged() {
    saveCurrentColumnConfiguration();
    
    // Update the list item text to match
    auto* current_item = _column_list->currentItem();
    if (current_item) {
        current_item->setText(_column_name_edit->text());
    }
}

void TableDesignerWidget::onColumnDescriptionChanged() {
    saveCurrentColumnConfiguration();
}

void TableDesignerWidget::onBuildTable() {
    if (_current_table_id.isEmpty()) {
        updateBuildStatus("No table selected", true);
        return;
    }
    
    QString row_source = _row_data_source_combo->currentText();
    if (row_source.isEmpty()) {
        updateBuildStatus("No row data source selected", true);
        return;
    }
    
    if (_column_list->count() == 0) {
        updateBuildStatus("No columns defined", true);
        return;
    }
    
    try {
        // Get the table info with column configurations
        auto table_info = _table_manager->getTableInfo(_current_table_id);
        if (table_info.columns.isEmpty()) {
            updateBuildStatus("No column configurations found", true);
            return;
        }
        
        // Create the row selector
        auto row_selector = createRowSelector(row_source);
        if (!row_selector) {
            updateBuildStatus("Failed to create row selector", true);
            return;
        }
        
        // Get the data manager extension
        auto data_manager_extension = _table_manager->getDataManagerExtension();
        if (!data_manager_extension) {
            updateBuildStatus("DataManager extension not available", true);
            return;
        }
        
        // Create the TableViewBuilder
        TableViewBuilder builder(data_manager_extension);
        builder.setRowSelector(std::move(row_selector));
        
        // Add all columns
        bool all_columns_valid = true;
        for (const auto& column_info : table_info.columns) {
            if (!addColumnToBuilder(builder, column_info)) {
                updateBuildStatus(QString("Failed to create column: %1").arg(column_info.name), true);
                all_columns_valid = false;
                break;
            }
        }
        
        if (!all_columns_valid) {
            return;
        }
        
        // Build the table
        auto table_view = builder.build();
        
        // Store the built table in the TableManager
        if (_table_manager->storeBuiltTable(_current_table_id, std::move(table_view))) {
            updateBuildStatus("Table built successfully!");
            qDebug() << "Successfully built table:" << _current_table_id;
        } else {
            updateBuildStatus("Failed to store built table", true);
        }
        
    } catch (const std::exception& e) {
        updateBuildStatus(QString("Error building table: %1").arg(e.what()), true);
        qDebug() << "Exception during table building:" << e.what();
    }
}

void TableDesignerWidget::onSaveTableInfo() {
    if (_current_table_id.isEmpty()) {
        return;
    }
    
    QString name = _table_name_edit->text().trimmed();
    QString description = _table_description_edit->toPlainText().trimmed();
    
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Table name cannot be empty");
        return;
    }
    
    if (_table_manager->updateTableInfo(_current_table_id, name, description)) {
        updateBuildStatus("Table information saved");
        // Refresh the combo to show updated name
        refreshTableCombo();
        // Restore selection
        for (int i = 0; i < _table_combo->count(); ++i) {
            if (_table_combo->itemData(i).toString() == _current_table_id) {
                _table_combo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to save table information");
    }
}

void TableDesignerWidget::onTableManagerTableCreated(const QString& table_id) {
    refreshTableCombo();
    qDebug() << "Table created signal received:" << table_id;
}

void TableDesignerWidget::onTableManagerTableRemoved(const QString& table_id) {
    refreshTableCombo();
    if (_current_table_id == table_id) {
        _current_table_id.clear();
        clearUI();
    }
    qDebug() << "Table removed signal received:" << table_id;
}

void TableDesignerWidget::onTableManagerTableInfoUpdated(const QString& table_id) {
    if (_current_table_id == table_id) {
        loadTableInfo(table_id);
    }
    qDebug() << "Table info updated signal received:" << table_id;
}

void TableDesignerWidget::refreshTableCombo() {
    _table_combo->clear();
    
    auto table_infos = _table_manager->getAllTableInfo();
    for (const auto& info : table_infos) {
        _table_combo->addItem(info.name, info.id);
    }
    
    if (_table_combo->count() == 0) {
        _table_combo->addItem("(No tables available)", "");
    }
}

void TableDesignerWidget::refreshRowDataSourceCombo() {
    _row_data_source_combo->clear();
    
    if (!_table_manager) {
        qDebug() << "refreshRowDataSourceCombo: No table manager";
        return;
    }
    
    auto data_sources = getAvailableDataSources();
    qDebug() << "refreshRowDataSourceCombo: Found" << data_sources.size() << "data sources:" << data_sources;
    
    for (const QString& source : data_sources) {
        _row_data_source_combo->addItem(source);
    }
    
    if (_row_data_source_combo->count() == 0) {
        _row_data_source_combo->addItem("(No data sources available)");
        qDebug() << "refreshRowDataSourceCombo: No data sources available";
    }
}

void TableDesignerWidget::refreshColumnDataSourceCombo() {
    _column_data_source_combo->clear();
    
    if (!_table_manager) {
        return;
    }
    
    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        return;
    }
    
    // Add AnalogTimeSeries data sources (continuous signals)
    auto analog_keys = _data_manager->getKeys<AnalogTimeSeries>();
    for (const auto& key : analog_keys) {
        _column_data_source_combo->addItem(QString("Analog: %1").arg(QString::fromStdString(key)), 
                                           QString("analog:%1").arg(QString::fromStdString(key)));
    }
    
    // Add DigitalEventSeries data sources (discrete events)
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();
    for (const auto& key : event_keys) {
        _column_data_source_combo->addItem(QString("Events: %1").arg(QString::fromStdString(key)), 
                                           QString("events:%1").arg(QString::fromStdString(key)));
    }
    
    // Add DigitalIntervalSeries data sources (time intervals)
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    for (const auto& key : interval_keys) {
        _column_data_source_combo->addItem(QString("Intervals: %1").arg(QString::fromStdString(key)), 
                                           QString("intervals:%1").arg(QString::fromStdString(key)));
    }
    
    // Add PointData sources with component access (X, Y coordinates)
    auto point_keys = _data_manager->getKeys<PointData>();
    for (const auto& key : point_keys) {
        _column_data_source_combo->addItem(QString("Points X: %1").arg(QString::fromStdString(key)), 
                                           QString("points_x:%1").arg(QString::fromStdString(key)));
        _column_data_source_combo->addItem(QString("Points Y: %1").arg(QString::fromStdString(key)), 
                                           QString("points_y:%1").arg(QString::fromStdString(key)));
    }
    
    // Add existing table columns as potential data sources
    auto table_columns = getAvailableTableColumns();
    for (const QString& column : table_columns) {
        _column_data_source_combo->addItem(QString("Table Column: %1").arg(column), 
                                           QString("table:%1").arg(column));
    }
    
    if (_column_data_source_combo->count() == 0) {
        _column_data_source_combo->addItem("(No data sources available)", "");
    }
    
    qDebug() << "Column data sources: " << analog_keys.size() << "analog," 
             << event_keys.size() << "events," << interval_keys.size() << "intervals," 
             << point_keys.size() << "point series," << table_columns.size() << "table columns";
}

void TableDesignerWidget::refreshColumnComputerCombo() {
    _column_computer_combo->clear();
    
    if (!_table_manager) {
        return;
    }
    
    QString row_source = _row_data_source_combo->currentText();
    QString column_source = _column_data_source_combo->currentData().toString();
    
    if (row_source.isEmpty()) {
        _column_computer_combo->addItem("(Select row source first)", "");
        return;
    }
    
    if (column_source.isEmpty()) {
        _column_computer_combo->addItem("(Select column data source first)", "");
        return;
    }
    
    // Convert row source to RowSelectorType
    RowSelectorType row_selector_type = RowSelectorType::Interval; // Default
    if (row_source.startsWith("TimeFrame: ")) {
        row_selector_type = RowSelectorType::Interval; // TimeFrames define intervals
    } else if (row_source.startsWith("Events: ")) {
        row_selector_type = RowSelectorType::Timestamp; // Events define timestamps
    } else if (row_source.startsWith("Intervals: ")) {
        row_selector_type = RowSelectorType::Interval; // Intervals are intervals
    }
    
    // Create DataSourceVariant from column source
    DataSourceVariant data_source_variant;
    bool valid_source = false;
    
    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        _column_computer_combo->addItem("(DataManager not available)", "");
        return;
    }
    
    // Parse column source and create appropriate adapter
    if (column_source.startsWith("analog:")) {
        QString source_name = column_source.mid(7); // Remove "analog:" prefix
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString());
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
        }
    } else if (column_source.startsWith("events:")) {
        QString source_name = column_source.mid(7); // Remove "events:" prefix
        auto event_source = data_manager_extension->getEventSource(source_name.toStdString());
        if (event_source) {
            data_source_variant = event_source;
            valid_source = true;
        }
    } else if (column_source.startsWith("intervals:")) {
        QString source_name = column_source.mid(10); // Remove "intervals:" prefix
        auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString());
        if (interval_source) {
            data_source_variant = interval_source;
            valid_source = true;
        }
    } else if (column_source.startsWith("points_x:")) {
        QString source_name = column_source.mid(9); // Remove "points_x:" prefix
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".x");
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
        }
    } else if (column_source.startsWith("points_y:")) {
        QString source_name = column_source.mid(9); // Remove "points_y:" prefix
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".y");
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
        }
    } else if (column_source.startsWith("table:")) {
        // TODO: Handle table column references
        // For now, show that this is not yet implemented
        _column_computer_combo->addItem("(Table columns not yet supported)", "");
        return;
    }
    
    if (!valid_source) {
        _column_computer_combo->addItem("(Invalid column data source)", "");
        return;
    }
    
    // Query ComputerRegistry for available computers
    const auto& registry = _table_manager->getComputerRegistry();
    auto available_computers = registry.getAvailableComputers(row_selector_type, data_source_variant);
    
    // Populate the combo box with available computers
    for (const auto& computer_info : available_computers) {
        _column_computer_combo->addItem(QString::fromStdString(computer_info.name), 
                                        QString::fromStdString(computer_info.name));
    }
    
    if (_column_computer_combo->count() == 0) {
        _column_computer_combo->addItem("(No compatible computers available)", "");
    }
    
    qDebug() << "Refreshed computer combo: row selector type" << static_cast<int>(row_selector_type)
             << ", found" << available_computers.size() << "compatible computers";
}

void TableDesignerWidget::loadTableInfo(const QString& table_id) {
    if (table_id.isEmpty() || !_table_manager) {
        clearUI();
        return;
    }
    
    auto info = _table_manager->getTableInfo(table_id);
    if (info.id.isEmpty()) {
        clearUI();
        return;
    }
    
    // Load table information
    _table_name_edit->setText(info.name);
    _table_description_edit->setPlainText(info.description);
    
    // Load row source if available
    if (!info.rowSourceName.isEmpty()) {
        int row_index = _row_data_source_combo->findText(info.rowSourceName);
        if (row_index >= 0) {
            // Block signals to prevent circular dependency when loading table info
            _row_data_source_combo->blockSignals(true);
            _row_data_source_combo->setCurrentIndex(row_index);
            _row_data_source_combo->blockSignals(false);
            
            // Manually update the info label without triggering the signal handler
            updateRowInfoLabel(info.rowSourceName);
        }
    }
    
    // Load columns
    _column_list->clear();
    for (int i = 0; i < info.columns.size(); ++i) {
        const auto& column = info.columns[i];
        auto* item = new QListWidgetItem(column.name, _column_list);
        item->setData(Qt::UserRole, i); // Store column index
    }
    
    // Refresh column data source combo to populate options
    refreshColumnDataSourceCombo();
    
    // Select first column if available
    if (_column_list->count() > 0) {
        _column_list->setCurrentRow(0);
        // loadColumnConfiguration will be called by the selection changed signal
    } else {
        clearColumnConfiguration();
    }
    
    updateBuildStatus(QString("Loaded table: %1").arg(info.name));
}

void TableDesignerWidget::clearUI() {
    _current_table_id.clear();
    
    // Clear table info
    _table_name_edit->clear();
    _table_description_edit->clear();
    
    // Clear row source
    _row_data_source_combo->setCurrentIndex(-1);
    _row_info_label->setText("No row source selected");
    
    // Clear columns
    _column_list->clear();
    clearColumnConfiguration();
    
    // Disable controls
    _delete_table_btn->setEnabled(false);
    _save_info_btn->setEnabled(false);
    _build_table_btn->setEnabled(false);
    
    updateBuildStatus("No table selected");
}

void TableDesignerWidget::updateBuildStatus(const QString& message, bool is_error) {
    _build_status_label->setText(message);
    
    if (is_error) {
        _build_status_label->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    } else {
        _build_status_label->setStyleSheet("QLabel { color: green; }");
    }
}

QStringList TableDesignerWidget::getAvailableDataSources() const {
    QStringList sources;
    
    if (!_table_manager) {
        qDebug() << "getAvailableDataSources: No table manager";
        return sources;
    }
    
    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        qDebug() << "getAvailableDataSources: No data manager extension";
        return sources;
    }
    
    if (!_data_manager) {
        qDebug() << "getAvailableDataSources: No data manager";
        return sources;
    }
    
    // Add TimeFrame keys as potential row sources
    // TimeFrames can define intervals for analysis
    auto timeframe_keys = _data_manager->getTimeFrameKeys();
    qDebug() << "getAvailableDataSources: TimeFrame keys:" << timeframe_keys.size();
    for (const auto& key : timeframe_keys) {
        QString source = QString("TimeFrame: %1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added TimeFrame:" << source;
    }
    
    // Add DigitalEventSeries keys as potential row sources
    // Events can be used to define analysis windows or timestamps
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();
    qDebug() << "getAvailableDataSources: Event keys:" << event_keys.size();
    for (const auto& key : event_keys) {
        QString source = QString("Events: %1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added Events:" << source;
    }
    
    // Add DigitalIntervalSeries keys as potential row sources
    // Intervals directly define analysis windows
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    qDebug() << "getAvailableDataSources: Interval keys:" << interval_keys.size();
    for (const auto& key : interval_keys) {
        QString source = QString("Intervals: %1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added Intervals:" << source;
    }
    
    qDebug() << "getAvailableDataSources: Total sources found:" << sources.size();
    
    return sources;
}

QStringList TableDesignerWidget::getAvailableTableColumns() const {
    QStringList columns;
    
    if (!_table_manager) {
        return columns;
    }
    
    // Get all existing tables and their columns
    auto table_infos = _table_manager->getAllTableInfo();
    for (const auto& info : table_infos) {
        if (info.id != _current_table_id) { // Don't include current table's columns
            for (const QString& column_name : info.columnNames) {
                columns << QString("%1.%2").arg(info.name, column_name);
            }
        }
    }
    
    return columns;
}

void TableDesignerWidget::updateRowInfoLabel(const QString& selected_source) {
    if (selected_source.isEmpty()) {
        _row_info_label->setText("No row source selected");
        return;
    }
    
    // Parse the selected source to get type and name
    QString source_type;
    QString source_name;
    
    if (selected_source.startsWith("TimeFrame: ")) {
        source_type = "TimeFrame";
        source_name = selected_source.mid(11); // Remove "TimeFrame: " prefix
    } else if (selected_source.startsWith("Events: ")) {
        source_type = "Events";
        source_name = selected_source.mid(8); // Remove "Events: " prefix
    } else if (selected_source.startsWith("Intervals: ")) {
        source_type = "Intervals";
        source_name = selected_source.mid(11); // Remove "Intervals: " prefix
    }
    
    // Get additional information about the selected source
    QString info_text = QString("Selected: %1 (%2)").arg(source_name, source_type);
    
    if (!_table_manager) {
        _row_info_label->setText(info_text);
        return;
    }
    
    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        _row_info_label->setText(info_text);
        return;
    }

    auto const source_name_str = source_name.toStdString();
    
    // Add specific information based on source type
    if (source_type == "TimeFrame") {
        auto timeframe = _data_manager->getTime(source_name_str);
        if (timeframe) {
            info_text += QString(" - %1 time points").arg(timeframe->getTotalFrameCount());
        }
    } else if (source_type == "Events") {
        auto event_series = _data_manager->getData<DigitalEventSeries>(source_name_str);
        if (event_series) {
            auto events = event_series->getEventSeries();
            info_text += QString(" - %1 events").arg(events.size());
        }
    } else if (source_type == "Intervals") {
        auto interval_series = _data_manager->getData<DigitalIntervalSeries>(source_name_str);
        if (interval_series) {
            auto intervals = interval_series->getDigitalIntervalSeries();
            info_text += QString(" - %1 intervals").arg(intervals.size());
        }
    }
    
    _row_info_label->setText(info_text);
}

void TableDesignerWidget::loadColumnConfiguration(int column_index) {
    if (_current_table_id.isEmpty() || !_table_manager) {
        clearColumnConfiguration();
        return;
    }
    
    auto column_info = _table_manager->getTableColumn(_current_table_id, column_index);
    if (column_info.name.isEmpty()) {
        clearColumnConfiguration();
        return;
    }
    
    // Block signals to prevent circular updates
    _column_name_edit->blockSignals(true);
    _column_description_edit->blockSignals(true);
    _column_data_source_combo->blockSignals(true);
    _column_computer_combo->blockSignals(true);
    
    // Load the configuration
    _column_name_edit->setText(column_info.name);
    _column_description_edit->setPlainText(column_info.description);
    
    // Set data source combo
    if (!column_info.dataSourceName.isEmpty()) {
        int data_source_index = _column_data_source_combo->findData(column_info.dataSourceName);
        if (data_source_index >= 0) {
            _column_data_source_combo->setCurrentIndex(data_source_index);
        }
    }
    
    // Set computer combo  
    if (!column_info.computerName.isEmpty()) {
        int computer_index = _column_computer_combo->findData(column_info.computerName);
        if (computer_index >= 0) {
            _column_computer_combo->setCurrentIndex(computer_index);
        }
    }
    
    // Restore signals
    _column_name_edit->blockSignals(false);
    _column_description_edit->blockSignals(false);
    _column_data_source_combo->blockSignals(false);
    _column_computer_combo->blockSignals(false);
    
    qDebug() << "Loaded column configuration for:" << column_info.name;
}

void TableDesignerWidget::saveCurrentColumnConfiguration() {
    int current_row = _column_list->currentRow();
    if (current_row < 0 || _current_table_id.isEmpty() || !_table_manager) {
        return;
    }
    
    // Create column info from UI
    ColumnInfo column_info;
    column_info.name = _column_name_edit->text().trimmed();
    column_info.description = _column_description_edit->toPlainText().trimmed();
    column_info.dataSourceName = _column_data_source_combo->currentData().toString();
    column_info.computerName = _column_computer_combo->currentData().toString();
    
    // Save to table manager
    if (_table_manager->updateTableColumn(_current_table_id, current_row, column_info)) {
        qDebug() << "Saved column configuration for:" << column_info.name;
    }
}

void TableDesignerWidget::clearColumnConfiguration() {
    _column_name_edit->clear();
    _column_description_edit->clear();
    _column_data_source_combo->setCurrentIndex(-1);
    _column_computer_combo->setCurrentIndex(-1);
}

std::unique_ptr<IRowSelector> TableDesignerWidget::createRowSelector(const QString& row_source) {
    // Parse the row source to get type and name
    QString source_type;
    QString source_name;
    
    if (row_source.startsWith("TimeFrame: ")) {
        source_type = "TimeFrame";
        source_name = row_source.mid(11); // Remove "TimeFrame: " prefix
    } else if (row_source.startsWith("Events: ")) {
        source_type = "Events";
        source_name = row_source.mid(8); // Remove "Events: " prefix
    } else if (row_source.startsWith("Intervals: ")) {
        source_type = "Intervals";
        source_name = row_source.mid(11); // Remove "Intervals: " prefix
    } else {
        qDebug() << "Unknown row source format:" << row_source;
        return nullptr;
    }
    
    auto const source_name_str = source_name.toStdString();
    
    try {
        if (source_type == "TimeFrame") {
            // Create IntervalSelector using TimeFrame
            auto timeframe = _data_manager->getTime(source_name_str);
            if (!timeframe) {
                qDebug() << "TimeFrame not found:" << source_name;
                return nullptr;
            }
            
            // Create intervals for the entire timeframe
            std::vector<TimeFrameInterval> intervals;
            intervals.emplace_back(TimeFrameIndex(0), TimeFrameIndex(timeframe->getTotalFrameCount() - 1));
            
            return std::make_unique<IntervalSelector>(std::move(intervals), timeframe);
            
        } else if (source_type == "Events") {
            // Create TimestampSelector using DigitalEventSeries
            auto event_series = _data_manager->getData<DigitalEventSeries>(source_name_str);
            if (!event_series) {
                qDebug() << "DigitalEventSeries not found:" << source_name;
                return nullptr;
            }
            
            auto events = event_series->getEventSeries();
            auto timeframe_key = _data_manager->getTimeFrame(source_name_str);
            auto timeframe_obj = _data_manager->getTime(timeframe_key);
            if (!timeframe_obj) {
                qDebug() << "TimeFrame not found for events:" << timeframe_key;
                return nullptr;
            }
            
            // Convert events to TimeFrameIndex
            std::vector<TimeFrameIndex> timestamps;
            for (const auto& event : events) {
                timestamps.push_back(TimeFrameIndex(static_cast<int64_t>(event)));
            }
            
            return std::make_unique<TimestampSelector>(std::move(timestamps), timeframe_obj);
            
        } else if (source_type == "Intervals") {
            // Create IntervalSelector using DigitalIntervalSeries
            auto interval_series = _data_manager->getData<DigitalIntervalSeries>(source_name_str);
            if (!interval_series) {
                qDebug() << "DigitalIntervalSeries not found:" << source_name;
                return nullptr;
            }
            
            auto intervals = interval_series->getDigitalIntervalSeries();
            auto timeframe_key = _data_manager->getTimeFrame(source_name_str);
            auto timeframe_obj = _data_manager->getTime(timeframe_key);
            if (!timeframe_obj) {
                qDebug() << "TimeFrame not found for intervals:" << timeframe_key;
                return nullptr;
            }
            
            // Convert DigitalInterval to TimeFrameInterval
            std::vector<TimeFrameInterval> tf_intervals;
            for (const auto& interval : intervals) {
                tf_intervals.emplace_back(TimeFrameIndex(interval.start), 
                TimeFrameIndex(interval.end));
            }
            
            return std::make_unique<IntervalSelector>(std::move(tf_intervals), timeframe_obj);
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Exception creating row selector:" << e.what();
        return nullptr;
    }
    
    qDebug() << "Unsupported row source type:" << source_type;
    return nullptr;
}

bool TableDesignerWidget::addColumnToBuilder(TableViewBuilder& builder, const ColumnInfo& column_info) {
    if (column_info.dataSourceName.isEmpty() || column_info.computerName.isEmpty()) {
        qDebug() << "Column" << column_info.name << "missing data source or computer configuration";
        return false;
    }
    
    try {
        // Get the data manager extension
        auto data_manager_extension = _table_manager->getDataManagerExtension();
        if (!data_manager_extension) {
            qDebug() << "DataManagerExtension not available";
            return false;
        }
        
        // Create DataSourceVariant from column data source
        DataSourceVariant data_source_variant;
        bool valid_source = false;
        
        const QString& column_source = column_info.dataSourceName;
        
        if (column_source.startsWith("analog:")) {
            QString source_name = column_source.mid(7); // Remove "analog:" prefix
            auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString());
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("events:")) {
            QString source_name = column_source.mid(7); // Remove "events:" prefix
            auto event_source = data_manager_extension->getEventSource(source_name.toStdString());
            if (event_source) {
                data_source_variant = event_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("intervals:")) {
            QString source_name = column_source.mid(10); // Remove "intervals:" prefix
            auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString());
            if (interval_source) {
                data_source_variant = interval_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("points_x:")) {
            QString source_name = column_source.mid(9); // Remove "points_x:" prefix
            auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".x");
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("points_y:")) {
            QString source_name = column_source.mid(9); // Remove "points_y:" prefix
            auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".y");
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        }
        
        if (!valid_source) {
            qDebug() << "Failed to create data source for column:" << column_info.name;
            return false;
        }
        
        // Create the computer from the registry
        const auto& registry = _table_manager->getComputerRegistry();
        auto computer_base = registry.createComputer(column_info.computerName.toStdString(), data_source_variant);
        if (!computer_base) {
            qDebug() << "Failed to create computer" << column_info.computerName << "for column:" << column_info.name;
            return false;
        }
        
        // The computer is wrapped in a ComputerWrapper. We need to work with the actual IColumnComputer
        // For now, assume all computers return double. This could be enhanced later
        // to handle different return types by querying the computer's type
        auto computer_wrapper = dynamic_cast<ComputerWrapper<double>*>(computer_base.get());
        if (!computer_wrapper) {
            qDebug() << "Computer" << column_info.computerName << "is not a double computer wrapper for column:" << column_info.name;
            return false;
        }
        
        // Get the actual computer from the wrapper
        auto typed_computer = computer_wrapper->get();
        if (!typed_computer) {
            qDebug() << "Failed to get typed computer from wrapper for column:" << column_info.name;
            return false;
        }
        
        // Since we can't easily extract the computer from the wrapper without modifying the wrapper class,
        // let's create the computer again directly for the builder
        // This is not ideal but works around the ownership issue
        auto new_computer = registry.createComputer(column_info.computerName.toStdString(), data_source_variant);
        if (!new_computer) {
            qDebug() << "Failed to recreate computer" << column_info.computerName << "for builder";
            return false;
        }
        
        auto new_wrapper = dynamic_cast<ComputerWrapper<double>*>(new_computer.get());
        if (!new_wrapper) {
            qDebug() << "Failed to cast recreated computer to wrapper";
            return false;
        }
        
        // Now we need to work around the ownership issue. 
        // For now, let's use a different approach - create the computer inline
        // based on the computer name and source
        std::unique_ptr<IColumnComputer<double>> final_computer;
        
        // Handle specific computer types
        if (column_info.computerName == "Interval Mean") {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&data_source_variant)) {
                final_computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Mean);
            }
        } else if (column_info.computerName == "Interval Max") {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&data_source_variant)) {
                final_computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Max);
            }
        } else if (column_info.computerName == "Interval Min") {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&data_source_variant)) {
                final_computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::Min);
            }
        } else if (column_info.computerName == "Interval Standard Deviation") {
            if (auto analogSrc = std::get_if<std::shared_ptr<IAnalogSource>>(&data_source_variant)) {
                final_computer = std::make_unique<IntervalReductionComputer>(*analogSrc, ReductionType::StdDev);
            }
        }
        
        if (!final_computer) {
            qDebug() << "Failed to create final computer for" << column_info.computerName;
            return false;
        }
        
        // Add the column to the builder
        builder.addColumn(column_info.name.toStdString(), std::move(final_computer));
        
        qDebug() << "Added column to builder:" << column_info.name;
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "Exception adding column" << column_info.name << "to builder:" << e.what();
        return false;
    }
}
