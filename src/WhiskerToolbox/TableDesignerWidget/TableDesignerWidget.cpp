#include "TableDesignerWidget.hpp"
#include "ui_TableDesignerWidget.h"

#include "Collapsible_Widget/Section.hpp"
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
#include "Entity/EntityGroupManager.hpp"
#include "TableExportWidget.hpp"
#include "TableInfoWidget.hpp"
#include "TableJSONWidget.hpp"
#include "TableTransformWidget.hpp"
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
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <tuple>
#include <typeindex>
#include <vector>

TableDesignerWidget::TableDesignerWidget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TableDesignerWidget),
      _data_manager(std::move(data_manager)) {

    ui->setupUi(this);

    // Configure combo boxes for better scrolling with many items
    // Combo box scrolling is now handled by the global stylesheet

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
    ui->main_layout->insertWidget(1, _table_info_section);

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

    // Insert Transform section
    _table_transform_widget = new TableTransformWidget(this);
    _table_transform_section = new Section(this, "Transforms");
    _table_transform_section->setContentLayout(*new QVBoxLayout());
    _table_transform_section->layout()->addWidget(_table_transform_widget);
    _table_transform_section->autoSetContentLayout();
    // Place after build_group (after preview)
    ui->main_layout->insertWidget(ui->main_layout->indexOf(ui->build_group) + 1, _table_transform_section);
    connect(_table_transform_widget, &TableTransformWidget::applyTransformClicked,
            this, &TableDesignerWidget::onApplyTransform);

    // Insert Export section
    _table_export_widget = new TableExportWidget(this);
    _table_export_section = new Section(this, "Export");
    _table_export_section->setContentLayout(*new QVBoxLayout());
    _table_export_section->layout()->addWidget(_table_export_widget);
    _table_export_section->autoSetContentLayout();
    ui->main_layout->insertWidget(ui->main_layout->indexOf(ui->build_group) + 2, _table_export_section);
    connect(_table_export_widget, &TableExportWidget::exportClicked,
            this, &TableDesignerWidget::onExportCsv);

    // Insert JSON section
    _table_json_widget = new TableJSONWidget(this);
    _table_json_section = new Section(this, "Table JSON Template");
    _table_json_section->setContentLayout(*new QVBoxLayout());
    _table_json_section->layout()->addWidget(_table_json_widget);
    _table_json_section->autoSetContentLayout();
    ui->main_layout->insertWidget(ui->main_layout->indexOf(ui->build_group) + 3, _table_json_section);
    connect(_table_json_widget, &TableJSONWidget::updateRequested, this, [this](QString const & jsonText) {
        applyJsonTemplateToUI(jsonText);
    });

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
    connect(ui->group_mode_toggle_btn, &QPushButton::toggled,
            this, &TableDesignerWidget::onGroupModeToggled);

    // Build signals
    connect(ui->build_table_btn, &QPushButton::clicked,
            this, &TableDesignerWidget::onBuildTable);
    // Transform apply handled via TableTransformWidget
    // Export handled via TableExportWidget

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
    if (auto gb = this->findChild<QGroupBox *>("row_source_group")) gb->setEnabled(true);
    if (auto gb = this->findChild<QGroupBox *>("column_design_group")) gb->setEnabled(true);
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
            if (reg->storeBuiltTable(_current_table_id.toStdString(), std::make_unique<TableView>(std::move(table_view)))) {
                updateBuildStatus(QString("Table built successfully with %1 columns!").arg(column_infos.size()));
                qDebug() << "Successfully built table:" << _current_table_id << "with" << column_infos.size() << "columns";
                // Populate JSON widget with the current configuration
                setJsonTemplateFromCurrentState();
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
            if (reg->storeBuiltTable(_current_table_id.toStdString(), std::make_unique<TableView>(std::move(table_view)))) {
                updateBuildStatus(QString("Table built successfully with %1 columns!").arg(column_infos.size()));
                qDebug() << "Successfully built table:" << _current_table_id << "with" << column_infos.size() << "columns";
                // Populate JSON widget with the current configuration as well
                setJsonTemplateFromCurrentState();
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
    QString transform = _table_transform_widget ? _table_transform_widget->getTransformType() : QString();
    if (transform != "PCA") {
        updateBuildStatus("Unsupported transform", true);
        return;
    }

    // Configure PCA
    PCAConfig cfg;
    cfg.center = _table_transform_widget && _table_transform_widget->isCenterEnabled();
    cfg.standardize = _table_transform_widget && _table_transform_widget->isStandardizeEnabled();
    if (_table_transform_widget) {
        for (auto const & s: _table_transform_widget->getIncludeColumns()) cfg.include.push_back(s);
        for (auto const & s: _table_transform_widget->getExcludeColumns()) cfg.exclude.push_back(s);
    }

    try {
        PCATransform pca(cfg);
        TableView derived = pca.apply(*base_view);

        // Determine output id/name
        QString out_name = _table_transform_widget ? _table_transform_widget->getOutputName().trimmed() : QString();
        if (out_name.isEmpty()) {
            QString base = _table_info_widget ? _table_info_widget->getName() : QString();
            out_name = base.isEmpty() ? QString("(PCA)") : QString("%1 (PCA)").arg(base);
        }

        std::string out_id = reg->generateUniqueTableId((_current_table_id + "_pca").toStdString());
        if (!reg->createTable(out_id, out_name.toStdString())) {
            reg->updateTableInfo(out_id, out_name.toStdString(), "");
        }
        if (reg->storeBuiltTable(out_id, std::make_unique<TableView>(std::move(derived)))) {
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

    // CSV options from export widget
    QString delimiter = _table_export_widget ? _table_export_widget->getDelimiterText() : "Comma";
    QString lineEnding = _table_export_widget ? _table_export_widget->getLineEndingText() : "LF (\\n)";
    int precision = _table_export_widget ? _table_export_widget->getPrecision() : 3;
    bool includeHeader = _table_export_widget && _table_export_widget->isHeaderIncluded();
    bool exportByGroup = _table_export_widget && _table_export_widget->isExportByGroup();

    std::string delim = ",";
    if (delimiter == "Space") delim = " ";
    else if (delimiter == "Tab")
        delim = "\t";
    std::string eol = "\n";
    if (lineEnding.startsWith("CRLF")) eol = "\r\n";

    if (exportByGroup) {
        // Export separate files by group
        QString directory = promptSaveDirectoryForGroupExport();
        if (directory.isEmpty()) return;
        
        // Extract base name from current table ID
        QString base_name = _current_table_id;
        
        try {
            int files_exported = exportTableByGroups(view.get(), directory, base_name, delim, eol, precision, includeHeader);
            
            if (files_exported > 0) {
                updateBuildStatus(QString("Exported %1 CSV files to: %2").arg(files_exported).arg(directory));
            } else {
                // Error message already set by exportTableByGroups
                // updateBuildStatus("No groups found or export failed", true);
            }
        } catch (std::exception const & e) {
            updateBuildStatus(QString("Export by group failed: %1").arg(e.what()), true);
        }
    } else {
        // Export single file
        QString filename = promptSaveCsvFilename();
        if (filename.isEmpty()) return;
        if (!filename.endsWith(".csv", Qt::CaseInsensitive)) filename += ".csv";
        
        if (exportTableToSingleCsv(view.get(), filename, delim, eol, precision, includeHeader)) {
            updateBuildStatus(QString("Exported CSV: %1").arg(filename));
        } else {
            updateBuildStatus("Export failed", true);
        }
    }
}

QString TableDesignerWidget::promptSaveCsvFilename() const {
    return QFileDialog::getSaveFileName(const_cast<TableDesignerWidget *>(this), "Export Table to CSV", QString(), "CSV Files (*.csv)");
}

QString TableDesignerWidget::promptSaveDirectoryForGroupExport() const {
    return QFileDialog::getExistingDirectory(const_cast<TableDesignerWidget *>(this), "Select Directory for Group CSV Export");
}

bool TableDesignerWidget::exportTableToSingleCsv(TableView * view, QString const & filename,
                                                 std::string const & delim, std::string const & eol,
                                                 int precision, bool includeHeader) {
    try {
        std::ofstream file(filename.toStdString());
        if (!file.is_open()) {
            return false;
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

                // Use efficient visitColumnData approach instead of try/catch
                try {
                    view->visitColumnData(names[c], [&file, r, precision, this](auto const & vec) {
                        using VecT = std::decay_t<decltype(vec)>;
                        using ElemT = typename VecT::value_type;

                        if (r >= vec.size()) {
                            if constexpr (std::is_same_v<ElemT, double>) file << "NaN";
                            else if constexpr (std::is_same_v<ElemT, float>)
                                file << "NaN";
                            else if constexpr (std::is_same_v<ElemT, int>)
                                file << "NaN";
                            else if constexpr (std::is_same_v<ElemT, int64_t>)
                                file << "NaN";
                            else if constexpr (std::is_same_v<ElemT, bool>)
                                file << "false";
                            else
                                file << "N/A";
                            return;
                        }

                        if constexpr (std::is_same_v<ElemT, double>) {
                            file << std::fixed << std::setprecision(precision) << vec[r];
                        } else if constexpr (std::is_same_v<ElemT, float>) {
                            file << std::fixed << std::setprecision(precision) << vec[r];
                        } else if constexpr (std::is_same_v<ElemT, int>) {
                            file << vec[r];
                        } else if constexpr (std::is_same_v<ElemT, int64_t>) {
                            file << static_cast<int64_t>(vec[r]);
                        } else if constexpr (std::is_same_v<ElemT, bool>) {
                            file << (vec[r] ? 1 : 0);
                        } else if constexpr (
                                std::is_same_v<ElemT, std::vector<double>> ||
                                std::is_same_v<ElemT, std::vector<int>> ||
                                std::is_same_v<ElemT, std::vector<float>>) {
                            // Format vector data as comma-separated values
                            formatVectorForCsv(file, vec[r], precision);
                        } else {
                            file << "?";
                        }
                    });
                } catch (...) {
                    file << "Error";
                }
            }
            file << eol;
        }
        file.close();
        return true;
    } catch (std::exception const & e) {
        qWarning() << "Export failed:" << e.what();
        return false;
    }
}

int TableDesignerWidget::exportTableByGroups(TableView * view, QString const & directory,
                                            QString const & base_name,
                                            std::string const & delim, std::string const & eol,
                                            int precision, bool includeHeader) {
    if (!view || !_data_manager) {
        return 0;
    }

    // Get the entity group manager
    auto * group_manager = _data_manager->getEntityGroupManager();
    if (!group_manager) {
        qWarning() << "EntityGroupManager not available";
        return 0;
    }

    // Get all groups
    auto group_descriptors = group_manager->getAllGroupDescriptors();
    if (group_descriptors.empty()) {
        qWarning() << "No entity groups defined";
        updateBuildStatus("Cannot export by group: No entity groups defined", true);
        return 0;
    }

    // Materialize all columns first - this ensures entity IDs are computed and stored
    try {
        view->materializeAll();
    } catch (std::exception const & e) {
        qWarning() << "Failed to materialize table:" << e.what();
        updateBuildStatus(QString("Cannot export by group: Failed to materialize table: %1").arg(e.what()), true);
        return 0;
    }

    // Check if table has entity information
    if (!view->hasEntityColumn()) {
        qWarning() << "Table does not have entity information";
        updateBuildStatus("Cannot export by group: Table does not have entity information", true);
        return 0;
    }

    // Get all entity IDs for all rows
    auto all_entity_ids = view->getEntityIds();
    if (all_entity_ids.empty() || all_entity_ids.size() != view->getRowCount()) {
        qWarning() << "Entity IDs not available or mismatch with row count";
        updateBuildStatus("Cannot export by group: Entity IDs incomplete or unavailable", true);
        return 0;
    }

    int files_exported = 0;

    // For each group, find matching rows and export
    for (auto const & group_desc : group_descriptors) {
        // Get entities in this group
        auto group_entities = group_manager->getEntitiesInGroup(group_desc.id);
        if (group_entities.empty()) {
            continue;
        }

        // Convert to unordered_set for fast lookup
        std::unordered_set<EntityId> group_entity_set(group_entities.begin(), group_entities.end());

        // Find rows that have any entity in this group
        std::vector<size_t> matching_rows;
        for (size_t r = 0; r < all_entity_ids.size(); ++r) {
            // Check if any of the row's entities are in this group
            bool has_match = false;
            for (auto const & entity_id : all_entity_ids[r]) {
                if (group_entity_set.count(entity_id) > 0) {
                    has_match = true;
                    break;
                }
            }
            if (has_match) {
                matching_rows.push_back(r);
            }
        }

        // Skip if no matching rows
        if (matching_rows.empty()) {
            continue;
        }

        // Create filename for this group
        QString group_name = QString::fromStdString(group_desc.name);
        // Sanitize group name for filename (replace invalid characters)
        group_name = group_name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString filename = QString("%1/%2_%3.csv").arg(directory).arg(base_name).arg(group_name);

        // Export this group to file
        try {
            std::ofstream file(filename.toStdString());
            if (!file.is_open()) {
                qWarning() << "Could not open file:" << filename;
                continue;
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

            // Write only matching rows
            for (size_t r : matching_rows) {
                for (size_t c = 0; c < names.size(); ++c) {
                    if (c > 0) file << delim;

                    try {
                        view->visitColumnData(names[c], [&file, r, precision, this](auto const & vec) {
                            using VecT = std::decay_t<decltype(vec)>;
                            using ElemT = typename VecT::value_type;

                            if (r >= vec.size()) {
                                if constexpr (std::is_same_v<ElemT, double>) file << "NaN";
                                else if constexpr (std::is_same_v<ElemT, float>)
                                    file << "NaN";
                                else if constexpr (std::is_same_v<ElemT, int>)
                                    file << "NaN";
                                else if constexpr (std::is_same_v<ElemT, int64_t>)
                                    file << "NaN";
                                else if constexpr (std::is_same_v<ElemT, bool>)
                                    file << "false";
                                else
                                    file << "N/A";
                                return;
                            }

                            if constexpr (std::is_same_v<ElemT, double>) {
                                file << std::fixed << std::setprecision(precision) << vec[r];
                            } else if constexpr (std::is_same_v<ElemT, float>) {
                                file << std::fixed << std::setprecision(precision) << vec[r];
                            } else if constexpr (std::is_same_v<ElemT, int>) {
                                file << vec[r];
                            } else if constexpr (std::is_same_v<ElemT, int64_t>) {
                                file << static_cast<int64_t>(vec[r]);
                            } else if constexpr (std::is_same_v<ElemT, bool>) {
                                file << (vec[r] ? 1 : 0);
                            } else if constexpr (
                                    std::is_same_v<ElemT, std::vector<double>> ||
                                    std::is_same_v<ElemT, std::vector<int>> ||
                                    std::is_same_v<ElemT, std::vector<float>>) {
                                formatVectorForCsv(file, vec[r], precision);
                            } else {
                                file << "?";
                            }
                        });
                    } catch (...) {
                        file << "Error";
                    }
                }
                file << eol;
            }
            
            file.close();
            files_exported++;
            
        } catch (std::exception const & e) {
            qWarning() << "Failed to export group" << group_name << ":" << e.what();
        }
    }

    return files_exported;
}

template<typename T>
void TableDesignerWidget::formatVectorForCsv(std::ofstream & file, std::vector<T> const & values, int precision) {
    if (values.empty()) {
        file << "[]";
        return;
    }

    file << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            file << std::fixed << std::setprecision(precision) << values[i];
        } else {
            file << values[i];
        }
        if (i + 1 < values.size()) file << ",";
    }
    file << "]";
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
    if (auto gb = this->findChild<QGroupBox *>("row_source_group")) gb->setEnabled(false);
    if (auto gb = this->findChild<QGroupBox *>("column_design_group")) gb->setEnabled(false);
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

    // Add LineData keys as data sources (for computers; not row selectors)
    auto line_keys = _data_manager->getKeys<LineData>();
    qDebug() << "getAvailableDataSources: Line keys:" << line_keys.size();
    for (auto const & key: line_keys) {
        QString source = QString("lines:%1").arg(QString::fromStdString(key));
        sources << source;
        qDebug() << "  Added Lines:" << source;
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
    } else if (data_source_string.startsWith("lines:")) {
        QString source_name = data_source_string.mid(6);// Remove "lines:" prefix
        row_selector_type = RowSelectorType::Timestamp; // LineData computers work with timestamp row selectors

        if (auto line_source = data_manager_extension->getLineSource(source_name.toStdString())) {
            result = line_source;
        }
    }

    return {result, row_selector_type};
}

std::optional<DataSourceVariant>
TableDesignerWidget::createColumnDataSourceVariant(QString const & data_source_string,
                                                   std::shared_ptr<DataManagerExtension> data_manager_extension) const {
    std::optional<DataSourceVariant> result;

    if (data_source_string.startsWith("Events: ")) {
        QString source_name = data_source_string.mid(8);// Remove "Events: " prefix
        if (auto event_source = data_manager_extension->getEventSource(source_name.toStdString())) {
            result = event_source;
        }

    } else if (data_source_string.startsWith("Intervals: ")) {
        QString source_name = data_source_string.mid(11);// Remove "Intervals: " prefix
        if (auto interval_source = data_manager_extension->getIntervalSource(source_name.toStdString())) {
            result = interval_source;
        }

    } else if (data_source_string.startsWith("analog:")) {
        QString source_name = data_source_string.mid(7);// Remove "analog:" prefix
        if (auto analog_source = data_manager_extension->getAnalogSource(source_name.toStdString())) {
            result = analog_source;
        }

    } else if (data_source_string.startsWith("lines:")) {
        QString source_name = data_source_string.mid(6);// Remove "lines:" prefix
        if (auto line_source = data_manager_extension->getLineSource(source_name.toStdString())) {
            result = line_source;
        }
    }

    return result;
}

std::optional<RowSelectorType> TableDesignerWidget::getCurrentRowSelectorType() const {
    QString row_source = ui->row_data_source_combo->currentText();
    
    if (row_source.isEmpty()) {
        return std::nullopt;
    }

    if (row_source.startsWith("TimeFrame: ")) {
        return RowSelectorType::Timestamp;
    } else if (row_source.startsWith("Events: ")) {
        return RowSelectorType::Timestamp;  // Events create timestamp rows
    } else if (row_source.startsWith("Intervals: ")) {
        return RowSelectorType::IntervalBased;
    }

    return std::nullopt;
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
        for (auto const & name: desiredOrder) {
            auto it = std::find_if(column_infos.begin(), column_infos.end(), [&](ColumnInfo const & ci) { return QString::fromStdString(ci.name) == name; });
            if (it != column_infos.end()) {
                reordered.push_back(*it);
            }
        }
        for (auto const & ci: column_infos) {
            if (std::find_if(reordered.begin(), reordered.end(), [&](ColumnInfo const & x) { return x.name == ci.name; }) == reordered.end()) {
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
            QString("Preview: %1").arg(_current_table_id),
            row_source);

    // Capture the current visual order from the viewer
    QStringList currentOrder;
    if (_table_viewer) {
        auto * tv = _table_viewer->findChild<QTableView *>();
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
            auto * top_level_item = ui->computers_tree->topLevelItem(i);
            // Handle both grouped and individual modes
            for (int j = 0; j < top_level_item->childCount(); ++j) {
                auto * child_item = top_level_item->child(j);
                if (child_item->childCount() > 0) {
                    // This is a computer item under a group/data source
                    for (int k = 0; k < child_item->childCount(); ++k) {
                        auto * computer_item = child_item->child(k);
                        QString ds = computer_item->data(0, Qt::UserRole).toString();
                        QString cn = computer_item->data(1, Qt::UserRole).toString();
                        std::string key = (ds + "||" + cn).toStdString();
                        previous_states[key] = {computer_item->checkState(1), computer_item->text(2)};
                    }
                } else {
                    // This is a computer item directly under data source (individual mode)
                    QString ds = child_item->data(0, Qt::UserRole).toString();
                    QString cn = child_item->data(1, Qt::UserRole).toString();
                    std::string key = (ds + "||" + cn).toStdString();
                    previous_states[key] = {child_item->checkState(1), child_item->text(2)};
                }
            }
        }
    }

    ui->computers_tree->clear();
    ui->computers_tree->setHeaderLabels({"Data Source / Computer", "Enabled", "Column Name", "Parameters"});

    // Clean up parameter widgets
    _computer_parameter_widgets.clear();
    _parameter_controls.clear();

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

    // Get the current row selector type from the selected row source
    auto current_row_selector_type = getCurrentRowSelectorType();
    if (!current_row_selector_type.has_value()) {
        // No row source selected, clear the tree and exit
        _updating_computers_tree = false;
        return;
    }

    // Get available data sources
    auto data_sources = getAvailableDataSources();

    if (_group_mode) {
        // Group similar data sources together
        std::map<std::string, QStringList> groups;

        // First pass: group data sources by their extracted group name
        for (QString const & data_source: data_sources) {
            std::string group_name = extractGroupName(data_source);
            groups[group_name].append(data_source);
        }

        // Second pass: create tree structure
        for (auto const & [group_name, group_members]: groups) {
            if (group_members.size() > 1) {
                // Create group item
                auto * group_item = new QTreeWidgetItem(ui->computers_tree);
                group_item->setText(0, QString::fromStdString(group_name) + " (Group)");
                group_item->setFlags(Qt::ItemIsEnabled);
                group_item->setExpanded(false);// Start collapsed

                // Get computers available for this group (use first member to determine available computers)
                auto first_variant = createColumnDataSourceVariant(group_members.first(), data_manager_extension);
                if (!first_variant.has_value()) {
                    continue;
                }

                auto available_computers = computer_registry.getAvailableComputers(current_row_selector_type.value(), first_variant.value());

                // Add computers as children of the group
                for (auto const & computer_info: available_computers) {
                    auto * computer_item = new QTreeWidgetItem(group_item);
                    computer_item->setText(0, QString::fromStdString(computer_info.name));
                    computer_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                    computer_item->setCheckState(1, Qt::Unchecked);

                    // Generate default column name using group name
                    QString default_name = QString("%1_%2").arg(QString::fromStdString(group_name), QString::fromStdString(computer_info.name));
                    computer_item->setText(2, default_name);
                    computer_item->setFlags(computer_item->flags() | Qt::ItemIsEditable);

                    // Store group members and computer name for later use
                    computer_item->setData(0, Qt::UserRole, group_members.join("||"));// Store all group members
                    computer_item->setData(1, Qt::UserRole, QString::fromStdString(computer_info.name));
                    computer_item->setData(2, Qt::UserRole, true);// Mark as group computer

                    // Create parameter widget if computer has parameters
                    if (!computer_info.parameterDescriptors.empty()) {
                        auto * param_widget = createParameterWidget(QString::fromStdString(computer_info.name),
                                                                    computer_info.parameterDescriptors);
                        if (param_widget) {
                            ui->computers_tree->setItemWidget(computer_item, 3, param_widget);
                            _computer_parameter_widgets[computer_item] = param_widget;
                        }
                    }

                    // Restore previous state if present (use first member for key)
                    std::string prev_key = (group_members.first() + "||" + QString::fromStdString(computer_info.name)).toStdString();
                    auto it_prev = previous_states.find(prev_key);
                    if (it_prev != previous_states.end()) {
                        computer_item->setCheckState(1, it_prev->second.first);
                        if (!it_prev->second.second.isEmpty()) {
                            computer_item->setText(2, it_prev->second.second);
                        }
                    }
                }
            } else {
                // Single item - create as individual data source
                QString const & data_source = group_members.first();
                auto * data_source_item = new QTreeWidgetItem(ui->computers_tree);
                data_source_item->setText(0, data_source);
                data_source_item->setFlags(Qt::ItemIsEnabled);
                data_source_item->setExpanded(false);

                auto data_source_variant = createColumnDataSourceVariant(data_source, data_manager_extension);
                if (!data_source_variant.has_value()) {
                    continue;
                }

                auto available_computers = computer_registry.getAvailableComputers(current_row_selector_type.value(), data_source_variant.value());

                for (auto const & computer_info: available_computers) {
                    auto * computer_item = new QTreeWidgetItem(data_source_item);
                    computer_item->setText(0, QString::fromStdString(computer_info.name));
                    computer_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                    computer_item->setCheckState(1, Qt::Unchecked);

                    QString default_name = generateDefaultColumnName(data_source, QString::fromStdString(computer_info.name));
                    computer_item->setText(2, default_name);
                    computer_item->setFlags(computer_item->flags() | Qt::ItemIsEditable);

                    computer_item->setData(0, Qt::UserRole, data_source);
                    computer_item->setData(1, Qt::UserRole, QString::fromStdString(computer_info.name));
                    computer_item->setData(2, Qt::UserRole, false);// Mark as individual computer

                    // Create parameter widget if computer has parameters
                    if (!computer_info.parameterDescriptors.empty()) {
                        auto * param_widget = createParameterWidget(QString::fromStdString(computer_info.name),
                                                                    computer_info.parameterDescriptors);
                        if (param_widget) {
                            ui->computers_tree->setItemWidget(computer_item, 3, param_widget);
                            _computer_parameter_widgets[computer_item] = param_widget;
                        }
                    }

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
        }
    } else {
        // Individual mode - create tree structure: Data Source -> Computers (original behavior)
        for (QString const & data_source: data_sources) {
            auto * data_source_item = new QTreeWidgetItem(ui->computers_tree);
            data_source_item->setText(0, data_source);
            data_source_item->setFlags(Qt::ItemIsEnabled);
            data_source_item->setExpanded(false);// Start collapsed

            // Convert data source string to DataSourceVariant
            auto data_source_variant = createColumnDataSourceVariant(data_source, data_manager_extension);

            if (!data_source_variant.has_value()) {
                qDebug() << "Failed to create data source variant for:" << data_source;
                continue;
            }

            // Get available computers for this specific data source and current row selector type
            auto available_computers = computer_registry.getAvailableComputers(current_row_selector_type.value(), data_source_variant.value());

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
                computer_item->setData(2, Qt::UserRole, false);// Mark as individual computer

                // Create parameter widget if computer has parameters
                if (!computer_info.parameterDescriptors.empty()) {
                    auto * param_widget = createParameterWidget(QString::fromStdString(computer_info.name),
                                                                computer_info.parameterDescriptors);
                    if (param_widget) {
                        ui->computers_tree->setItemWidget(computer_item, 3, param_widget);
                        _computer_parameter_widgets[computer_item] = param_widget;
                    }
                }

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
    }

    // Resize columns to content
    ui->computers_tree->resizeColumnToContents(0);
    ui->computers_tree->resizeColumnToContents(1);
    ui->computers_tree->resizeColumnToContents(2);
    ui->computers_tree->resizeColumnToContents(3);

    _updating_computers_tree = false;

    // Update preview after refresh
    triggerPreviewDebounced();
}

void TableDesignerWidget::setJsonTemplateFromCurrentState() {
    if (!_table_json_widget) return;
    // Build a minimal JSON template representing current UI state
    QString row_source = ui->row_data_source_combo ? ui->row_data_source_combo->currentText() : QString();
    auto columns = getEnabledColumnInfos();
    if (row_source.isEmpty() && columns.empty()) {
        _table_json_widget->setJsonText("{}");
        return;
    }

    QString row_type;
    QString row_source_name;
    if (row_source.startsWith("TimeFrame: ")) {
        row_type = "timestamp";
        row_source_name = row_source.mid(11);
    } else if (row_source.startsWith("Events: ")) {
        row_type = "timestamp";
        row_source_name = row_source.mid(8);
    } else if (row_source.startsWith("Intervals: ")) {
        row_type = "interval";
        row_source_name = row_source.mid(11);
    }

    QStringList column_entries;
    for (auto const & c: columns) {
        // Strip any internal prefixes for JSON to keep schema user-friendly
        QString ds = QString::fromStdString(c.dataSourceName);
        if (ds.startsWith("events:")) ds = ds.mid(7);
        else if (ds.startsWith("intervals:"))
            ds = ds.mid(10);
        else if (ds.startsWith("analog:"))
            ds = ds.mid(7);

        QString entry = QString(
                                "{\n  \"name\": \"%1\",\n  \"description\": \"%2\",\n  \"data_source\": \"%3\",\n  \"computer\": \"%4\"%5\n}")
                                .arg(QString::fromStdString(c.name))
                                .arg(QString::fromStdString(c.description))
                                .arg(ds)
                                .arg(QString::fromStdString(c.computerName))
                                .arg(c.parameters.empty() ? QString() : QString(",\n  \"parameters\": {}"));
        column_entries << entry;
    }

    QString table_name = _table_info_widget ? _table_info_widget->getName() : _current_table_id;
    QString json = QString(
                           "{\n  \"tables\": [\n    {\n      \"table_id\": \"%1\",\n      \"name\": \"%2\",\n      \"row_selector\": { \"type\": \"%3\", \"source\": \"%4\" },\n      \"columns\": [\n%5\n      ]\n    }\n  ]\n}")
                           .arg(_current_table_id)
                           .arg(table_name)
                           .arg(row_type)
                           .arg(row_source_name)
                           .arg(column_entries.join(",\n"));

    _table_json_widget->setJsonText(json);
}

void TableDesignerWidget::applyJsonTemplateToUI(QString const & jsonText) {
    // Very light-weight parser using Qt to extract essential fields.
    // Assumes a schema similar to tests under computers *.test.cpp.
    QJsonParseError err;
    QByteArray bytes = jsonText.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // Compute line/column from byte offset if possible
        int64_t offset = static_cast<int64_t>(err.offset);
        int line = 1;
        int col = 1;
        // Avoid operator[] ambiguity on some compilers by using qsizetype and at()
        qsizetype len = std::min<qsizetype>(bytes.size(), static_cast<qsizetype>(offset));
        for (qsizetype i = 0; i < len; ++i) {
            char ch = bytes.at(i);
            if (ch == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        QString detail = err.error != QJsonParseError::NoError
                                 ? QString("%1 (line %2, column %3)").arg(err.errorString()).arg(line).arg(col)
                                 : QString("JSON root must be an object");
        auto * box = new QMessageBox(this);
        box->setIcon(QMessageBox::Critical);
        box->setWindowTitle("Invalid JSON");
        box->setText(QString("JSON format is invalid: %1").arg(detail));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();
        return;
    }
    auto obj = doc.object();
    if (!obj.contains("tables") || !obj["tables"].isArray()) {
        auto * box = new QMessageBox(this);
        box->setIcon(QMessageBox::Critical);
        box->setWindowTitle("Invalid JSON");
        box->setText("Missing required key: tables (array)");
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();
        return;
    }
    auto tables = obj["tables"].toArray();
    if (tables.isEmpty() || !tables[0].isObject()) return;
    auto table = tables[0].toObject();

    // Row selector
    QStringList errors;
    QString rs_type;
    QString rs_source;
    if (table.contains("row_selector") && table["row_selector"].isObject()) {
        auto rs = table["row_selector"].toObject();
        rs_type = rs.value("type").toString();
        rs_source = rs.value("source").toString();
        if (rs_type.isEmpty() || rs_source.isEmpty()) {
            errors << "Missing required keys in row_selector: 'type' and/or 'source'";
        } else {
            // Validate existence
            bool source_ok = false;
            if (rs_type == "interval") {
                source_ok = (_data_manager && _data_manager->getData<DigitalIntervalSeries>(rs_source.toStdString()) != nullptr);
            } else if (rs_type == "timestamp") {
                source_ok = (_data_manager && (_data_manager->getTime(TimeKey(rs_source.toStdString())) != nullptr ||
                                               _data_manager->getData<DigitalEventSeries>(rs_source.toStdString()) != nullptr));
            } else {
                errors << QString("Unsupported row_selector type: %1").arg(rs_type);
            }
            if (!source_ok) {
                errors << QString("Row selector data key not found in DataManager: %1").arg(rs_source);
            } else {
                // Apply selection to UI
                QString entry;
                if (rs_type == "interval") {
                    entry = QString("Intervals: %1").arg(rs_source);
                } else if (rs_type == "timestamp") {
                    // Prefer TimeFrame, fallback to Events
                    entry = QString("TimeFrame: %1").arg(rs_source);
                    int idx_tf = ui->row_data_source_combo->findText(entry);
                    if (idx_tf < 0) entry = QString("Events: %1").arg(rs_source);
                }
                int idx = ui->row_data_source_combo->findText(entry);
                if (idx >= 0) {
                    ui->row_data_source_combo->setCurrentIndex(idx);
                    // Ensure computers tree reflects this row selector before enabling columns
                    refreshComputersTree();
                } else {
                    errors << QString("Row selector entry not available in UI: %1").arg(entry);
                }
            }
        }
    } else {
        errors << "Missing required key: row_selector (object)";
    }

    // Columns: enable matching computers and set column names
    if (table.contains("columns") && table["columns"].isArray()) {
        auto cols = table["columns"].toArray();
        auto * tree = ui->computers_tree;
        // Avoid recursive preview rebuilds while we toggle many items
        bool prevBlocked = tree->blockSignals(true);
        for (auto const & cval: cols) {
            if (!cval.isObject()) continue;
            auto cobj = cval.toObject();
            QString data_source = cobj.value("data_source").toString();
            QString computer = cobj.value("computer").toString();
            QString name = cobj.value("name").toString();
            if (data_source.isEmpty() || computer.isEmpty() || name.isEmpty()) {
                errors << "Missing required keys in column: 'name', 'data_source', and 'computer'";
                continue;
            }
            // Validate data source existence
            bool has_ds = (_data_manager && (_data_manager->getData<DigitalEventSeries>(data_source.toStdString()) != nullptr ||
                                             _data_manager->getData<DigitalIntervalSeries>(data_source.toStdString()) != nullptr ||
                                             _data_manager->getData<AnalogTimeSeries>(data_source.toStdString()) != nullptr));
            if (!has_ds) {
                errors << QString("Data key not found in DataManager: %1").arg(data_source);
            }
            // Validate computer exists
            bool computer_exists = false;
            if (_data_manager) {
                if (auto * reg = _data_manager->getTableRegistry()) {
                    auto & cr = reg->getComputerRegistry();
                    computer_exists = cr.findComputerInfo(computer.toStdString());
                }
            }
            if (!computer_exists) {
                errors << QString("Requested computer does not exist: %1").arg(computer);
            }
            
            // Validate compatibility using registry and current row selector type
            if (_data_manager) {
                if (auto * reg = _data_manager->getTableRegistry()) {
                    auto data_manager_ext = reg->getDataManagerExtension();
                    if (data_manager_ext) {
                        auto & cr = reg->getComputerRegistry();
                        auto current_row_type = getCurrentRowSelectorType();
                        
                        // Build data source variant based on data type
                        bool type_event = false, type_interval = false, type_analog = false;
                        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                            auto * ds_item = tree->topLevelItem(i);
                            QString ds_text = ds_item->text(0);
                            if (ds_text.contains(data_source)) {
                                if (ds_text.startsWith("Events: ")) type_event = true;
                                else if (ds_text.startsWith("Intervals: ")) type_interval = true;
                                else if (ds_text.startsWith("analog:")) type_analog = true;
                            }
                        }
                        
                        QString ds_repr;
                        if (type_event) ds_repr = QString("Events: %1").arg(data_source);
                        else if (type_interval) ds_repr = QString("Intervals: %1").arg(data_source);
                        else if (type_analog) ds_repr = QString("analog:%1").arg(data_source);
                        
                        if (!ds_repr.isEmpty() && current_row_type.has_value()) {
                            auto ds_variant = createColumnDataSourceVariant(ds_repr, data_manager_ext);
                            if (ds_variant.has_value()) {
                                auto available = cr.getAvailableComputers(current_row_type.value(), ds_variant.value());
                                bool found = false;
                                for (auto const & comp_info : available) {
                                    if (comp_info.name == computer.toStdString()) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    errors << QString("Computer '%1' is not compatible with data source '%2' for the current row selector type")
                                                  .arg(computer, ds_repr);
                                }
                            }
                        }
                    }
                }
            }

            // Find matching tree item with strict preference
            QString exact_events = QString("Events: %1").arg(data_source);
            QString exact_intervals = QString("Intervals: %1").arg(data_source);
            QString exact_analog = QString("analog:%1").arg(data_source);
            QTreeWidgetItem * matched_ds = nullptr;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto * ds_item = tree->topLevelItem(i);
                QString t = ds_item->text(0);
                if (t == exact_events || t == exact_intervals || t == exact_analog) {
                    matched_ds = ds_item;
                    break;
                }
            }
            if (!matched_ds) {
                for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                    auto * ds_item = tree->topLevelItem(i);
                    QString t = ds_item->text(0);
                    if (t.contains(data_source) || t.endsWith(data_source)) {
                        matched_ds = ds_item;
                        break;
                    }
                }
            }
            if (matched_ds) {
                for (int j = 0; j < matched_ds->childCount(); ++j) {
                    auto * comp_item = matched_ds->child(j);
                    QString comp_text = comp_item->text(0).trimmed();
                    if (comp_text == computer || comp_text.contains(computer)) {
                        comp_item->setCheckState(1, Qt::Checked);
                        if (!name.isEmpty()) comp_item->setText(2, name);
                    }
                }
            } else {
                errors << QString("Data source not found in tree: %1").arg(data_source);
            }
        }
        tree->blockSignals(prevBlocked);
        if (!errors.isEmpty()) {
            auto * box = new QMessageBox(this);
            box->setIcon(QMessageBox::Critical);
            box->setWindowTitle("Invalid Table JSON");
            box->setText(errors.join("\n"));
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->show();
            return;
        }
        triggerPreviewDebounced();
    }
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
                bool is_group_computer = computer_item->data(2, Qt::UserRole).toBool();

                // Get parameter values for this computer
                std::map<std::string, std::string> parameters = getParameterValues(computer_name);

                if (is_group_computer) {
                    // This is a group computer - create columns for all members
                    QStringList group_members = data_source.split("||");

                    for (QString const & member: group_members) {
                        // Generate individual column name (e.g., "spike_1_Mean", "spike_2_Mean")
                        QString individual_column_name = generateDefaultColumnName(member, computer_name);

                        // Create ColumnInfo for each group member
                        QString source_key = member;
                        if (source_key.startsWith("Events: ")) {
                            source_key = QString("events:%1").arg(source_key.mid(8));
                        } else if (source_key.startsWith("Intervals: ")) {
                            source_key = QString("intervals:%1").arg(source_key.mid(11));
                        } else if (source_key.startsWith("analog:")) {
                            source_key = source_key;// already prefixed
                        } else if (source_key.startsWith("lines:")) {
                            source_key = source_key;// already prefixed
                        } else if (source_key.startsWith("TimeFrame: ")) {
                            // TimeFrame used only for row selector; columns require concrete sources
                            source_key = source_key.mid(11);
                        }

                        ColumnInfo info(individual_column_name.toStdString(),
                                        QString("Column from %1 using %2 (group applied)").arg(member, computer_name).toStdString(),
                                        source_key.toStdString(),
                                        computer_name.toStdString());

                        // Set parameters
                        info.parameters = parameters;

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
                } else {
                    // Individual computer - original behavior
                    if (column_name.isEmpty()) {
                        // Clean the data source name before generating column name
                        QString clean_data_source = data_source;
                        if (clean_data_source.startsWith("lines:")) {
                            clean_data_source = clean_data_source.mid(6);// Remove "lines:" prefix
                        }
                        column_name = generateDefaultColumnName(clean_data_source, computer_name);
                    }

                    // Create ColumnInfo (use raw key without UI prefixes)
                    QString source_key = data_source;
                    if (source_key.startsWith("Events: ")) {
                        source_key = QString("events:%1").arg(source_key.mid(8));
                    } else if (source_key.startsWith("Intervals: ")) {
                        source_key = QString("intervals:%1").arg(source_key.mid(11));
                    } else if (source_key.startsWith("analog:")) {
                        source_key = source_key;// already prefixed
                    } else if (source_key.startsWith("lines:")) {
                        source_key = source_key;// already prefixed
                    } else if (source_key.startsWith("TimeFrame: ")) {
                        // TimeFrame used only for row selector; columns require concrete sources
                        source_key = source_key.mid(11);
                    }

                    ColumnInfo info(column_name.toStdString(),
                                    QString("Column from %1 using %2").arg(data_source, computer_name).toStdString(),
                                    source_key.toStdString(),
                                    computer_name.toStdString());

                    // Set parameters
                    info.parameters = parameters;

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
    }

    return column_infos;
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
    } else if (source_name.startsWith("lines:")) {
        source_name = source_name.mid(6);
    } else if (source_name.startsWith("TimeFrame: ")) {
        source_name = source_name.mid(11);
    }

    // Create a concise name
    return QString("%1_%2").arg(source_name, computer_name);
}

std::string TableDesignerWidget::extractGroupName(QString const & data_source) const {
    QString source_name = data_source;

    // Extract the actual name from prefixed data sources
    if (source_name.startsWith("Events: ")) {
        source_name = source_name.mid(8);
    } else if (source_name.startsWith("Intervals: ")) {
        source_name = source_name.mid(11);
    } else if (source_name.startsWith("analog:")) {
        source_name = source_name.mid(7);
    } else if (source_name.startsWith("lines:")) {
        source_name = source_name.mid(6);
    } else if (source_name.startsWith("TimeFrame: ")) {
        source_name = source_name.mid(11);
    }

    // Use the same grouping pattern as Feature_Tree_Widget
    std::regex const pattern{_grouping_pattern};
    std::smatch matches{};
    std::string key = source_name.toStdString();

    if (std::regex_search(key, matches, pattern) && matches.size() > 1) {
        return matches[1].str();
    }

    return key;// Return the key itself if no match
}

void TableDesignerWidget::onGroupModeToggled(bool enabled) {
    _group_mode = enabled;

    // Update button text to reflect current mode
    if (enabled) {
        ui->group_mode_toggle_btn->setText("Group Mode");
        ui->computers_info_label->setText("Select computers by checking the boxes. Similar data will be grouped and transformed together.");
    } else {
        ui->group_mode_toggle_btn->setText("Individual Mode");
        ui->computers_info_label->setText("Select computers by checking the boxes. Each data source will be handled individually.");
    }

    // Refresh the tree to apply the new grouping mode
    refreshComputersTree();
}

QWidget * TableDesignerWidget::createParameterWidget(QString const & computer_name,
                                                     std::vector<std::unique_ptr<IParameterDescriptor>> const & parameter_descriptors) {
    if (parameter_descriptors.empty()) {
        return nullptr;
    }

    auto * widget = new QWidget();
    auto * layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    for (auto const & param_desc: parameter_descriptors) {
        QString param_name = QString::fromStdString(param_desc->getName());
        QString param_key = computer_name + "::" + param_name;

        // Add parameter label
        auto * label = new QLabel(QString::fromStdString(param_desc->getName()) + ":");
        label->setToolTip(QString::fromStdString(param_desc->getDescription()));
        layout->addWidget(label);

        if (param_desc->getUIHint() == "enum") {
            // Create combo box for enum parameters
            auto * combo = new QComboBox();
            combo->setObjectName(param_key);// Store parameter key for retrieval

            auto ui_props = param_desc->getUIProperties();
            QString options_str = QString::fromStdString(ui_props["options"]);
            QString default_value = QString::fromStdString(ui_props["default"]);

            QStringList options = options_str.split(',', Qt::SkipEmptyParts);
            combo->addItems(options);

            // Set default value
            int default_index = combo->findText(default_value);
            if (default_index >= 0) {
                combo->setCurrentIndex(default_index);
            }

            combo->setToolTip(QString::fromStdString(param_desc->getDescription()));
            layout->addWidget(combo);

            // Store the widget for parameter retrieval
            _parameter_controls[param_key.toStdString()] = combo;

        } else if (param_desc->getUIHint() == "number") {
            // Create spin box for numeric parameters
            auto * spinbox = new QSpinBox();
            spinbox->setObjectName(param_key);

            auto ui_props = param_desc->getUIProperties();
            QString default_str = QString::fromStdString(ui_props["default"]);
            QString min_str = QString::fromStdString(ui_props["min"]);
            QString max_str = QString::fromStdString(ui_props["max"]);

            if (!min_str.isEmpty()) spinbox->setMinimum(min_str.toInt());
            if (!max_str.isEmpty()) spinbox->setMaximum(max_str.toInt());
            if (!default_str.isEmpty()) spinbox->setValue(default_str.toInt());

            spinbox->setToolTip(QString::fromStdString(param_desc->getDescription()));
            layout->addWidget(spinbox);

            _parameter_controls[param_key.toStdString()] = spinbox;

        } else {
            // Default to text input
            auto * lineedit = new QLineEdit();
            lineedit->setObjectName(param_key);

            auto ui_props = param_desc->getUIProperties();
            QString default_value = QString::fromStdString(ui_props["default"]);
            lineedit->setText(default_value);

            lineedit->setToolTip(QString::fromStdString(param_desc->getDescription()));
            layout->addWidget(lineedit);

            _parameter_controls[param_key.toStdString()] = lineedit;
        }
    }

    return widget;
}

std::map<std::string, std::string> TableDesignerWidget::getParameterValues(QString const & computer_name) const {
    std::map<std::string, std::string> parameters;

    // Look for parameter controls with this computer name prefix
    QString prefix = computer_name + "::";

    for (auto const & [key, widget]: _parameter_controls) {
        QString key_str = QString::fromStdString(key);
        if (key_str.startsWith(prefix)) {
            QString param_name = key_str.mid(prefix.length());

            if (auto * combo = qobject_cast<QComboBox *>(widget)) {
                parameters[param_name.toStdString()] = combo->currentText().toStdString();
            } else if (auto * spinbox = qobject_cast<QSpinBox *>(widget)) {
                parameters[param_name.toStdString()] = QString::number(spinbox->value()).toStdString();
            } else if (auto * lineedit = qobject_cast<QLineEdit *>(widget)) {
                parameters[param_name.toStdString()] = lineedit->text().toStdString();
            }
        }
    }

    return parameters;
}

// Explicit template instantiations for formatVectorForCsv
template void TableDesignerWidget::formatVectorForCsv<double>(std::ofstream & file, std::vector<double> const & values, int precision);
template void TableDesignerWidget::formatVectorForCsv<int>(std::ofstream & file, std::vector<int> const & values, int precision);
template void TableDesignerWidget::formatVectorForCsv<float>(std::ofstream & file, std::vector<float> const & values, int precision);
