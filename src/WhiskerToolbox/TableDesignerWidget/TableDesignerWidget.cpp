#include "TableDesignerWidget.hpp"
#include "ui_TableDesignerWidget.h"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/TableEvents.hpp"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/interfaces/IColumnComputer.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/transforms/PCATransform.hpp"

#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include "PreviewTableModel.hpp"

#include <algorithm>
#include <limits>
#include <typeindex>
#include <vector>
#include <tuple>
#include <fstream>
#include <iomanip>

TableDesignerWidget::TableDesignerWidget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableDesignerWidget),
      _data_manager(std::move(data_manager)) {

    ui->setupUi(this);
    
    // Set up parameter UI container
    _parameter_widget = ui->parameter_container_widget;
    _parameter_layout = new QVBoxLayout(_parameter_widget);
    _parameter_layout->setContentsMargins(0, 0, 0, 0);
    _parameter_layout->setSpacing(4);
    
    // Initialize preview model and debounce timer
    _preview_model = new PreviewTableModel(this);
    if (ui->preview_table) {
        ui->preview_table->setModel(_preview_model);
    }
    _preview_debounce_timer = new QTimer(this);
    _preview_debounce_timer->setSingleShot(true);
    _preview_debounce_timer->setInterval(150);
    connect(_preview_debounce_timer, &QTimer::timeout, this, &TableDesignerWidget::rebuildPreviewNow);

    connectSignals();
    refreshTableCombo();
    refreshRowDataSourceCombo();
    clearUI();

    // Add observer to automatically refresh dropdowns when DataManager changes
    if (_data_manager) {
        _data_manager->addObserver([this]() {
            refreshAllDataSources();
        });
    }

    qDebug() << "TableDesignerWidget initialized (legacy path - to be removed)";
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
    if (ui->apply_transform_btn) {
        connect(ui->apply_transform_btn, &QPushButton::clicked,
                this, &TableDesignerWidget::onApplyTransform);
    }
    if (ui->export_csv_btn) {
        connect(ui->export_csv_btn, &QPushButton::clicked,
                this, &TableDesignerWidget::onExportCsv);
    }

    // Preview controls
    if (ui->preview_slider) {
        connect(ui->preview_slider, &QSlider::valueChanged, this, [this](int) { triggerPreviewDebounced(); });
    }
    if (ui->preview_window_size_spinbox) {
        connect(ui->preview_window_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            updatePreviewSliderRange();
            triggerPreviewDebounced();
        });
    }
    if (ui->preview_auto_refresh_checkbox) {
        connect(ui->preview_auto_refresh_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
            if (ui->preview_refresh_btn) ui->preview_refresh_btn->setEnabled(!checked);
            if (checked) triggerPreviewDebounced();
        });
    }
    if (ui->preview_refresh_btn) {
        connect(ui->preview_refresh_btn, &QPushButton::clicked, this, &TableDesignerWidget::rebuildPreviewNow);
    }

    // Subscribe to DataManager table observer
    if (_data_manager) {
        auto token = _data_manager->addTableObserver([this](TableEvent const & ev) {
            switch (ev.type) {
                case TableEventType::Created:
                    this->onTableManagerTableCreated(QString::fromStdString(ev.tableId));
                    break;
                case TableEventType::Removed:
                    this->onTableManagerTableRemoved(QString::fromStdString(ev.tableId));
                    break;
                case TableEventType::InfoUpdated:
                    this->onTableManagerTableInfoUpdated(QString::fromStdString(ev.tableId));
                    break;
                case TableEventType::DataChanged:
                    // No direct UI change needed here for designer list
                    break;
            }
        });
        (void)token; // Optionally store and remove on dtor
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

    auto * registry = _data_manager->getTableRegistry();
    if (!registry) { return; }
    auto table_id = registry->generateUniqueTableId("Table");

    if (registry->createTable(table_id, name.toStdString())) {
        // The combo will be refreshed by the signal handler
        // Set the new table as selected
        for (int i = 0; i < ui->table_combo->count(); ++i) {
            if (ui->table_combo->itemData(i).toString().toStdString() == table_id) {
                ui->table_combo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, "Error", "Failed to create table with ID: " + QString::fromStdString(table_id));
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
        auto * registry = _data_manager->getTableRegistry();
        if (registry && registry->removeTable(_current_table_id.toStdString())) {
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
    if (!_current_table_id.isEmpty() && _data_manager) {
        if (auto* reg = _data_manager->getTableRegistry()) {
            reg->updateTableRowSource(_current_table_id.toStdString(), selected.toStdString());
        }
    }

    // Update the info label
    updateRowInfoLabel(selected);

    // Update interval settings visibility
    updateIntervalSettingsVisibility();

    // Refresh column computer options since they depend on row selector type
    refreshColumnComputerCombo();

    qDebug() << "Row data source changed to:" << selected;
    updatePreviewSliderRange();
    triggerPreviewDebounced();
}

void TableDesignerWidget::onCaptureRangeChanged() {
    // Update the info label to reflect the new capture range
    QString selected = ui->row_data_source_combo->currentText();
    if (!selected.isEmpty()) {
        updateRowInfoLabel(selected);
    }
    triggerPreviewDebounced();
}

void TableDesignerWidget::onIntervalSettingChanged() {
    // Update the info label to reflect the new interval setting
    QString selected = ui->row_data_source_combo->currentText();
    if (!selected.isEmpty()) {
        updateRowInfoLabel(selected);
    }
    
    // Update capture range visibility based on interval setting
    updateIntervalSettingsVisibility();
    triggerPreviewDebounced();
}

void TableDesignerWidget::onAddColumn() {
    qDebug() << "=== onAddColumn() called ===";
    qDebug() << "Current table ID:" << _current_table_id;
    qDebug() << "Current column count:" << ui->column_list->count();
    
    if (_current_table_id.isEmpty()) {
        qDebug() << "No current table ID, returning";
        return;
    }

    // Set flag to prevent recursive updates
    _updating_column_configuration = true;

    QString column_name = QString("Column_%1").arg(ui->column_list->count() + 1);
    qDebug() << "Generated column name:" << column_name;

    // Create column info and add to table manager
    ColumnInfo column_info(column_name.toStdString());
    qDebug() << "Calling TableRegistry.addTableColumn()";
    if (auto* reg = _data_manager->getTableRegistry(); reg && reg->addTableColumn(_current_table_id.toStdString(), column_info)) {
        qDebug() << "Successfully added column to table manager";
        
        // Add to UI list with correct index
        int new_index = ui->column_list->count(); // Get index BEFORE adding
        qDebug() << "UI column list count BEFORE adding item:" << ui->column_list->count();
        qDebug() << "Calculated new_index:" << new_index;
        
        auto * item = new QListWidgetItem(column_name, ui->column_list);
        item->setData(Qt::UserRole, new_index); // Store correct column index

        qDebug() << "UI column list count AFTER adding item:" << ui->column_list->count();

        // Select the newly added column so subsequent UI edits map to this row
        // This ensures data source/computer selections are saved correctly
        ui->column_list->setCurrentRow(new_index);
        ui->column_name_edit->setText(column_name);
        ui->column_name_edit->selectAll();
        ui->column_name_edit->setFocus();

        qDebug() << "Added column:" << column_name << "at index:" << new_index;
        qDebug() << "UI column list count is now:" << ui->column_list->count();
    } else {
        qDebug() << "Failed to add column to table manager";
        QMessageBox::warning(this, "Error", "Failed to add column to table");
    }
    
    // Reset flag
    _updating_column_configuration = false;
    qDebug() << "=== onAddColumn() finished ===";
}

void TableDesignerWidget::onRemoveColumn() {
    auto * current_item = ui->column_list->currentItem();
    if (!current_item || _current_table_id.isEmpty()) {
        return;
    }

    int column_index = ui->column_list->currentRow();
    QString column_name = current_item->text();

    if (auto* reg = _data_manager->getTableRegistry(); reg && reg->removeTableColumn(_current_table_id.toStdString(), column_index)) {
        delete current_item;
        
        // Update UserRole indices for all remaining items after the removed one
        updateColumnIndices();

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
    triggerPreviewDebounced();
}

void TableDesignerWidget::onColumnComputerChanged() {
    saveCurrentColumnConfiguration();
    
    // Clear existing parameter UI
    clearParameterUI();
    
    // Get and display type information for the selected computer
    auto computer_name = ui->column_computer_combo->currentData().toString();
    if (!computer_name.isEmpty() && _data_manager) {
        auto* reg = _data_manager->getTableRegistry();
        if (!reg) { return; }
        auto [type_name, is_vector, element_type] = reg->getComputerTypeInfo(computer_name.toStdString());
        
        QString type_info = QString("Returns: %1").arg(QString::fromStdString(type_name));
        if (is_vector && element_type != "unknown") {
            type_info += QString(" (elements: %1)").arg(QString::fromStdString(element_type));
        }
        
        qDebug() << "Column computer changed to:" << ui->column_computer_combo->currentText() 
                 << "-" << type_info;
        
        // Setup parameter UI for the selected computer
        setupParameterUI(computer_name);
        
        // If there's a type info label in the UI, update it
        // For now, we'll just log it - you could add a label to the UI to display this
        updateBuildStatus(type_info);
    } else {
        qDebug() << "Column computer changed to:" << ui->column_computer_combo->currentText();
    }
    triggerPreviewDebounced();
}

void TableDesignerWidget::onColumnSelectionChanged() {
    int current_row = ui->column_list->currentRow();
    if (current_row >= 0 && !_current_table_id.isEmpty()) {
        loadColumnConfiguration(current_row);
    } else {
        clearColumnConfiguration();
    }
    triggerPreviewDebounced();
}

void TableDesignerWidget::onColumnNameChanged() {
    saveCurrentColumnConfiguration();

    // Update the list item text to match
    auto * current_item = ui->column_list->currentItem();
    if (current_item) {
        current_item->setText(ui->column_name_edit->text());
    }
    triggerPreviewDebounced();
}

void TableDesignerWidget::onColumnDescriptionChanged() {
    saveCurrentColumnConfiguration();
    triggerPreviewDebounced();
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
        auto* reg = _data_manager->getTableRegistry();
        if (!reg) { updateBuildStatus("Registry unavailable", true); return; }
        auto table_info = reg->getTableInfo(_current_table_id.toStdString());
        if (table_info.columns.empty()) {
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
        auto* reg2 = _data_manager->getTableRegistry();
        auto data_manager_extension = reg2 ? reg2->getDataManagerExtension() : nullptr;
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
                updateBuildStatus(QString("Failed to create column: %1").arg(QString::fromStdString(column_info.name)), true);
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
        if (auto* reg3 = _data_manager->getTableRegistry(); reg3 && reg3->storeBuiltTable(_current_table_id.toStdString(), std::move(table_view))) {
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

void TableDesignerWidget::onApplyTransform() {
    if (_current_table_id.isEmpty() || !_data_manager) {
        updateBuildStatus("No base table selected", true);
        return;
    }

    // Fetch the built base table
    auto * reg = _data_manager->getTableRegistry();
    if (!reg) { updateBuildStatus("Registry unavailable", true); return; }
    auto base_view = reg->getBuiltTable(_current_table_id.toStdString());
    if (!base_view) {
        updateBuildStatus("Build the base table first", true);
        return;
    }

    // Currently only PCA option is exposed
    QString transform = ui->transform_type_combo ? ui->transform_type_combo->currentText() : QString();
    if (transform != "PCA") {
        updateBuildStatus("Unsupported transform", true);
        return;
    }

    // Configure PCA
    PCAConfig cfg;
    cfg.center = ui->transform_center_checkbox && ui->transform_center_checkbox->isChecked();
    cfg.standardize = ui->transform_standardize_checkbox && ui->transform_standardize_checkbox->isChecked();
    if (ui->transform_include_edit) {
        for (auto const & s : parseCommaSeparatedList(ui->transform_include_edit->text())) cfg.include.push_back(s);
    }
    if (ui->transform_exclude_edit) {
        for (auto const & s : parseCommaSeparatedList(ui->transform_exclude_edit->text())) cfg.exclude.push_back(s);
    }

    try {
        PCATransform pca(cfg);
        TableView derived = pca.apply(*base_view);

        // Determine output id/name
        QString out_name = ui->transform_output_name_edit ? ui->transform_output_name_edit->text().trimmed()
                                                          : QString();
        if (out_name.isEmpty()) out_name = QString("%1 (PCA)").arg(ui->table_name_edit->text().trimmed());

        std::string out_id = reg->generateUniqueTableId((_current_table_id + "_pca").toStdString());
        if (!reg->createTable(out_id, out_name.toStdString())) {
            reg->updateTableInfo(out_id, out_name.toStdString(), "");
        }
        if (reg->storeBuiltTable(out_id, std::move(derived))) {
            updateBuildStatus(QString("Created transformed table: %1").arg(out_name));
            refreshTableCombo();
        } else {
            updateBuildStatus("Failed to store transformed table", true);
        }
    } catch (std::exception const & e) {
        updateBuildStatus(QString("Transform failed: %1").arg(e.what()), true);
    }
}

std::vector<std::string> TableDesignerWidget::parseCommaSeparatedList(QString const & text) const {
    std::vector<std::string> out;
    for (QString s : text.split(",", Qt::SkipEmptyParts)) {
        s = s.trimmed();
        if (!s.isEmpty()) out.push_back(s.toStdString());
    }
    return out;
}

void TableDesignerWidget::onExportCsv() {
    if (_current_table_id.isEmpty() || !_data_manager) {
        updateBuildStatus("No table selected", true);
        return;
    }

    auto * reg = _data_manager->getTableRegistry();
    if (!reg) { updateBuildStatus("Registry unavailable", true); return; }
    auto view = reg->getBuiltTable(_current_table_id.toStdString());
    if (!view) { updateBuildStatus("Build the table first", true); return; }

    QString filename = promptSaveCsvFilename();
    if (filename.isEmpty()) return;
    if (!filename.endsWith(".csv", Qt::CaseInsensitive)) filename += ".csv";

    // CSV options
    QString delimiter = ui->export_delimiter_combo ? ui->export_delimiter_combo->currentText() : "Comma";
    QString lineEnding = ui->export_line_ending_combo ? ui->export_line_ending_combo->currentText() : "LF (\\n)";
    int precision = ui->export_precision_spinbox ? ui->export_precision_spinbox->value() : 3;
    bool includeHeader = ui->export_header_checkbox && ui->export_header_checkbox->isChecked();

    std::string delim = ",";
    if (delimiter == "Space") delim = " ";
    else if (delimiter == "Tab") delim = "\t";
    std::string eol = "\n";
    if (lineEnding.startsWith("CRLF")) eol = "\r\n";

    try {
        std::ofstream file(filename.toStdString());
        if (!file.is_open()) {
            updateBuildStatus("Could not open file for writing", true);
            return;
        }
        file << std::fixed << std::setprecision(precision);

        auto names = view->getColumnNames();
        if (includeHeader) {
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) file << delim;
                file << names[i];
            }
            file << eol;
        }
        size_t rows = view->getRowCount();
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < names.size(); ++c) {
                if (c > 0) file << delim;
                try {
                    auto const & vals = view->getColumnValues<double>(names[c].c_str());
                    if (r < vals.size()) file << vals[r]; else file << "NaN";
                } catch (...) {
                    file << "NaN";
                }
            }
            file << eol;
        }
        file.close();
        updateBuildStatus(QString("Exported CSV: %1").arg(filename));
    } catch (std::exception const & e) {
        updateBuildStatus(QString("Export failed: %1").arg(e.what()), true);
    }
}

QString TableDesignerWidget::promptSaveCsvFilename() const {
    return QFileDialog::getSaveFileName(const_cast<TableDesignerWidget*>(this), "Export Table to CSV", QString(), "CSV Files (*.csv)");
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

    if (auto* reg = _data_manager->getTableRegistry(); reg && reg->updateTableInfo(_current_table_id.toStdString(), name.toStdString(), description.toStdString())) {
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

    auto* reg = _data_manager->getTableRegistry();
    auto table_infos = reg ? reg->getAllTableInfo() : std::vector<TableInfo>{};
    for (auto const & info: table_infos) {
        ui->table_combo->addItem(QString::fromStdString(info.name), QString::fromStdString(info.id));
    }

    if (ui->table_combo->count() == 0) {
        ui->table_combo->addItem("(No tables available)", "");
    }
}

void TableDesignerWidget::refreshRowDataSourceCombo() {
    ui->row_data_source_combo->clear();

    if (!_data_manager) {
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

    if (!_data_manager) {
        return;
    }

    auto* reg = _data_manager->getTableRegistry();
    auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
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

    // Add LineData sources
    auto line_keys = _data_manager->getKeys<LineData>();
    for (auto const & key: line_keys) {
        ui->column_data_source_combo->addItem(QString("Lines: %1").arg(QString::fromStdString(key)),
                                              QString("lines:%1").arg(QString::fromStdString(key)));
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

    if (!_data_manager) {
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
    RowSelectorType row_selector_type = RowSelectorType::IntervalBased;// Default
    if (row_source.startsWith("TimeFrame: ")) {
        row_selector_type = RowSelectorType::Timestamp;// TimeFrames define timestamps
        qDebug() << "Row selector type: Timestamp (TimeFrame)";
    } else if (row_source.startsWith("Events: ")) {
        row_selector_type = RowSelectorType::Timestamp;// Events define timestamps
        qDebug() << "Row selector type: Timestamp (Events)";
    } else if (row_source.startsWith("Intervals: ")) {
        row_selector_type = RowSelectorType::IntervalBased;// Intervals are intervals
        qDebug() << "Row selector type: Interval (Intervals)";
    }

    // Create DataSourceVariant from column source
    DataSourceVariant data_source_variant;
    bool valid_source = false;

    auto* reg = _data_manager->getTableRegistry();
    auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
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
    } else if (column_source.startsWith("lines:")) {
        QString source_name = column_source.mid(6);// Remove "lines:" prefix
        qDebug() << "Creating line source for:" << source_name;
        auto line_source = data_manager_extension->getLineSource(source_name.toStdString());
        if (line_source) {
            data_source_variant = line_source;
            valid_source = true;
            qDebug() << "Successfully created line source";
        } else {
            qDebug() << "Failed to create line source";
        }
    } else {
        qDebug() << "Unknown column source format:" << column_source;
    }

    if (!valid_source) {
        ui->column_computer_combo->addItem("(Invalid column data source)", "");
        _refreshing_computer_combo = false;
        return;
    }

    // Query ComputerRegistry for available computers
    auto const & registry = _data_manager->getTableRegistry()->getComputerRegistry();
    auto available_computers = registry.getAvailableComputers(row_selector_type, data_source_variant);

    // Populate the combo box with available computers, including type information
    for (auto const & computer_info: available_computers) {
        QString display_name = QString::fromStdString(computer_info.name);
        
        // Add type information to the display name
        if (!computer_info.outputTypeName.empty()) {
            display_name += QString(" -> %1").arg(QString::fromStdString(computer_info.outputTypeName));
        }
        
        // Add vector element information if applicable
        if (computer_info.isVectorType && !computer_info.elementTypeName.empty()) {
            display_name += QString(" [%1]").arg(QString::fromStdString(computer_info.elementTypeName));
        }
        
        ui->column_computer_combo->addItem(display_name, QString::fromStdString(computer_info.name));
        
        qDebug() << "Added computer:" << computer_info.name.c_str() 
                 << "returning" << computer_info.outputTypeName.c_str();
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
    if (table_id.isEmpty() || !_data_manager) {
        clearUI();
        return;
    }

    auto* reg = _data_manager->getTableRegistry();
    auto info = reg ? reg->getTableInfo(table_id.toStdString()) : TableInfo{};
    if (info.id.empty()) {
        clearUI();
        return;
    }

    // Load table information
    ui->table_name_edit->setText(QString::fromStdString(info.name));
    ui->table_description_edit->setPlainText(QString::fromStdString(info.description));

    // Load row source if available
    if (!info.rowSourceName.empty()) {
        int row_index = ui->row_data_source_combo->findText(QString::fromStdString(info.rowSourceName));
        if (row_index >= 0) {
            // Block signals to prevent circular dependency when loading table info
            ui->row_data_source_combo->blockSignals(true);
            ui->row_data_source_combo->setCurrentIndex(row_index);
            ui->row_data_source_combo->blockSignals(false);

            // Manually update the info label without triggering the signal handler
            updateRowInfoLabel(QString::fromStdString(info.rowSourceName));
            
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
        auto * item = new QListWidgetItem(QString::fromStdString(column.name), ui->column_list);
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

    updateBuildStatus(QString("Loaded table: %1").arg(QString::fromStdString(info.name)));
    updatePreviewSliderRange();
    triggerPreviewDebounced();
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
    if (_preview_model) _preview_model->clearPreview();
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

    if (!_data_manager) {
        qDebug() << "getAvailableDataSources: No table manager";
        return sources;
    }

    auto* reg = _data_manager->getTableRegistry();
    auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
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
        QString source = QString("TimeFrame: %1").arg(QString::fromStdString(key.str()));
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

    if (!_data_manager) {
        return columns;
    }

    // Get all existing tables and their columns
    auto* reg2 = _data_manager->getTableRegistry();
    auto table_infos = reg2 ? reg2->getAllTableInfo() : std::vector<TableInfo>{};
    for (auto const & info: table_infos) {
        if (QString::fromStdString(info.id) != _current_table_id) {// Don't include current table's columns
            for (std::string const & column_name: info.columnNames) {
                columns << QString("%1.%2").arg(QString::fromStdString(info.name), QString::fromStdString(column_name));
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

    if (!_data_manager) {
        ui->row_info_label->setText(info_text);
        return;
    }

    auto* reg3 = _data_manager->getTableRegistry();
    auto data_manager_extension = reg3 ? reg3->getDataManagerExtension() : nullptr;
    if (!data_manager_extension) {
        ui->row_info_label->setText(info_text);
        return;
    }

    auto const source_name_str = source_name.toStdString();

    // Add specific information based on source type
    if (source_type == "TimeFrame") {
        auto timeframe = _data_manager->getTime(TimeKey(source_name_str));
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
    
    if (_current_table_id.isEmpty() || !_data_manager) {
        clearColumnConfiguration();
        return;
    }

    auto* reg = _data_manager->getTableRegistry();
    auto column_info = reg ? reg->getTableColumn(_current_table_id.toStdString(), column_index) : ColumnInfo{};
    if (column_info.name.empty()) {
        clearColumnConfiguration();
        return;
    }

    qDebug() << "Loading column config - name:" << QString::fromStdString(column_info.name)
             << "dataSource:" << QString::fromStdString(column_info.dataSourceName)
             << "computer:" << QString::fromStdString(column_info.computerName);

    // Set flag to prevent infinite loops
    _loading_column_configuration = true;

    // Block signals to prevent circular updates
    ui->column_name_edit->blockSignals(true);
    ui->column_description_edit->blockSignals(true);
    ui->column_data_source_combo->blockSignals(true);
    ui->column_computer_combo->blockSignals(true);

    // Load the configuration
    ui->column_name_edit->setText(QString::fromStdString(column_info.name));
    ui->column_description_edit->setPlainText(QString::fromStdString(column_info.description));

    // Set data source combo
    if (!column_info.dataSourceName.empty()) {
        int data_source_index = ui->column_data_source_combo->findData(QString::fromStdString(column_info.dataSourceName));
        if (data_source_index >= 0) {
            ui->column_data_source_combo->setCurrentIndex(data_source_index);
            qDebug() << "Set data source combo to index" << data_source_index;
        } else {
            qDebug() << "Could not find data source" << QString::fromStdString(column_info.dataSourceName) << "in combo box";
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
    if (!column_info.computerName.empty()) {
        int computer_index = ui->column_computer_combo->findData(QString::fromStdString(column_info.computerName));
        if (computer_index >= 0) {
            ui->column_computer_combo->setCurrentIndex(computer_index);
            qDebug() << "Set computer combo to index" << computer_index << "after refresh";
            
            // Setup parameter UI for the loaded computer
            setupParameterUI(QString::fromStdString(column_info.computerName));
            
            // Load parameter values
            setParameterValues(column_info.parameters);
        } else {
            qDebug() << "Could not find computer" << QString::fromStdString(column_info.computerName) << "in refreshed combo box";
        }
    }

    // Reset flag
    _loading_column_configuration = false;

    qDebug() << "Loaded column configuration for:" << QString::fromStdString(column_info.name);
}

void TableDesignerWidget::saveCurrentColumnConfiguration() {
    qDebug() << "saveCurrentColumnConfiguration called, _updating_column_configuration =" << _updating_column_configuration;
    
    int current_row = ui->column_list->currentRow();
    if (current_row < 0 || _current_table_id.isEmpty() || !_data_manager) {
        qDebug() << "saveCurrentColumnConfiguration: Invalid state, returning. current_row=" << current_row << "table_id=" << _current_table_id;
        return;
    }

    // Don't save during column updates to prevent recursive modifications
    if (_updating_column_configuration) {
        qDebug() << "saveCurrentColumnConfiguration: Skipping due to _updating_column_configuration flag";
        return;
    }

    // Set flag to prevent reload during update
    _updating_column_configuration = true;

    qDebug() << "saveCurrentColumnConfiguration: Saving column" << current_row << "with name" << ui->column_name_edit->text();

    // Create column info from UI
    ColumnInfo column_info;
    column_info.name = ui->column_name_edit->text().trimmed().toStdString();
    column_info.description = ui->column_description_edit->toPlainText().trimmed().toStdString();
    column_info.dataSourceName = ui->column_data_source_combo->currentData().toString().toStdString();
    column_info.computerName = ui->column_computer_combo->currentData().toString().toStdString();
    column_info.parameters = getCurrentParameterValues();

    // Save to table manager
    if (auto* reg = _data_manager->getTableRegistry(); reg && reg->updateTableColumn(_current_table_id.toStdString(), current_row, column_info)) {
        qDebug() << "Saved column configuration for:" << QString::fromStdString(column_info.name);
    }

    // Reset flag
    _updating_column_configuration = false;
}

void TableDesignerWidget::clearColumnConfiguration() {
    ui->column_name_edit->clear();
    ui->column_description_edit->clear();
    ui->column_data_source_combo->setCurrentIndex(-1);
    ui->column_computer_combo->setCurrentIndex(-1);
    clearParameterUI();
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
            auto timeframe = _data_manager->getTime(TimeKey(source_name_str));
            if (!timeframe) {
                qDebug() << "TimeFrame not found:" << source_name;
                return nullptr;
            }

            // Use timestamps to select all rows
            std::vector<TimeFrameIndex> timestamps;
            for (int64_t i = 0; i < timeframe->getTotalFrameCount(); ++i) {
                timestamps.push_back(TimeFrameIndex(i));
            }
            return std::make_unique<TimestampSelector>(std::move(timestamps), timeframe);

        } else if (source_type == "Events") {
            // Create TimestampSelector using DigitalEventSeries
            auto event_series = _data_manager->getData<DigitalEventSeries>(source_name_str);
            if (!event_series) {
                qDebug() << "DigitalEventSeries not found:" << source_name;
                return nullptr;
            }

            auto events = event_series->getEventSeries();
            auto timeframe_key = _data_manager->getTimeKey(source_name_str);
            auto timeframe_obj = _data_manager->getTime(timeframe_key);
            if (!timeframe_obj) {
                qDebug() << "TimeFrame not found for events:" << timeframe_key.str();
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
            auto timeframe_key = _data_manager->getTimeKey(source_name_str);
            auto timeframe_obj = _data_manager->getTime(timeframe_key);
            if (!timeframe_obj) {
                qDebug() << "TimeFrame not found for intervals:" << timeframe_key.str();
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
                    start_point = std::max(start_point, int64_t(0));
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
    if (column_info.dataSourceName.empty() || column_info.computerName.empty()) {
        qDebug() << "Column" << QString::fromStdString(column_info.name) << "missing data source or computer configuration";
        return false;
    }

    try {
        // Get the data manager extension
        auto* reg = _data_manager->getTableRegistry();
        auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
        if (!data_manager_extension) {
            qDebug() << "DataManagerExtension not available";
            return false;
        }

        // Create DataSourceVariant from column data source
        DataSourceVariant data_source_variant;
        bool valid_source = false;

        QString column_source = QString::fromStdString(column_info.dataSourceName);

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
        } else if (column_source.startsWith("lines:")) {
            QString source_name = column_source.mid(6);// Remove "lines:" prefix
            auto line_source = data_manager_extension->getLineSource(source_name.toStdString());
            if (line_source) {
                data_source_variant = line_source;
                valid_source = true;
            }
        }

        if (!valid_source) {
            qDebug() << "Failed to create data source for column:" << column_info.name;
            return false;
        }

        // Create the computer from the registry
        auto const & registry = _data_manager->getTableRegistry()->getComputerRegistry();
        
        // Get type information for the computer
        auto computer_info_ptr = registry.findComputerInfo(column_info.computerName);
        if (!computer_info_ptr) {
            qDebug() << "Computer info not found for" << QString::fromStdString(column_info.computerName);
            return false;
        }
        
        auto computer_base = registry.createComputer(column_info.computerName, data_source_variant, column_info.parameters);
        if (!computer_base) {
            qDebug() << "Failed to create computer" << QString::fromStdString(column_info.computerName) << "for column:" << QString::fromStdString(column_info.name);
            // Continue; the computer may be multi-output and use a different factory
        }

        // Use the actual return type from the computer info instead of hardcoding double
        std::type_index return_type = computer_info_ptr->outputType;
        qDebug() << "Computer" << column_info.computerName << "returns type:" << computer_info_ptr->outputTypeName.c_str();
        
        // Handle different return types using runtime type dispatch
        bool success = false;
        
        if (computer_info_ptr->isMultiOutput) {
            // Multi-output: expand into multiple columns using builder.addColumns
            if (return_type == typeid(double)) {
                auto multi = registry.createTypedMultiComputer<double>(
                    column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    qDebug() << "Failed to create typed MULTI computer for" << QString::fromStdString(column_info.computerName);
                    return false;
                }
                builder.addColumns<double>(column_info.name, std::move(multi));
                success = true;
            } else if (return_type == typeid(int)) {
                auto multi = registry.createTypedMultiComputer<int>(
                    column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    qDebug() << "Failed to create typed MULTI computer<int> for" << QString::fromStdString(column_info.computerName);
                    return false;
                }
                builder.addColumns<int>(column_info.name, std::move(multi));
                success = true;
            } else if (return_type == typeid(bool)) {
                auto multi = registry.createTypedMultiComputer<bool>(
                    column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    qDebug() << "Failed to create typed MULTI computer<bool> for" << QString::fromStdString(column_info.computerName);
                    return false;
                }
                builder.addColumns<bool>(column_info.name, std::move(multi));
                success = true;
            } else {
                qDebug() << "Unsupported multi-output element type for" << QString::fromStdString(column_info.computerName);
                return false;
            }
        } else {
            if (return_type == typeid(double)) {
                success = addTypedColumnToBuilder<double>(builder, column_info, data_source_variant, registry);
            } else if (return_type == typeid(int)) {
                success = addTypedColumnToBuilder<int>(builder, column_info, data_source_variant, registry);
            } else if (return_type == typeid(bool)) {
                success = addTypedColumnToBuilder<bool>(builder, column_info, data_source_variant, registry);
            } else if (return_type == typeid(std::vector<double>)) {
                success = addTypedColumnToBuilder<std::vector<double>>(builder, column_info, data_source_variant, registry);
            } else if (return_type == typeid(std::vector<int>)) {
                success = addTypedColumnToBuilder<std::vector<int>>(builder, column_info, data_source_variant, registry);
            } else if (return_type == typeid(std::vector<float>)) {
                success = addTypedColumnToBuilder<std::vector<float>>(builder, column_info, data_source_variant, registry);
            } else {
                qDebug() << "Unsupported return type for computer" << column_info.computerName 
                         << "- type:" << computer_info_ptr->outputTypeName.c_str();
                return false;
            }
        }
        
        if (!success) {
            qDebug() << "Failed to create typed computer for" << QString::fromStdString(column_info.computerName);
            return false;
        }

        qDebug() << "Added column to builder:" << QString::fromStdString(column_info.name) << "with type:" << computer_info_ptr->outputTypeName.c_str();
        return true;

    } catch (std::exception const & e) {
        qDebug() << "Exception adding column" << QString::fromStdString(column_info.name) << "to builder:" << e.what();
        return false;
    }
}

template<typename T>
bool TableDesignerWidget::addTypedColumnToBuilder(TableViewBuilder & builder, 
                                                  ColumnInfo const & column_info,
                                                  DataSourceVariant const & data_source_variant,
                                                  ComputerRegistry const & registry) {
    try {
        // Use the registry's type-safe computer creation method with parameters
        auto typed_computer = registry.createTypedComputer<T>(
            column_info.computerName, 
            data_source_variant,
            column_info.parameters
        );
        
        if (!typed_computer) {
            qDebug() << "Failed to create typed computer" << column_info.computerName 
                     << "for type" << typeid(T).name();
            return false;
        }

        // Add the typed column to the builder
        builder.addColumn(column_info.name, std::move(typed_computer));
        
        qDebug() << "Successfully added typed column" << QString::fromStdString(column_info.name)
                 << "with type" << typeid(T).name();
        return true;
        
    } catch (std::exception const & e) {
        qDebug() << "Exception creating typed column" << QString::fromStdString(column_info.name)
                 << ":" << e.what();
        return false;
    }
}

void TableDesignerWidget::updateColumnIndices() {
    // Update UserRole data for all items to match their current position
    for (int i = 0; i < ui->column_list->count(); ++i) {
        auto* item = ui->column_list->item(i);
        if (item) {
            item->setData(Qt::UserRole, i);
        }
    }
    qDebug() << "Updated column indices for" << ui->column_list->count() << "items";
}

// Explicit template instantiations to ensure they're compiled
template bool TableDesignerWidget::addTypedColumnToBuilder<double>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);
template bool TableDesignerWidget::addTypedColumnToBuilder<int>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);
template bool TableDesignerWidget::addTypedColumnToBuilder<bool>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);
template bool TableDesignerWidget::addTypedColumnToBuilder<std::vector<double>>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);
template bool TableDesignerWidget::addTypedColumnToBuilder<std::vector<int>>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);
template bool TableDesignerWidget::addTypedColumnToBuilder<std::vector<float>>(TableViewBuilder &, ColumnInfo const &, DataSourceVariant const &, ComputerRegistry const &);

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

size_t TableDesignerWidget::computeTotalRowCountForRowSource(QString const & row_source) const {
    if (!_data_manager) return 0;
    if (row_source.startsWith("TimeFrame: ")) {
        auto name = row_source.mid(11).toStdString();
        auto tf = _data_manager->getTime(TimeKey(name));
        return tf ? static_cast<size_t>(tf->getTotalFrameCount()) : 0;
    }
    if (row_source.startsWith("Events: ")) {
        auto name = row_source.mid(8).toStdString();
        auto es = _data_manager->getData<DigitalEventSeries>(name);
        return es ? es->getEventSeries().size() : 0;
    }
    if (row_source.startsWith("Intervals: ")) {
        auto name = row_source.mid(11).toStdString();
        auto is = _data_manager->getData<DigitalIntervalSeries>(name);
        return is ? is->getDigitalIntervalSeries().size() : 0;
    }
    return 0;
}

std::unique_ptr<IRowSelector> TableDesignerWidget::createRowSelectorForWindow(QString const & row_source, size_t start, size_t size) const {
    if (!_data_manager) return nullptr;
    auto endExclusive = start + size;

    if (row_source.startsWith("TimeFrame: ")) {
        auto name = row_source.mid(11).toStdString();
        auto tf = _data_manager->getTime(TimeKey(name));
        if (!tf) return nullptr;
        size_t total = static_cast<size_t>(tf->getTotalFrameCount());
        start = std::min(start, total);
        endExclusive = std::min(endExclusive, total);
        std::vector<TimeFrameIndex> indices;
        indices.reserve(endExclusive - start);
        for (size_t i = start; i < endExclusive; ++i) indices.emplace_back(static_cast<int64_t>(i));
        return std::make_unique<TimestampSelector>(std::move(indices), tf);
    }

    if (row_source.startsWith("Events: ")) {
        auto name = row_source.mid(8).toStdString();
        auto es = _data_manager->getData<DigitalEventSeries>(name);
        if (!es) return nullptr;
        auto tfKey = _data_manager->getTimeKey(name);
        auto tf = _data_manager->getTime(tfKey);
        if (!tf) return nullptr;
        auto const & events = es->getEventSeries();
        size_t total = events.size();
        start = std::min(start, total);
        endExclusive = std::min(endExclusive, total);
        std::vector<TimeFrameIndex> indices;
        indices.reserve(endExclusive - start);
        for (size_t i = start; i < endExclusive; ++i) indices.emplace_back(static_cast<int64_t>(events[i]));
        return std::make_unique<TimestampSelector>(std::move(indices), tf);
    }

    if (row_source.startsWith("Intervals: ")) {
        auto name = row_source.mid(11).toStdString();
        auto is = _data_manager->getData<DigitalIntervalSeries>(name);
        if (!is) return nullptr;
        auto tfKey = _data_manager->getTimeKey(name);
        auto tf = _data_manager->getTime(tfKey);
        if (!tf) return nullptr;
        auto const & intervals = is->getDigitalIntervalSeries();
        size_t total = intervals.size();
        start = std::min(start, total);
        endExclusive = std::min(endExclusive, total);

        int capture = getCaptureRange();
        bool begin = isIntervalBeginningSelected();
        bool useSelf = isIntervalItselfSelected();

        std::vector<TimeFrameInterval> out;
        out.reserve(endExclusive - start);
        for (size_t i = start; i < endExclusive; ++i) {
            auto const & iv = intervals[i];
            if (useSelf) {
                out.emplace_back(TimeFrameIndex(iv.start), TimeFrameIndex(iv.end));
            } else {
                int64_t ref = begin ? iv.start : iv.end;
                int64_t s = std::max<int64_t>(0, ref - capture);
                int64_t e = std::min<int64_t>(tf->getTotalFrameCount() - 1, ref + capture);
                out.emplace_back(TimeFrameIndex(s), TimeFrameIndex(e));
            }
        }
        return std::make_unique<IntervalSelector>(std::move(out), tf);
    }

    return nullptr;
}

void TableDesignerWidget::updatePreviewSliderRange() {
    if (!ui->preview_slider) return;
    QString row_source = ui->row_data_source_combo ? ui->row_data_source_combo->currentText() : QString();
    _total_preview_rows = computeTotalRowCountForRowSource(row_source);
    int window = ui->preview_window_size_spinbox ? ui->preview_window_size_spinbox->value() : 8;
    long long maxStart = 0;
    if (_total_preview_rows > static_cast<size_t>(window)) maxStart = static_cast<long long>(_total_preview_rows - window);
    ui->preview_slider->blockSignals(true);
    ui->preview_slider->setMinimum(0);
    ui->preview_slider->setMaximum(static_cast<int>(std::min<long long>(std::numeric_limits<int>::max(), maxStart)));
    ui->preview_slider->blockSignals(false);
}

void TableDesignerWidget::triggerPreviewDebounced() {
    if (!ui->preview_auto_refresh_checkbox || ui->preview_auto_refresh_checkbox->isChecked()) {
        if (_preview_debounce_timer) _preview_debounce_timer->start();
    }
}

void TableDesignerWidget::rebuildPreviewNow() {
    if (!_data_manager || !_preview_model) return;
    if (_current_table_id.isEmpty()) { _preview_model->clearPreview(); return; }

    QString row_source = ui->row_data_source_combo ? ui->row_data_source_combo->currentText() : QString();
    if (row_source.isEmpty()) { _preview_model->clearPreview(); return; }
    if (ui->column_list->count() == 0) { _preview_model->clearPreview(); return; }

    int start = ui->preview_slider ? ui->preview_slider->value() : 0;
    int window = ui->preview_window_size_spinbox ? ui->preview_window_size_spinbox->value() : 8;

    auto * reg = _data_manager->getTableRegistry();
    if (!reg) { _preview_model->clearPreview(); return; }
    auto table_info = reg->getTableInfo(_current_table_id.toStdString());
    if (table_info.columns.empty()) { _preview_model->clearPreview(); return; }
    auto data_manager_extension = reg->getDataManagerExtension();
    if (!data_manager_extension) { _preview_model->clearPreview(); return; }

    auto future = QtConcurrent::run([this, row_source, start, window, table_info = std::move(table_info), data_manager_extension]() -> std::shared_ptr<TableView> {
        try {
            auto selector = createRowSelectorForWindow(row_source, static_cast<size_t>(start), static_cast<size_t>(window));
            if (!selector) return nullptr;
            TableViewBuilder builder(data_manager_extension);
            builder.setRowSelector(std::move(selector));
            for (auto const & col : table_info.columns) {
                if (!addColumnToBuilder(builder, col)) {
                    return nullptr;
                }
            }
            auto view = std::make_shared<TableView>(builder.build());
            view->materializeAll();
            return view;
        } catch (...) {
            return nullptr;
        }
    });

    auto * watcher = new QFutureWatcher<std::shared_ptr<TableView>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<TableView>>::finished, this, [this, watcher]() {
        auto result = watcher->result();
        watcher->deleteLater();
        if (result) {
            _preview_model->setPreview(std::move(result));
        } else {
            _preview_model->clearPreview();
        }
    });
    watcher->setFuture(future);
}
void TableDesignerWidget::setupParameterUI(QString const & computerName) {
    clearParameterUI();
    
    if (!_data_manager || computerName.isEmpty()) {
        return;
    }
    
    // Get computer info from table manager
    auto* reg = _data_manager->getTableRegistry();
    auto computer_info = reg ? reg->getComputerInfo(computerName.toStdString()) : nullptr;
    if (!computer_info) {
        return;
    }
    
    // Check if this computer has parameters
    if (!computer_info->hasParameters()) {
        _parameter_widget->setVisible(false);
        return;
    }
    
    _parameter_widget->setVisible(true);
    
    // Create a group box for the parameters
    auto* group_box = new QGroupBox("Parameters", _parameter_widget);
    auto* group_layout = new QVBoxLayout(group_box);
    
    // Create controls for each parameter
    for (const auto& param_desc : computer_info->parameterDescriptors) {
        auto* control = createParameterControl(param_desc.get());
        if (control) {
            _parameter_controls[param_desc->getName()] = control;
            
            // Create label and control layout
            auto* param_layout = new QHBoxLayout();
            auto* label = new QLabel(QString::fromStdString(param_desc->getName()) + ":", group_box);
            label->setToolTip(QString::fromStdString(param_desc->getDescription()));
            
            param_layout->addWidget(label);
            param_layout->addWidget(control);
            param_layout->setStretch(1, 1); // Give control more space
            
            group_layout->addLayout(param_layout);
        }
    }
    
    _parameter_layout->addWidget(group_box);
}

void TableDesignerWidget::clearParameterUI() {
    // Clear the parameter controls map
    _parameter_controls.clear();
    
    // Remove all widgets from the parameter layout
    while (QLayoutItem* item = _parameter_layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    
    _parameter_widget->setVisible(false);
}

QWidget* TableDesignerWidget::createParameterControl(IParameterDescriptor const * descriptor) {
    if (!descriptor) {
        return nullptr;
    }
    
    QString uiHint = QString::fromStdString(descriptor->getUIHint());
    
    if (uiHint == "enum") {
        // Create combo box for enum parameters
        auto* combo = new QComboBox();
        
        auto properties = descriptor->getUIProperties();
        QString optionsStr = QString::fromStdString(properties["options"]);
        QString defaultValue = QString::fromStdString(properties["default"]);
        
        // Parse options (comma-separated)
        QStringList options = optionsStr.split(",", Qt::SkipEmptyParts);
        for (const QString& option : options) {
            combo->addItem(option.trimmed());
        }
        
        // Set default value if specified
        if (!defaultValue.isEmpty()) {
            int index = combo->findText(defaultValue);
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
        }
        
        // Connect signal to save configuration when parameter changes
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &TableDesignerWidget::saveCurrentColumnConfiguration);
        
        return combo;
    } else if (uiHint == "text") {
        // Create line edit for text parameters
        auto* lineEdit = new QLineEdit();
        
        auto properties = descriptor->getUIProperties();
        QString defaultValue = QString::fromStdString(properties["default"]);
        
        if (!defaultValue.isEmpty()) {
            lineEdit->setText(defaultValue);
        }
        
        // Connect signal to save configuration when parameter changes
        connect(lineEdit, &QLineEdit::textChanged,
                this, &TableDesignerWidget::saveCurrentColumnConfiguration);
        
        return lineEdit;
    } else if (uiHint == "number") {
        // Create line edit for number parameters (could be enhanced with validators)
        auto* lineEdit = new QLineEdit();
        
        auto properties = descriptor->getUIProperties();
        QString defaultValue = QString::fromStdString(properties["default"]);
        
        if (!defaultValue.isEmpty()) {
            lineEdit->setText(defaultValue);
        }
        
        // Connect signal to save configuration when parameter changes
        connect(lineEdit, &QLineEdit::textChanged,
                this, &TableDesignerWidget::saveCurrentColumnConfiguration);
        
        return lineEdit;
    }
    
    // Unsupported parameter type, create a simple text edit
    auto* lineEdit = new QLineEdit();
    lineEdit->setEnabled(false);
    lineEdit->setPlaceholderText("Unsupported parameter type: " + uiHint);
    return lineEdit;
}

std::map<std::string, std::string> TableDesignerWidget::getCurrentParameterValues() const {
    std::map<std::string, std::string> values;
    
    for (const auto& [paramName, widget] : _parameter_controls) {
        if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            values[paramName] = combo->currentText().toStdString();
        } else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            values[paramName] = lineEdit->text().toStdString();
        }
    }
    
    return values;
}

void TableDesignerWidget::setParameterValues(std::map<std::string, std::string> const & parameters) {
    for (const auto& [paramName, paramValue] : parameters) {
        auto it = _parameter_controls.find(paramName);
        if (it != _parameter_controls.end()) {
            QWidget* widget = it->second;
            
            if (auto* combo = qobject_cast<QComboBox*>(widget)) {
                int index = combo->findText(QString::fromStdString(paramValue));
                if (index >= 0) {
                    combo->setCurrentIndex(index);
                }
            } else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
                lineEdit->setText(QString::fromStdString(paramValue));
            }
        }
    }
}

