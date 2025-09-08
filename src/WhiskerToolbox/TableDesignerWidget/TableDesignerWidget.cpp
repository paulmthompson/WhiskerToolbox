#include "TableDesignerWidget.hpp"
#include "ui_TableDesignerWidget.h"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/TableEvents.hpp"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/interfaces/IColumnComputer.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/transforms/PCATransform.hpp"
#include "TableInfoWidget.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "TableViewerWidget/TableViewerWidget.hpp"


#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QTableView>

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <tuple>
#include <typeindex>
#include <vector>

TableDesignerWidget::TableDesignerWidget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableDesignerWidget),
      _data_manager(std::move(data_manager)) {

    ui->setupUi(this);
    
    _parameter_widget = nullptr;
    _parameter_layout = nullptr;
    
    // Initialize table viewer widget for preview
    _table_viewer = new TableViewerWidget(this);
    
    // Add the table viewer widget to the preview layout
    ui->preview_layout->addWidget(_table_viewer);
    
    _preview_debounce_timer = new QTimer(this);
    _preview_debounce_timer->setSingleShot(true);
    _preview_debounce_timer->setInterval(150);
    connect(_preview_debounce_timer, &QTimer::timeout, this, &TableDesignerWidget::rebuildPreviewNow);

    _table_info_widget = new TableInfoWidget(this);
    _table_info_section = new Section(this, "Table Information");
    _table_info_section->setContentLayout(*new QVBoxLayout());
    _table_info_section->layout()->addWidget(_table_info_widget);
    _table_info_section->autoSetContentLayout();
    // Replace existing Table Information contents in UI with the Section
    if (ui->table_info_layout) {
        QLayoutItem * child;
        while ((child = ui->table_info_layout->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->setParent(nullptr);
            delete child;
        }
        ui->table_info_layout->addWidget(_table_info_section);
    }
    // Hook save from table info widget
    connect(_table_info_widget, &TableInfoWidget::saveClicked, this, &TableDesignerWidget::onSaveTableInfo);

    // Connect table viewer signals for better integration
    connect(_table_viewer, &TableViewerWidget::rowScrolled, this, [this](size_t row_index) {
        // Optional: Could emit a signal or update status when user scrolls preview
        // For now, just ensure the table viewer is working as expected
        Q_UNUSED(row_index)
    });

    connectSignals();
    // Initialize UI to a clean state, then populate controls
    clearUI();
    refreshTableCombo();
    refreshRowDataSourceCombo();
    refreshComputersTree();

    // Add observer to automatically refresh dropdowns when DataManager changes
    if (_data_manager) {
        _data_manager->addObserver([this]() {
            refreshAllDataSources();
        });
    }

    qDebug() << "TableDesignerWidget initialized with TableViewerWidget for efficient pagination";
}

TableDesignerWidget::~TableDesignerWidget() {
    delete ui;
}

void TableDesignerWidget::refreshAllDataSources() {
    qDebug() << "Manually refreshing all data sources...";
    refreshRowDataSourceCombo();
    refreshComputersTree();

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

    // Table info signals are connected via TableInfoWidget

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

    // Column design signals (tree-based)
    connect(ui->computers_tree, &QTreeWidget::itemChanged,
            this, &TableDesignerWidget::onComputersTreeItemChanged);
    connect(ui->computers_tree, &QTreeWidget::itemChanged,
            this, &TableDesignerWidget::onComputersTreeItemEdited);

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
        (void) token;// Optionally store and remove on dtor
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
    // Table info section is controlled separately
    ui->build_table_btn->setEnabled(true);
    if (auto gb = this->findChild<QGroupBox*>("row_source_group")) gb->setEnabled(true);
    if (auto gb = this->findChild<QGroupBox*>("column_design_group")) gb->setEnabled(true);
    // Enable save info within TableInfoWidget
    if (_table_info_section) _table_info_section->setEnabled(true);

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
        if (auto * reg = _data_manager->getTableRegistry()) {
            reg->updateTableRowSource(_current_table_id.toStdString(), selected.toStdString());
        }
    }

    // Update the info label
    updateRowInfoLabel(selected);

    // Update interval settings visibility
    updateIntervalSettingsVisibility();

    // Refresh computers tree since available computers depend on row selector type
    refreshComputersTree();

    qDebug() << "Row data source changed to:" << selected;
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

    // Get enabled column infos from the tree
    auto column_infos = getEnabledColumnInfos();
    if (column_infos.empty()) {
        updateBuildStatus("No computers enabled. Check boxes in the tree to enable computers.", true);
        return;
    }

    try {
        // Create the row selector
        auto row_selector = createRowSelector(row_source);
        if (!row_selector) {
            updateBuildStatus("Failed to create row selector", true);
            return;
        }

        // Get the data manager extension
        auto * reg = _data_manager->getTableRegistry();
        auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
        if (!data_manager_extension) {
            updateBuildStatus("DataManager extension not available", true);
            return;
        }

        // Create the TableViewBuilder
        TableViewBuilder builder(data_manager_extension);
        builder.setRowSelector(std::move(row_selector));

        // Add all enabled columns from the tree
        bool all_columns_valid = true;
        for (auto const & column_info: column_infos) {
            if (!reg->addColumnToBuilder(builder, column_info)) {
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

        // Store the built table in the TableManager and update table info with current columns
        if (reg) {
            // Update table info with current column configuration
            auto table_info = reg->getTableInfo(_current_table_id.toStdString());
            table_info.columns = column_infos;// Store the current enabled columns
            reg->updateTableInfo(_current_table_id.toStdString(),
                                 table_info.name, table_info.description);

            // Store the built table
            if (reg->storeBuiltTable(_current_table_id.toStdString(), std::move(table_view))) {
                updateBuildStatus(QString("Table built successfully with %1 columns!").arg(column_infos.size()));
                qDebug() << "Successfully built table:" << _current_table_id << "with" << column_infos.size() << "columns";
            } else {
                updateBuildStatus("Failed to store built table", true);
            }
        } else {
            updateBuildStatus("Registry unavailable", true);
        }

    } catch (std::exception const & e) {
        updateBuildStatus(QString("Error building table: %1").arg(e.what()), true);
        qDebug() << "Exception during table building:" << e.what();
    }
}

bool TableDesignerWidget::buildTableFromTree() {
    // This is essentially the same as onBuildTable but returns success status
    if (_current_table_id.isEmpty()) {
        updateBuildStatus("No table selected", true);
        return false;
    }

    QString row_source = ui->row_data_source_combo->currentText();
    if (row_source.isEmpty()) {
        updateBuildStatus("No row data source selected", true);
        return false;
    }

    // Get enabled column infos from the tree
    auto column_infos = getEnabledColumnInfos();
    if (column_infos.empty()) {
        updateBuildStatus("No computers enabled. Check boxes in the tree to enable computers.", true);
        return false;
    }

    try {
        // Create the row selector
        auto row_selector = createRowSelector(row_source);
        if (!row_selector) {
            updateBuildStatus("Failed to create row selector", true);
            return false;
        }

        // Get the data manager extension
        auto * reg = _data_manager->getTableRegistry();
        auto data_manager_extension = reg ? reg->getDataManagerExtension() : nullptr;
        if (!data_manager_extension) {
            updateBuildStatus("DataManager extension not available", true);
            return false;
        }

        // Create the TableViewBuilder
        TableViewBuilder builder(data_manager_extension);
        builder.setRowSelector(std::move(row_selector));

        // Add all enabled columns from the tree
        bool all_columns_valid = true;
        for (auto const & column_info: column_infos) {
            if (!reg->addColumnToBuilder(builder, column_info)) {
                updateBuildStatus(QString("Failed to create column: %1").arg(QString::fromStdString(column_info.name)), true);
                all_columns_valid = false;
                break;
            }
        }

        if (!all_columns_valid) {
            return false;
        }

        // Build the table
        auto table_view = builder.build();

        // Store the built table in the TableManager and update table info with current columns
        if (reg) {
            // Update table info with current column configuration
            auto table_info = reg->getTableInfo(_current_table_id.toStdString());
            table_info.columns = column_infos;// Store the current enabled columns
            reg->updateTableInfo(_current_table_id.toStdString(),
                                 table_info.name,
                                 table_info.description);

            // Store the built table
            if (reg->storeBuiltTable(_current_table_id.toStdString(), std::move(table_view))) {
                updateBuildStatus(QString("Table built successfully with %1 columns!").arg(column_infos.size()));
                qDebug() << "Successfully built table:" << _current_table_id << "with" << column_infos.size() << "columns";
                return true;
            } else {
                updateBuildStatus("Failed to store built table", true);
                return false;
            }
        } else {
            updateBuildStatus("Registry unavailable", true);
            return false;
        }

    } catch (std::exception const & e) {
        updateBuildStatus(QString("Error building table: %1").arg(e.what()), true);
        qDebug() << "Exception during table building:" << e.what();
        return false;
    }
}

void TableDesignerWidget::onApplyTransform() {
    if (_current_table_id.isEmpty() || !_data_manager) {
        updateBuildStatus("No base table selected", true);
        return;
    }

    // Fetch the built base table
    auto * reg = _data_manager->getTableRegistry();
    if (!reg) {
        updateBuildStatus("Registry unavailable", true);
        return;
    }
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
        for (auto const & s: parseCommaSeparatedList(ui->transform_include_edit->text())) cfg.include.push_back(s);
    }
    if (ui->transform_exclude_edit) {
        for (auto const & s: parseCommaSeparatedList(ui->transform_exclude_edit->text())) cfg.exclude.push_back(s);
    }

    try {
        PCATransform pca(cfg);
        TableView derived = pca.apply(*base_view);

        // Determine output id/name
        QString out_name = ui->transform_output_name_edit ? ui->transform_output_name_edit->text().trimmed()
                                                          : QString();
        if (out_name.isEmpty()) {
            QString base = _table_info_widget ? _table_info_widget->getName() : QString();
            out_name = base.isEmpty() ? QString("(PCA)") : QString("%1 (PCA)").arg(base);
        }

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
    for (QString s: text.split(",", Qt::SkipEmptyParts)) {
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
    if (!reg) {
        updateBuildStatus("Registry unavailable", true);
        return;
    }
    auto view = reg->getBuiltTable(_current_table_id.toStdString());
    if (!view) {
        updateBuildStatus("Build the table first", true);
        return;
    }

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
    else if (delimiter == "Tab")
        delim = "\t";
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
                    if (r < vals.size()) file << vals[r];
                    else
                        file << "NaN";
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
    return QFileDialog::getSaveFileName(const_cast<TableDesignerWidget *>(this), "Export Table to CSV", QString(), "CSV Files (*.csv)");
}

void TableDesignerWidget::onSaveTableInfo() {
    if (_current_table_id.isEmpty()) {
        return;
    }

    QString name = _table_info_widget ? _table_info_widget->getName() : QString();
    QString description = _table_info_widget ? _table_info_widget->getDescription() : QString();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Table name cannot be empty");
        return;
    }

    if (auto * reg = _data_manager->getTableRegistry(); reg && reg->updateTableInfo(_current_table_id.toStdString(), name.toStdString(), description.toStdString())) {
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
    if (_current_table_id == table_id && !_loading_column_configuration) {
        loadTableInfo(table_id);
    }
    qDebug() << "Table info updated signal received:" << table_id;
}

void TableDesignerWidget::refreshTableCombo() {
    ui->table_combo->clear();

    auto * reg = _data_manager->getTableRegistry();
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
        // Only include valid row sources in this combo
        if (source.startsWith("TimeFrame: ") || source.startsWith("Events: ") || source.startsWith("Intervals: ")) {
            ui->row_data_source_combo->addItem(source);
        }
    }

    if (ui->row_data_source_combo->count() == 0) {
        ui->row_data_source_combo->addItem("(No data sources available)");
        qDebug() << "refreshRowDataSourceCombo: No data sources available";
    }
}


void TableDesignerWidget::loadTableInfo(QString const & table_id) {
    if (table_id.isEmpty() || !_data_manager) {
        clearUI();
        return;
    }

    auto * reg = _data_manager->getTableRegistry();
    auto info = reg ? reg->getTableInfo(table_id.toStdString()) : TableInfo{};
    if (info.id.empty()) {
        clearUI();
        return;
    }

    // Load table information
    if (_table_info_widget) {
        _table_info_widget->setName(QString::fromStdString(info.name));
        _table_info_widget->setDescription(QString::fromStdString(info.description));
    }

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

            // Since signals were blocked, this will ensure the tree is refreshed
            // when the computers tree is populated later in this function
        }
    }

    // Clear old column list (deprecated)
    // The computers tree will be populated based on available data sources
    refreshComputersTree();

    updateBuildStatus(QString("Loaded table: %1").arg(QString::fromStdString(info.name)));
    triggerPreviewDebounced();
}

void TableDesignerWidget::clearUI() {
    _current_table_id.clear();

    // Clear table info
    if (_table_info_widget) {
        _table_info_widget->setName("");
        _table_info_widget->setDescription("");
    }

    // Clear row source
    ui->row_data_source_combo->setCurrentIndex(-1);
    ui->row_info_label->setText("No row source selected");

    // Reset capture range and interval settings
    setCaptureRange(30000);// Default value
    if (ui->interval_beginning_radio) {
        ui->interval_beginning_radio->setChecked(true);
    }
    if (ui->interval_itself_radio) {
        ui->interval_itself_radio->setChecked(false);
    }
    if (ui->interval_settings_group) {
        ui->interval_settings_group->setVisible(false);
    }

    // Clear computers tree
    if (ui->computers_tree) {
        ui->computers_tree->clear();
    }

    // Disable controls
    ui->delete_table_btn->setEnabled(false);
    // Table info section is controlled separately
    ui->build_table_btn->setEnabled(false);
    if (auto gb = this->findChild<QGroupBox*>("row_source_group")) gb->setEnabled(false);
    if (auto gb = this->findChild<QGroupBox*>("column_design_group")) gb->setEnabled(false);
    if (_table_info_section) _table_info_section->setEnabled(false);

    updateBuildStatus("No table selected");
    if (_table_viewer) _table_viewer->clearTable();
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

    auto * reg = _data_manager->getTableRegistry();
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

    // Add AnalogTimeSeries keys as data sources (for computers; not row selectors)
    auto analog_keys = _data_manager->getKeys<AnalogTimeSeries>();
    qDebug() << "getAvailableDataSources: Analog keys:" << analog_keys.size();
    for (auto const & key: analog_keys) {
        QString source = QString("analog:%1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added Analog:" << source;
    }

    qDebug() << "getAvailableDataSources: Total sources found:" << sources.size();

    return sources;
}

std::pair<std::optional<DataSourceVariant>, RowSelectorType>
TableDesignerWidget::createDataSourceVariant(QString const & data_source_string,
                                             std::shared_ptr<DataManagerExtension> data_manager_extension) const {
    std::optional<DataSourceVariant> result;
    RowSelectorType row_selector_type = RowSelectorType::IntervalBased;

    if (data_source_string.startsWith("TimeFrame: ")) {
        // TimeFrames are used with TimestampSelector for rows; no concrete data source needed
        row_selector_type = RowSelectorType::Timestamp;

    } else if (data_source_string.startsWith("Events: ")) {
        QString source_name = data_source_string.mid(8);// Remove "Events: " prefix
        // Event-based computers in the registry operate with interval rows
        row_selector_type = RowSelectorType::IntervalBased;

        if (auto event_source = data_manager_extension->getEventSource(source_name.toStdString())) {
            result = event_source;
        }

    } else if (data_source_string.startsWith("Intervals: ")) {
        QString source_name = data_source_string.mid(11);// Remove "Intervals: " prefix
        row_selector_type = RowSelectorType::IntervalBased;

        if (auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString())) {
            result = interval_source;
        }
    } else if (data_source_string.startsWith("analog:")) {
        QString source_name = data_source_string.mid(7);// Remove "analog:" prefix
        row_selector_type = RowSelectorType::IntervalBased;

        if (auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString())) {
            result = analog_source;
        }
    }

    return {result, row_selector_type};
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

    auto * reg3 = _data_manager->getTableRegistry();
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
    // Use the simplified TableRegistry method that handles all the type checking internally
    auto * reg = _data_manager->getTableRegistry();
    if (!reg) {
        qDebug() << "TableRegistry not available";
        return false;
    }

    bool success = reg->addColumnToBuilder(builder, column_info);
    if (!success) {
        qDebug() << "Failed to add column to builder:" << QString::fromStdString(column_info.name);
    }

    return success;
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

void TableDesignerWidget::triggerPreviewDebounced() {
    if (_preview_debounce_timer) _preview_debounce_timer->start();
    // Also trigger an immediate rebuild to support non-interactive/test contexts
    rebuildPreviewNow();
}

void TableDesignerWidget::rebuildPreviewNow() {
    if (!_data_manager || !_table_viewer) return;
    if (_current_table_id.isEmpty()) {
        _table_viewer->clearTable();
        return;
    }

    QString row_source = ui->row_data_source_combo ? ui->row_data_source_combo->currentText() : QString();
    if (row_source.isEmpty()) {
        _table_viewer->clearTable();
        return;
    }

    // Get enabled column infos from the computers tree
    auto column_infos = getEnabledColumnInfos();
    if (column_infos.empty()) {
        _table_viewer->clearTable();
        return;
    }

    // Create row selector for the entire dataset
    auto selector = createRowSelector(row_source);
    if (!selector) {
        _table_viewer->clearTable();
        return;
    }

    // Apply any saved column order for this table id
    auto desiredOrder = _table_column_order.value(_current_table_id);
    if (!desiredOrder.isEmpty()) {
        std::vector<ColumnInfo> reordered;
        reordered.reserve(column_infos.size());
        for (auto const & name : desiredOrder) {
            auto it = std::find_if(column_infos.begin(), column_infos.end(), [&](ColumnInfo const & ci){ return QString::fromStdString(ci.name) == name; });
            if (it != column_infos.end()) {
                reordered.push_back(*it);
            }
        }
        for (auto const & ci : column_infos) {
            if (std::find_if(reordered.begin(), reordered.end(), [&](ColumnInfo const & x){ return x.name == ci.name; }) == reordered.end()) {
                reordered.push_back(ci);
            }
        }
        column_infos = std::move(reordered);
    }

    // Set up the table viewer with pagination
    _table_viewer->setTableConfiguration(
            std::move(selector),
            std::move(column_infos),
            _data_manager,
            QString("Preview: %1").arg(_current_table_id));

    // Capture the current visual order from the viewer
    QStringList currentOrder;
    if (_table_viewer) {
        auto * tv = _table_viewer->findChild<QTableView*>();
        if (tv && tv->model()) {
            auto * header = tv->horizontalHeader();
            int cols = tv->model()->columnCount();
            for (int v = 0; header && v < cols; ++v) {
                int logical = header->logicalIndex(v);
                auto name = tv->model()->headerData(logical, Qt::Horizontal, Qt::DisplayRole).toString();
                currentOrder.push_back(name);
            }
        }
    }
    if (!currentOrder.isEmpty()) {
        _table_column_order[_current_table_id] = currentOrder;
    }
}

void TableDesignerWidget::refreshComputersTree() {
    if (!_data_manager) return;

    _updating_computers_tree = true;

    // Preserve previous checkbox states and custom column names
    std::map<std::string, std::pair<Qt::CheckState, QString>> previous_states;
    if (ui->computers_tree && ui->computers_tree->topLevelItemCount() > 0) {
        for (int i = 0; i < ui->computers_tree->topLevelItemCount(); ++i) {
            auto * data_source_item_old = ui->computers_tree->topLevelItem(i);
            for (int j = 0; j < data_source_item_old->childCount(); ++j) {
                auto * computer_item_old = data_source_item_old->child(j);
                QString ds = computer_item_old->data(0, Qt::UserRole).toString();
                QString cn = computer_item_old->data(1, Qt::UserRole).toString();
                std::string key = (ds + "||" + cn).toStdString();
                previous_states[key] = {computer_item_old->checkState(1), computer_item_old->text(2)};
            }
        }
    }

    ui->computers_tree->clear();
    ui->computers_tree->setHeaderLabels({"Data Source / Computer", "Enabled", "Column Name"});

    auto * registry = _data_manager->getTableRegistry();
    if (!registry) {
        _updating_computers_tree = false;
        return;
    }

    auto data_manager_extension = registry->getDataManagerExtension();
    if (!data_manager_extension) {
        _updating_computers_tree = false;
        return;
    }

    auto & computer_registry = registry->getComputerRegistry();

    // Get available data sources
    auto data_sources = getAvailableDataSources();

    // Create tree structure: Data Source -> Computers
    for (QString const & data_source: data_sources) {
        auto * data_source_item = new QTreeWidgetItem(ui->computers_tree);
        data_source_item->setText(0, data_source);
        data_source_item->setFlags(Qt::ItemIsEnabled);
        data_source_item->setExpanded(false);// Start collapsed

        // Convert data source string to DataSourceVariant and determine RowSelectorType
        auto [data_source_variant, row_selector_type] = createDataSourceVariant(data_source, data_manager_extension);

        if (!data_source_variant.has_value()) {
            qDebug() << "Failed to create data source variant for:" << data_source;
            continue;
        }

        // Get available computers for this specific data source and row selector combination
        auto available_computers = computer_registry.getAvailableComputers(row_selector_type, data_source_variant.value());

        // Add compatible computers as children
        for (auto const & computer_info: available_computers) {
            auto * computer_item = new QTreeWidgetItem(data_source_item);
            computer_item->setText(0, QString::fromStdString(computer_info.name));
            computer_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
            computer_item->setCheckState(1, Qt::Unchecked);

            // Generate default column name
            QString default_name = generateDefaultColumnName(data_source, QString::fromStdString(computer_info.name));
            computer_item->setText(2, default_name);
            computer_item->setFlags(computer_item->flags() | Qt::ItemIsEditable);

            // Store data source and computer name for later use
            computer_item->setData(0, Qt::UserRole, data_source);
            computer_item->setData(1, Qt::UserRole, QString::fromStdString(computer_info.name));

            // Restore previous state if present
            std::string prev_key = (data_source + "||" + QString::fromStdString(computer_info.name)).toStdString();
            auto it_prev = previous_states.find(prev_key);
            if (it_prev != previous_states.end()) {
                computer_item->setCheckState(1, it_prev->second.first);
                if (!it_prev->second.second.isEmpty()) {
                    computer_item->setText(2, it_prev->second.second);
                }
            }
        }
    }

    // Resize columns to content
    ui->computers_tree->resizeColumnToContents(0);
    ui->computers_tree->resizeColumnToContents(1);
    ui->computers_tree->resizeColumnToContents(2);

    _updating_computers_tree = false;

    // Update preview after refresh
    triggerPreviewDebounced();
}

void TableDesignerWidget::onComputersTreeItemChanged() {
    if (_updating_computers_tree) return;

    // Trigger preview update when checkbox states change
    triggerPreviewDebounced();
}

void TableDesignerWidget::onComputersTreeItemEdited(QTreeWidgetItem * item, int column) {
    if (_updating_computers_tree) return;

    // Only respond to column name edits (column 2)
    if (column == 2) {
        // Column name was edited, trigger preview update
        triggerPreviewDebounced();
    }
}

std::vector<ColumnInfo> TableDesignerWidget::getEnabledColumnInfos() const {
    std::vector<ColumnInfo> column_infos;

    if (!ui->computers_tree) return column_infos;

    // Iterate through all data source items
    for (int i = 0; i < ui->computers_tree->topLevelItemCount(); ++i) {
        auto * data_source_item = ui->computers_tree->topLevelItem(i);

        // Iterate through computer items under each data source
        for (int j = 0; j < data_source_item->childCount(); ++j) {
            auto * computer_item = data_source_item->child(j);

            // Check if this computer is enabled
            if (computer_item->checkState(1) == Qt::Checked) {
                QString data_source = computer_item->data(0, Qt::UserRole).toString();
                QString computer_name = computer_item->data(1, Qt::UserRole).toString();
                QString column_name = computer_item->text(2);

                if (column_name.isEmpty()) {
                    column_name = generateDefaultColumnName(data_source, computer_name);
                }

                // Create ColumnInfo (use raw key without UI prefixes)
                QString source_key = data_source;
                if (source_key.startsWith("Events: ")) {
                    source_key = source_key.mid(8);
                } else if (source_key.startsWith("Intervals: ")) {
                    source_key = source_key.mid(11);
                } else if (source_key.startsWith("analog:")) {
                    source_key = source_key.mid(7);
                } else if (source_key.startsWith("TimeFrame: ")) {
                    source_key = source_key.mid(11);
                }

                ColumnInfo info(column_name.toStdString(),
                                QString("Column from %1 using %2").arg(data_source, computer_name).toStdString(),
                                source_key.toStdString(),
                                computer_name.toStdString());

                // Set output type based on computer info
                if (auto * registry = _data_manager->getTableRegistry()) {
                    auto & computer_registry = registry->getComputerRegistry();
                    auto computer_info = computer_registry.findComputerInfo(computer_name.toStdString());
                    if (computer_info) {
                        info.outputType = computer_info->outputType;
                        info.outputTypeName = computer_info->outputTypeName;
                        info.isVectorType = computer_info->isVectorType;
                        if (info.isVectorType) {
                            info.elementType = computer_info->elementType;
                            info.elementTypeName = computer_info->elementTypeName;
                        }
                    }
                }

                column_infos.push_back(std::move(info));
            }
        }
    }

    return column_infos;
}

bool TableDesignerWidget::isComputerCompatibleWithDataSource(std::string const & computer_name, QString const & data_source) const {
    if (!_data_manager) return false;

    auto * registry = _data_manager->getTableRegistry();
    if (!registry) return false;

    auto & computer_registry = registry->getComputerRegistry();
    auto computer_info = computer_registry.findComputerInfo(computer_name);
    if (!computer_info) return false;

    // Basic compatibility check based on data source type and common computer patterns
    if (data_source.startsWith("Events: ")) {
        // Event-based computers typically have "Event" in their name
        return computer_name.find("Event") != std::string::npos;
    } else if (data_source.startsWith("Intervals: ")) {
        // Interval-based computers typically work with intervals or events
        return computer_name.find("Event") != std::string::npos ||
               computer_name.find("Interval") != std::string::npos;
    } else if (data_source.startsWith("analog:")) {
        // Analog-based computers typically have "Analog" in their name
        return computer_name.find("Analog") != std::string::npos;
    } else if (data_source.startsWith("TimeFrame: ")) {
        // TimeFrame-based computers - generally most computers can work with timestamps
        return computer_name.find("Timestamp") != std::string::npos ||
               computer_name.find("Time") != std::string::npos;
    }

    // Default: assume compatibility for unrecognized patterns
    return true;
}

QString TableDesignerWidget::generateDefaultColumnName(QString const & data_source, QString const & computer_name) const {
    QString source_name = data_source;

    // Extract the actual name from prefixed data sources
    if (source_name.startsWith("Events: ")) {
        source_name = source_name.mid(8);
    } else if (source_name.startsWith("Intervals: ")) {
        source_name = source_name.mid(11);
    } else if (source_name.startsWith("analog:")) {
        source_name = source_name.mid(7);
    } else if (source_name.startsWith("TimeFrame: ")) {
        source_name = source_name.mid(11);
    }

    // Create a concise name
    return QString("%1_%2").arg(source_name, computer_name);
}

