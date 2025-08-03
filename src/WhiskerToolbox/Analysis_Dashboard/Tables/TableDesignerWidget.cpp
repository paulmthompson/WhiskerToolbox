#include "TableDesignerWidget.hpp"
#include "ui_TableDesignerWidget.h"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/interfaces/IColumnComputer.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "TableManager.hpp"

#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>

TableDesignerWidget::TableDesignerWidget(TableManager * table_manager, std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableDesignerWidget),
      _table_manager(table_manager),
      _data_manager(data_manager) {

    ui->setupUi(this);
    connectSignals();
    refreshTableCombo();
    refreshRowDataSourceCombo();
    clearUI();

    qDebug() << "TableDesignerWidget initialized";
}

TableDesignerWidget::~TableDesignerWidget() {
    delete ui;
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


void TableDesignerWidget::connectSignals() {
    // Table selection signals
    connect(ui->table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onTableSelectionChanged);
    connect(ui->new_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onCreateNewTable);
    connect(ui->delete_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onDeleteTable);

    // Table info signals
    connect(ui->save_info_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onSaveTableInfo);

    // Row source signals
    connect(ui->row_data_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onRowDataSourceChanged);
    connect(ui->capture_range_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TableDesignerWidget::onCaptureRangeChanged);
    connect(ui->interval_beginning_radio, &QRadioButton::toggled,
            this, &TableDesignerWidget::onIntervalSettingChanged);
    connect(ui->interval_end_radio, &QRadioButton::toggled,
            this, &TableDesignerWidget::onIntervalSettingChanged);
    connect(ui->interval_itself_radio, &QRadioButton::toggled,
            this, &TableDesignerWidget::onIntervalSettingChanged);

    // Column design signals
    connect(ui->add_column_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onAddColumn);
    connect(ui->remove_column_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onRemoveColumn);
    connect(ui->move_up_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onMoveColumnUp);
    connect(ui->move_down_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onMoveColumnDown);

    connect(ui->column_data_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onColumnDataSourceChanged);
    connect(ui->column_computer_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TableDesignerWidget::onColumnComputerChanged);

    // Column list selection
    connect(ui->column_list, &QListWidget::currentRowChanged,
            this, &TableDesignerWidget::onColumnSelectionChanged);

    // Column configuration editing
    connect(ui->column_name_edit, &QLineEdit::textChanged,
            this, &TableDesignerWidget::onColumnNameChanged);
    connect(ui->column_description_edit, &QTextEdit::textChanged,
            this, &TableDesignerWidget::onColumnDescriptionChanged);

    // Build signals
    connect(ui->build_table_btn, &QPushButton::clicked,
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
    int current_index = ui->table_combo->currentIndex();
    if (current_index < 0) {
        clearUI();
        return;
    }

    QString table_id = ui->table_combo->itemData(current_index).toString();
    if (table_id.isEmpty()) {
        clearUI();
        return;
    }

    _current_table_id = table_id;
    loadTableInfo(table_id);

    // Enable/disable controls
    ui->delete_table_btn->setEnabled(true);
    ui->save_info_btn->setEnabled(true);
    ui->build_table_btn->setEnabled(true);

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
        for (int i = 0; i < ui->table_combo->count(); ++i) {
            if (ui->table_combo->itemData(i).toString() == table_id) {
                ui->table_combo->setCurrentIndex(i);
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
    QString selected = ui->row_data_source_combo->currentText();
    if (selected.isEmpty()) {
        ui->row_info_label->setText("No row source selected");
        return;
    }

    // Save the row source selection to the current table
    // Only save if we have a current table and we're not loading table info
    if (!_current_table_id.isEmpty() && _table_manager) {
        _table_manager->updateTableRowSource(_current_table_id, selected);
    }

    // Update the info label
    updateRowInfoLabel(selected);

    // Update interval settings visibility
    updateIntervalSettingsVisibility();

    // Refresh column computer options since they depend on row selector type
    refreshColumnComputerCombo();

    qDebug() << "Row data source changed to:" << selected;
}

void TableDesignerWidget::onCaptureRangeChanged() {
    // Update the info label to reflect the new capture range
    QString selected = ui->row_data_source_combo->currentText();
    if (!selected.isEmpty()) {
        updateRowInfoLabel(selected);
    }
}

void TableDesignerWidget::onIntervalSettingChanged() {
    // Update the info label to reflect the new interval setting
    QString selected = ui->row_data_source_combo->currentText();
    if (!selected.isEmpty()) {
        updateRowInfoLabel(selected);
    }
    
    // Update capture range visibility based on interval setting
    updateIntervalSettingsVisibility();
}

void TableDesignerWidget::onAddColumn() {
    if (_current_table_id.isEmpty()) {
        return;
    }

    QString column_name = QString("Column_%1").arg(ui->column_list->count() + 1);

    // Create column info and add to table manager
    ColumnInfo column_info(column_name);
    if (_table_manager->addTableColumn(_current_table_id, column_info)) {
        // Add to UI list
        auto * item = new QListWidgetItem(column_name, ui->column_list);
        item->setData(Qt::UserRole, ui->column_list->count() - 1);// Store column index

        // Don't automatically select the new column to avoid triggering loadColumnConfiguration
        // The user can manually select it if they want to configure it
        ui->column_name_edit->setText(column_name);
        ui->column_name_edit->selectAll();
        ui->column_name_edit->setFocus();

        qDebug() << "Added column:" << column_name;
    } else {
        QMessageBox::warning(this, "Error", "Failed to add column to table");
    }
}

void TableDesignerWidget::onRemoveColumn() {
    auto * current_item = ui->column_list->currentItem();
    if (!current_item || _current_table_id.isEmpty()) {
        return;
    }

    int column_index = ui->column_list->currentRow();
    QString column_name = current_item->text();

    if (_table_manager->removeTableColumn(_current_table_id, column_index)) {
        delete current_item;

        // Clear column configuration if no items left
        if (ui->column_list->count() == 0) {
            clearColumnConfiguration();
        } else {
            // Select next item or previous if at end
            int new_index = column_index;
            if (new_index >= ui->column_list->count()) {
                new_index = ui->column_list->count() - 1;
            }
            if (new_index >= 0) {
                ui->column_list->setCurrentRow(new_index);
            }
        }

        qDebug() << "Removed column:" << column_name;
    } else {
        QMessageBox::warning(this, "Error", "Failed to remove column from table");
    }
}

void TableDesignerWidget::onMoveColumnUp() {
    int current_row = ui->column_list->currentRow();
    if (current_row <= 0 || _current_table_id.isEmpty()) {
        return;
    }

    if (_table_manager->moveTableColumnUp(_current_table_id, current_row)) {
        auto * item = ui->column_list->takeItem(current_row);
        ui->column_list->insertItem(current_row - 1, item);
        ui->column_list->setCurrentRow(current_row - 1);
    }
}

void TableDesignerWidget::onMoveColumnDown() {
    int current_row = ui->column_list->currentRow();
    if (current_row < 0 || current_row >= ui->column_list->count() - 1 || _current_table_id.isEmpty()) {
        return;
    }

    if (_table_manager->moveTableColumnDown(_current_table_id, current_row)) {
        auto * item = ui->column_list->takeItem(current_row);
        ui->column_list->insertItem(current_row + 1, item);
        ui->column_list->setCurrentRow(current_row + 1);
    }
}

void TableDesignerWidget::onColumnDataSourceChanged() {
    qDebug() << "onColumnDataSourceChanged called";
    qDebug() << "Current column data source:" << ui->column_data_source_combo->currentText();
    qDebug() << "Current column data source data:" << ui->column_data_source_combo->currentData().toString();
    
    // Only refresh and save if we're not loading configuration (to prevent infinite loops)
    if (!_loading_column_configuration) {
        refreshColumnComputerCombo();
        saveCurrentColumnConfiguration();
    }
    
    qDebug() << "Column data source changed to:" << ui->column_data_source_combo->currentText();
}

void TableDesignerWidget::onColumnComputerChanged() {
    saveCurrentColumnConfiguration();
    qDebug() << "Column computer changed to:" << ui->column_computer_combo->currentText();
}

void TableDesignerWidget::onColumnSelectionChanged() {
    int current_row = ui->column_list->currentRow();
    if (current_row >= 0 && !_current_table_id.isEmpty()) {
        loadColumnConfiguration(current_row);
    } else {
        clearColumnConfiguration();
    }
}

void TableDesignerWidget::onColumnNameChanged() {
    saveCurrentColumnConfiguration();

    // Update the list item text to match
    auto * current_item = ui->column_list->currentItem();
    if (current_item) {
        current_item->setText(ui->column_name_edit->text());
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

    QString row_source = ui->row_data_source_combo->currentText();
    if (row_source.isEmpty()) {
        updateBuildStatus("No row data source selected", true);
        return;
    }

    if (ui->column_list->count() == 0) {
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
        for (auto const & column_info: table_info.columns) {
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

    } catch (std::exception const & e) {
        updateBuildStatus(QString("Error building table: %1").arg(e.what()), true);
        qDebug() << "Exception during table building:" << e.what();
    }
}

void TableDesignerWidget::onSaveTableInfo() {
    if (_current_table_id.isEmpty()) {
        return;
    }

    QString name = ui->table_name_edit->text().trimmed();
    QString description = ui->table_description_edit->toPlainText().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Table name cannot be empty");
        return;
    }

    if (_table_manager->updateTableInfo(_current_table_id, name, description)) {
        updateBuildStatus("Table information saved");
        // Refresh the combo to show updated name
        refreshTableCombo();
        // Restore selection
        for (int i = 0; i < ui->table_combo->count(); ++i) {
            if (ui->table_combo->itemData(i).toString() == _current_table_id) {
                ui->table_combo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to save table information");
    }
}

void TableDesignerWidget::onTableManagerTableCreated(QString const & table_id) {
    refreshTableCombo();
    qDebug() << "Table created signal received:" << table_id;
}

void TableDesignerWidget::onTableManagerTableRemoved(QString const & table_id) {
    refreshTableCombo();
    if (_current_table_id == table_id) {
        _current_table_id.clear();
        clearUI();
    }
    qDebug() << "Table removed signal received:" << table_id;
}

void TableDesignerWidget::onTableManagerTableInfoUpdated(QString const & table_id) {
    if (_current_table_id == table_id && !_updating_column_configuration) {
        loadTableInfo(table_id);
    }
    qDebug() << "Table info updated signal received:" << table_id;
}

void TableDesignerWidget::refreshTableCombo() {
    ui->table_combo->clear();

    auto table_infos = _table_manager->getAllTableInfo();
    for (auto const & info: table_infos) {
        ui->table_combo->addItem(info.name, info.id);
    }

    if (ui->table_combo->count() == 0) {
        ui->table_combo->addItem("(No tables available)", "");
    }
}

void TableDesignerWidget::refreshRowDataSourceCombo() {
    ui->row_data_source_combo->clear();

    if (!_table_manager) {
        qDebug() << "refreshRowDataSourceCombo: No table manager";
        return;
    }

    auto data_sources = getAvailableDataSources();
    qDebug() << "refreshRowDataSourceCombo: Found" << data_sources.size() << "data sources:" << data_sources;

    for (QString const & source: data_sources) {
        ui->row_data_source_combo->addItem(source);
    }

    if (ui->row_data_source_combo->count() == 0) {
        ui->row_data_source_combo->addItem("(No data sources available)");
        qDebug() << "refreshRowDataSourceCombo: No data sources available";
    }
}

void TableDesignerWidget::refreshColumnDataSourceCombo() {
    ui->column_data_source_combo->clear();

    if (!_table_manager) {
        return;
    }

    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        return;
    }

    // Add AnalogTimeSeries data sources (continuous signals)
    auto analog_keys = _data_manager->getKeys<AnalogTimeSeries>();
    for (auto const & key: analog_keys) {
        ui->column_data_source_combo->addItem(QString("Analog: %1").arg(QString::fromStdString(key)),
                                              QString("analog:%1").arg(QString::fromStdString(key)));
    }

    // Add DigitalEventSeries data sources (discrete events)
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();
    for (auto const & key: event_keys) {
        ui->column_data_source_combo->addItem(QString("Events: %1").arg(QString::fromStdString(key)),
                                              QString("events:%1").arg(QString::fromStdString(key)));
    }

    // Add DigitalIntervalSeries data sources (time intervals)
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    for (auto const & key: interval_keys) {
        ui->column_data_source_combo->addItem(QString("Intervals: %1").arg(QString::fromStdString(key)),
                                              QString("intervals:%1").arg(QString::fromStdString(key)));
    }

    // Add PointData sources with component access (X, Y coordinates)
    auto point_keys = _data_manager->getKeys<PointData>();
    for (auto const & key: point_keys) {
        ui->column_data_source_combo->addItem(QString("Points X: %1").arg(QString::fromStdString(key)),
                                              QString("points_x:%1").arg(QString::fromStdString(key)));
        ui->column_data_source_combo->addItem(QString("Points Y: %1").arg(QString::fromStdString(key)),
                                              QString("points_y:%1").arg(QString::fromStdString(key)));
    }

    // Add existing table columns as potential data sources
    auto table_columns = getAvailableTableColumns();
    for (QString const & column: table_columns) {
        ui->column_data_source_combo->addItem(QString("Table Column: %1").arg(column),
                                              QString("table:%1").arg(column));
    }

    if (ui->column_data_source_combo->count() == 0) {
        ui->column_data_source_combo->addItem("(No data sources available)", "");
    }

    qDebug() << "Column data sources: " << analog_keys.size() << "analog,"
             << event_keys.size() << "events," << interval_keys.size() << "intervals,"
             << point_keys.size() << "point series," << table_columns.size() << "table columns";
}

void TableDesignerWidget::refreshColumnComputerCombo() {
    qDebug() << "refreshColumnComputerCombo called";
    
    // Prevent recursive calls
    if (_refreshing_computer_combo) {
        qDebug() << "refreshColumnComputerCombo: Already refreshing, skipping to prevent recursion";
        return;
    }
    
    _refreshing_computer_combo = true;
    ui->column_computer_combo->clear();

    if (!_table_manager) {
        qDebug() << "No table manager available";
        _refreshing_computer_combo = false;
        return;
    }

    QString row_source = ui->row_data_source_combo->currentText();
    QString column_source = ui->column_data_source_combo->currentData().toString();

    if (row_source.isEmpty()) {
        ui->column_computer_combo->addItem("(Select row source first)", "");
        _refreshing_computer_combo = false;
        return;
    }

    if (column_source.isEmpty()) {
        ui->column_computer_combo->addItem("(Select column data source first)", "");
        _refreshing_computer_combo = false;
        return;
    }

    // Convert row source to RowSelectorType
    RowSelectorType row_selector_type = RowSelectorType::Interval;// Default
    if (row_source.startsWith("TimeFrame: ")) {
        row_selector_type = RowSelectorType::Interval;// TimeFrames define intervals
        qDebug() << "Row selector type: Interval (TimeFrame)";
    } else if (row_source.startsWith("Events: ")) {
        row_selector_type = RowSelectorType::Timestamp;// Events define timestamps
        qDebug() << "Row selector type: Timestamp (Events)";
    } else if (row_source.startsWith("Intervals: ")) {
        row_selector_type = RowSelectorType::Interval;// Intervals are intervals
        qDebug() << "Row selector type: Interval (Intervals)";
    }

    // Create DataSourceVariant from column source
    DataSourceVariant data_source_variant;
    bool valid_source = false;

    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        ui->column_computer_combo->addItem("(DataManager not available)", "");
        _refreshing_computer_combo = false;
        return;
    }

    // Parse column source and create appropriate adapter
    if (column_source.startsWith("analog:")) {
        QString source_name = column_source.mid(7);// Remove "analog:" prefix
        qDebug() << "Creating analog source for:" << source_name;
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString());
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
            qDebug() << "Successfully created analog source";
        } else {
            qDebug() << "Failed to create analog source";
        }
    } else if (column_source.startsWith("events:")) {
        QString source_name = column_source.mid(7);// Remove "events:" prefix
        qDebug() << "Creating event source for:" << source_name;
        auto event_source = data_manager_extension->getEventSource(source_name.toStdString());
        if (event_source) {
            data_source_variant = event_source;
            valid_source = true;
            qDebug() << "Successfully created event source";
        } else {
            qDebug() << "Failed to create event source";
        }
    } else if (column_source.startsWith("intervals:")) {
        QString source_name = column_source.mid(10);// Remove "intervals:" prefix
        qDebug() << "Creating interval source for:" << source_name;
        auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString());
        if (interval_source) {
            data_source_variant = interval_source;
            valid_source = true;
            qDebug() << "Successfully created interval source";
        } else {
            qDebug() << "Failed to create interval source";
        }
    } else if (column_source.startsWith("points_x:")) {
        QString source_name = column_source.mid(9);// Remove "points_x:" prefix
        qDebug() << "Creating points X source for:" << source_name;
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".x");
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
            qDebug() << "Successfully created points X source";
        } else {
            qDebug() << "Failed to create points X source";
        }
    } else if (column_source.startsWith("points_y:")) {
        QString source_name = column_source.mid(9);// Remove "points_y:" prefix
        qDebug() << "Creating points Y source for:" << source_name;
        auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".y");
        if (analog_source) {
            data_source_variant = analog_source;
            valid_source = true;
            qDebug() << "Successfully created points Y source";
        } else {
            qDebug() << "Failed to create points Y source";
        }
    } else if (column_source.startsWith("table:")) {
        // TODO: Handle table column references
        // For now, show that this is not yet implemented
        qDebug() << "Table columns not yet supported";
        ui->column_computer_combo->addItem("(Table columns not yet supported)", "");
        _refreshing_computer_combo = false;
        return;
    } else {
        qDebug() << "Unknown column source format:" << column_source;
    }

    if (!valid_source) {
        ui->column_computer_combo->addItem("(Invalid column data source)", "");
        _refreshing_computer_combo = false;
        return;
    }

    // Query ComputerRegistry for available computers
    auto const & registry = _table_manager->getComputerRegistry();
    auto available_computers = registry.getAvailableComputers(row_selector_type, data_source_variant);

    // Populate the combo box with available computers
    for (auto const & computer_info: available_computers) {
        ui->column_computer_combo->addItem(QString::fromStdString(computer_info.name),
                                           QString::fromStdString(computer_info.name));
    }

    if (ui->column_computer_combo->count() == 0) {
        ui->column_computer_combo->addItem("(No compatible computers available)", "");
    }

    qDebug() << "Refreshed computer combo: row selector type" << static_cast<int>(row_selector_type)
             << ", found" << available_computers.size() << "compatible computers";
    qDebug() << "Column source:" << column_source << ", valid_source:" << valid_source;
    qDebug() << "DataSourceVariant type:" << data_source_variant.index();
    
    if (available_computers.empty()) {
        qDebug() << "No computers available for row selector type" << static_cast<int>(row_selector_type)
                 << "and data source variant index" << data_source_variant.index();
    }
    
    _refreshing_computer_combo = false;
}

void TableDesignerWidget::loadTableInfo(QString const & table_id) {
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
    ui->table_name_edit->setText(info.name);
    ui->table_description_edit->setPlainText(info.description);

    // Load row source if available
    if (!info.rowSourceName.isEmpty()) {
        int row_index = ui->row_data_source_combo->findText(info.rowSourceName);
        if (row_index >= 0) {
            // Block signals to prevent circular dependency when loading table info
            ui->row_data_source_combo->blockSignals(true);
            ui->row_data_source_combo->setCurrentIndex(row_index);
            ui->row_data_source_combo->blockSignals(false);

            // Manually update the info label without triggering the signal handler
            updateRowInfoLabel(info.rowSourceName);
            
            // Update interval settings visibility
            updateIntervalSettingsVisibility();
            
            // Since signals were blocked, refresh column computer combo
            // This will be called again when column configuration is loaded, but ensures
            // the combo is updated based on the row source
            refreshColumnComputerCombo();
        }
    }

    // Load columns
    ui->column_list->clear();
    for (int i = 0; i < info.columns.size(); ++i) {
        auto const & column = info.columns[i];
        auto * item = new QListWidgetItem(column.name, ui->column_list);
        item->setData(Qt::UserRole, i);// Store column index
    }

    // Refresh column data source combo to populate options
    refreshColumnDataSourceCombo();

    // Select first column if available
    if (ui->column_list->count() > 0) {
        ui->column_list->setCurrentRow(0);
        // loadColumnConfiguration will be called by the selection changed signal
    } else {
        clearColumnConfiguration();
    }

    updateBuildStatus(QString("Loaded table: %1").arg(info.name));
}

void TableDesignerWidget::clearUI() {
    _current_table_id.clear();

    // Clear table info
    ui->table_name_edit->clear();
    ui->table_description_edit->clear();

    // Clear row source
    ui->row_data_source_combo->setCurrentIndex(-1);
    ui->row_info_label->setText("No row source selected");
    
    // Reset capture range and interval settings
    setCaptureRange(30000); // Default value
    if (ui->interval_beginning_radio) {
        ui->interval_beginning_radio->setChecked(true);
    }
    if (ui->interval_itself_radio) {
        ui->interval_itself_radio->setChecked(false);
    }
    if (ui->interval_settings_group) {
        ui->interval_settings_group->setVisible(false);
    }

    // Clear columns
    ui->column_list->clear();
    clearColumnConfiguration();

    // Disable controls
    ui->delete_table_btn->setEnabled(false);
    ui->save_info_btn->setEnabled(false);
    ui->build_table_btn->setEnabled(false);

    updateBuildStatus("No table selected");
}

void TableDesignerWidget::updateBuildStatus(QString const & message, bool is_error) {
    ui->build_status_label->setText(message);

    if (is_error) {
        ui->build_status_label->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    } else {
        ui->build_status_label->setStyleSheet("QLabel { color: green; }");
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
    for (auto const & key: timeframe_keys) {
        QString source = QString("TimeFrame: %1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added TimeFrame:" << source;
    }

    // Add DigitalEventSeries keys as potential row sources
    // Events can be used to define analysis windows or timestamps
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();
    qDebug() << "getAvailableDataSources: Event keys:" << event_keys.size();
    for (auto const & key: event_keys) {
        QString source = QString("Events: %1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added Events:" << source;
    }

    // Add DigitalIntervalSeries keys as potential row sources
    // Intervals directly define analysis windows
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    qDebug() << "getAvailableDataSources: Interval keys:" << interval_keys.size();
    for (auto const & key: interval_keys) {
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
    for (auto const & info: table_infos) {
        if (info.id != _current_table_id) {// Don't include current table's columns
            for (QString const & column_name: info.columnNames) {
                columns << QString("%1.%2").arg(info.name, column_name);
            }
        }
    }

    return columns;
}

void TableDesignerWidget::updateRowInfoLabel(QString const & selected_source) {
    if (selected_source.isEmpty()) {
        ui->row_info_label->setText("No row source selected");
        return;
    }

    // Parse the selected source to get type and name
    QString source_type;
    QString source_name;

    if (selected_source.startsWith("TimeFrame: ")) {
        source_type = "TimeFrame";
        source_name = selected_source.mid(11);// Remove "TimeFrame: " prefix
    } else if (selected_source.startsWith("Events: ")) {
        source_type = "Events";
        source_name = selected_source.mid(8);// Remove "Events: " prefix
    } else if (selected_source.startsWith("Intervals: ")) {
        source_type = "Intervals";
        source_name = selected_source.mid(11);// Remove "Intervals: " prefix
    }

    // Get additional information about the selected source
    QString info_text = QString("Selected: %1 (%2)").arg(source_name, source_type);

    if (!_table_manager) {
        ui->row_info_label->setText(info_text);
        return;
    }

    auto data_manager_extension = _table_manager->getDataManagerExtension();
    if (!data_manager_extension) {
        ui->row_info_label->setText(info_text);
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
            
            // Add capture range and interval setting information
            if (isIntervalItselfSelected()) {
                info_text += QString("\nUsing intervals as-is (no capture range)");
            } else {
                int capture_range = getCaptureRange();
                QString interval_point = isIntervalBeginningSelected() ? "beginning" : "end";
                info_text += QString("\nCapture range: ±%1 samples around %2 of intervals").arg(capture_range).arg(interval_point);
            }
        }
    }

    ui->row_info_label->setText(info_text);
}

void TableDesignerWidget::loadColumnConfiguration(int column_index) {
    qDebug() << "loadColumnConfiguration called for column" << column_index;
    
    if (_current_table_id.isEmpty() || !_table_manager) {
        clearColumnConfiguration();
        return;
    }

    auto column_info = _table_manager->getTableColumn(_current_table_id, column_index);
    if (column_info.name.isEmpty()) {
        clearColumnConfiguration();
        return;
    }

    qDebug() << "Loading column config - name:" << column_info.name 
             << "dataSource:" << column_info.dataSourceName 
             << "computer:" << column_info.computerName;

    // Set flag to prevent infinite loops
    _loading_column_configuration = true;

    // Block signals to prevent circular updates
    ui->column_name_edit->blockSignals(true);
    ui->column_description_edit->blockSignals(true);
    ui->column_data_source_combo->blockSignals(true);
    ui->column_computer_combo->blockSignals(true);

    // Load the configuration
    ui->column_name_edit->setText(column_info.name);
    ui->column_description_edit->setPlainText(column_info.description);

    // Set data source combo
    if (!column_info.dataSourceName.isEmpty()) {
        int data_source_index = ui->column_data_source_combo->findData(column_info.dataSourceName);
        if (data_source_index >= 0) {
            ui->column_data_source_combo->setCurrentIndex(data_source_index);
            qDebug() << "Set data source combo to index" << data_source_index;
        } else {
            qDebug() << "Could not find data source" << column_info.dataSourceName << "in combo box";
        }
    } else {
        qDebug() << "No data source name in saved configuration";
    }

    // Note: Computer combo will be set after refreshing based on the data source

    // Restore signals
    ui->column_name_edit->blockSignals(false);
    ui->column_description_edit->blockSignals(false);
    ui->column_data_source_combo->blockSignals(false);
    ui->column_computer_combo->blockSignals(false);

    // Since signals were blocked, we need to manually refresh the computer combo
    // to populate it based on the loaded data source
    refreshColumnComputerCombo();

    // Now set the computer combo to the saved value after the refresh
    if (!column_info.computerName.isEmpty()) {
        int computer_index = ui->column_computer_combo->findData(column_info.computerName);
        if (computer_index >= 0) {
            ui->column_computer_combo->setCurrentIndex(computer_index);
            qDebug() << "Set computer combo to index" << computer_index << "after refresh";
        } else {
            qDebug() << "Could not find computer" << column_info.computerName << "in refreshed combo box";
        }
    }

    // Reset flag
    _loading_column_configuration = false;

    qDebug() << "Loaded column configuration for:" << column_info.name;
}

void TableDesignerWidget::saveCurrentColumnConfiguration() {
    int current_row = ui->column_list->currentRow();
    if (current_row < 0 || _current_table_id.isEmpty() || !_table_manager) {
        return;
    }

    // Set flag to prevent reload during update
    _updating_column_configuration = true;

    // Create column info from UI
    ColumnInfo column_info;
    column_info.name = ui->column_name_edit->text().trimmed();
    column_info.description = ui->column_description_edit->toPlainText().trimmed();
    column_info.dataSourceName = ui->column_data_source_combo->currentData().toString();
    column_info.computerName = ui->column_computer_combo->currentData().toString();

    // Save to table manager
    if (_table_manager->updateTableColumn(_current_table_id, current_row, column_info)) {
        qDebug() << "Saved column configuration for:" << column_info.name;
    }

    // Reset flag
    _updating_column_configuration = false;
}

void TableDesignerWidget::clearColumnConfiguration() {
    ui->column_name_edit->clear();
    ui->column_description_edit->clear();
    ui->column_data_source_combo->setCurrentIndex(-1);
    ui->column_computer_combo->setCurrentIndex(-1);
}

std::unique_ptr<IRowSelector> TableDesignerWidget::createRowSelector(QString const & row_source) {
    // Parse the row source to get type and name
    QString source_type;
    QString source_name;

    if (row_source.startsWith("TimeFrame: ")) {
        source_type = "TimeFrame";
        source_name = row_source.mid(11);// Remove "TimeFrame: " prefix
    } else if (row_source.startsWith("Events: ")) {
        source_type = "Events";
        source_name = row_source.mid(8);// Remove "Events: " prefix
    } else if (row_source.startsWith("Intervals: ")) {
        source_type = "Intervals";
        source_name = row_source.mid(11);// Remove "Intervals: " prefix
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
            for (auto const & event: events) {
                timestamps.push_back(TimeFrameIndex(static_cast<int64_t>(event)));
            }

            return std::make_unique<TimestampSelector>(std::move(timestamps), timeframe_obj);

        } else if (source_type == "Intervals") {
            // Create IntervalSelector using DigitalIntervalSeries with capture range
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

            // Get capture range and interval setting
            int capture_range = getCaptureRange();
            bool use_beginning = isIntervalBeginningSelected();
            bool use_interval_itself = isIntervalItselfSelected();

            // Create intervals based on the selected option
            std::vector<TimeFrameInterval> tf_intervals;
            for (auto const & interval: intervals) {
                if (use_interval_itself) {
                    // Use the interval as-is
                    tf_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
                } else {
                    // Determine the reference point (beginning or end of interval)
                    int64_t reference_point;
                    if (use_beginning) {
                        reference_point = interval.start;
                    } else {
                        reference_point = interval.end;
                    }

                    // Create a new interval around the reference point
                    int64_t start_point = reference_point - capture_range;
                    int64_t end_point = reference_point + capture_range;

                    // Ensure bounds are within the timeframe
                    start_point = std::max(start_point, 0L);
                    end_point = std::min(end_point, static_cast<int64_t>(timeframe_obj->getTotalFrameCount() - 1));

                    tf_intervals.emplace_back(TimeFrameIndex(start_point), TimeFrameIndex(end_point));
                }
            }

            return std::make_unique<IntervalSelector>(std::move(tf_intervals), timeframe_obj);
        }

    } catch (std::exception const & e) {
        qDebug() << "Exception creating row selector:" << e.what();
        return nullptr;
    }

    qDebug() << "Unsupported row source type:" << source_type;
    return nullptr;
}

bool TableDesignerWidget::addColumnToBuilder(TableViewBuilder & builder, ColumnInfo const & column_info) {
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

        QString const & column_source = column_info.dataSourceName;

        if (column_source.startsWith("analog:")) {
            QString source_name = column_source.mid(7);// Remove "analog:" prefix
            auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString());
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("events:")) {
            QString source_name = column_source.mid(7);// Remove "events:" prefix
            auto event_source = data_manager_extension->getEventSource(source_name.toStdString());
            if (event_source) {
                data_source_variant = event_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("intervals:")) {
            QString source_name = column_source.mid(10);// Remove "intervals:" prefix
            auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString());
            if (interval_source) {
                data_source_variant = interval_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("points_x:")) {
            QString source_name = column_source.mid(9);// Remove "points_x:" prefix
            auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString() + ".x");
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.startsWith("points_y:")) {
            QString source_name = column_source.mid(9);// Remove "points_y:" prefix
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
        auto const & registry = _table_manager->getComputerRegistry();
        auto computer_base = registry.createComputer(column_info.computerName.toStdString(), data_source_variant);
        if (!computer_base) {
            qDebug() << "Failed to create computer" << column_info.computerName << "for column:" << column_info.name;
            return false;
        }

        // The computer is wrapped in a ComputerWrapper. We need to work with the actual IColumnComputer
        // For now, assume all computers return double. This could be enhanced later
        // to handle different return types by querying the computer's type
        auto computer_wrapper = dynamic_cast<ComputerWrapper<double> *>(computer_base.get());
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

        auto new_wrapper = dynamic_cast<ComputerWrapper<double> *>(new_computer.get());
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

    } catch (std::exception const & e) {
        qDebug() << "Exception adding column" << column_info.name << "to builder:" << e.what();
        return false;
    }
}

void TableDesignerWidget::updateIntervalSettingsVisibility() {
    if (!ui->interval_settings_group) {
        return;
    }

    QString selected_key = ui->row_data_source_combo->currentText();
    if (selected_key.isEmpty()) {
        ui->interval_settings_group->setVisible(false);
        if (ui->capture_range_spinbox) {
            ui->capture_range_spinbox->setEnabled(false);
        }
        return;
    }

    if (!_data_manager) {
        ui->interval_settings_group->setVisible(false);
        if (ui->capture_range_spinbox) {
            ui->capture_range_spinbox->setEnabled(false);
        }
        return;
    }

    // Check if the selected source is an interval series
    if (selected_key.startsWith("Intervals: ")) {
        ui->interval_settings_group->setVisible(true);
        
        // Enable/disable capture range based on interval setting
        if (ui->capture_range_spinbox) {
            bool use_interval_itself = isIntervalItselfSelected();
            ui->capture_range_spinbox->setEnabled(!use_interval_itself);
        }
    } else {
        ui->interval_settings_group->setVisible(false);
        if (ui->capture_range_spinbox) {
            ui->capture_range_spinbox->setEnabled(false);
        }
    }
}

int TableDesignerWidget::getCaptureRange() const {
    if (ui->capture_range_spinbox) {
        return ui->capture_range_spinbox->value();
    }
    return 30000;// Default value
}

void TableDesignerWidget::setCaptureRange(int value) {
    if (ui->capture_range_spinbox) {
        ui->capture_range_spinbox->blockSignals(true);
        ui->capture_range_spinbox->setValue(value);
        ui->capture_range_spinbox->blockSignals(false);
    }
}

bool TableDesignerWidget::isIntervalBeginningSelected() const {
    if (ui->interval_beginning_radio) {
        return ui->interval_beginning_radio->isChecked();
    }
    return true;// Default to beginning
}

bool TableDesignerWidget::isIntervalItselfSelected() const {
    if (ui->interval_itself_radio) {
        return ui->interval_itself_radio->isChecked();
    }
    return false;// Default to not selected
}
