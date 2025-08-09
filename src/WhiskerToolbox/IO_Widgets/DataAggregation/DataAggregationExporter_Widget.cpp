#include "DataAggregationExporter_Widget.hpp"
#include "ui_DataAggregationExporter_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/computers/IntervalPropertyComputer.h"
#include "DataManager/utils/TableView/computers/IntervalOverlapComputer.h"
#include "DataManager/utils/TableView/computers/AnalogSliceGathererComputer.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/interfaces/IColumnComputer.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QSpinBox>
#include <QCheckBox>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>

DataAggregationExporter_Widget::DataAggregationExporter_Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::DataAggregationExporter_Widget)
{
    ui->setupUi(this);
    _setupTables();
    
    // Connect signals
    connect(ui->interval_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataAggregationExporter_Widget::_populateAvailableDataTable);
    connect(ui->available_data_table, &QTableWidget::itemSelectionChanged,
            this, &DataAggregationExporter_Widget::_onAvailableDataSelectionChanged);
    connect(ui->transformation_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataAggregationExporter_Widget::_onTransformationChanged);
    connect(ui->add_export_button, &QPushButton::clicked,
            this, &DataAggregationExporter_Widget::_onAddExportButtonClicked);
    connect(ui->remove_export_button, &QPushButton::clicked,
            this, &DataAggregationExporter_Widget::_onRemoveExportButtonClicked);
    connect(ui->clear_export_button, &QPushButton::clicked,
            this, &DataAggregationExporter_Widget::_onClearExportButtonClicked);
    connect(ui->export_csv_button, &QPushButton::clicked,
            this, &DataAggregationExporter_Widget::_onExportCsvButtonClicked);
}

DataAggregationExporter_Widget::~DataAggregationExporter_Widget()
{
    delete ui;
}

void DataAggregationExporter_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = std::move(data_manager);
    
    // Create DataManagerExtension for TableView access
    if (_data_manager) {
        _data_manager_extension = std::make_shared<DataManagerExtension>(*_data_manager);
        
        // Add observer to DataManager for updates
        _data_manager->addObserver([this]() {
            _onDataManagerUpdated();
        });
        
        _onDataManagerUpdated();
    }
}

void DataAggregationExporter_Widget::_setupTables()
{
    // Setup available data table
    ui->available_data_table->setColumnCount(3);
    QStringList available_headers = {"Data Key", "Type", "Time Frame"};
    ui->available_data_table->setHorizontalHeaderLabels(available_headers);
    ui->available_data_table->horizontalHeader()->setStretchLastSection(true);
    
    // Setup export list table
    ui->export_list_table->setColumnCount(4);
    QStringList export_headers = {"Data Key", "Type", "Transformation", "Column Name"};
    ui->export_list_table->setHorizontalHeaderLabels(export_headers);
    ui->export_list_table->horizontalHeader()->setStretchLastSection(true);
}

void DataAggregationExporter_Widget::_onDataManagerUpdated()
{
    _populateIntervalSourceCombo();
    _populateAvailableDataTable();
}

void DataAggregationExporter_Widget::_populateIntervalSourceCombo()
{
    ui->interval_source_combo->clear();
    
    if (!_data_manager) {
        return;
    }
    
    // Get all DigitalIntervalSeries keys
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    for (const auto& key : interval_keys) {
        ui->interval_source_combo->addItem(QString::fromStdString(key));
    }
}

void DataAggregationExporter_Widget::_populateAvailableDataTable()
{
    ui->available_data_table->setRowCount(0);
    
    if (!_data_manager) {
        return;
    }
    
    // Get all keys and filter by supported types
    auto all_keys = _data_manager->getAllKeys();
    
    for (const auto& key : all_keys) {
        DM_DataType type = _data_manager->getType(key);
        
        // Only include supported types
        if (type != DM_DataType::Analog &&
            type != DM_DataType::Points && 
            type != DM_DataType::DigitalInterval) {
            continue;
        }
        
        int row = ui->available_data_table->rowCount();
        ui->available_data_table->insertRow(row);
        
        // Data Key
        ui->available_data_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));
        
        // Type
        std::string type_str = convert_data_type_to_string(type);
        ui->available_data_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(type_str)));
        
        // Time Frame
        std::string time_frame = _data_manager->getTimeKey(key).str();
        ui->available_data_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(time_frame)));
    }
}

void DataAggregationExporter_Widget::_onAvailableDataSelectionChanged()
{
    _populateTransformationCombo();
    _onTransformationChanged();
}

void DataAggregationExporter_Widget::_populateTransformationCombo()
{
    ui->transformation_combo->clear();
    
    std::string selected_key = _getSelectedDataKey();
    if (selected_key.empty()) {
        return;
    }
    
    DM_DataType data_type = _getSelectedDataType();
    std::string interval_source = _getSelectedIntervalSource();
    
    auto transformations = _getAvailableTransformations(data_type, selected_key, interval_source);
    
    for (const auto& transformation : transformations) {
        QString display_name = _getTransformationDisplayName(transformation);
        ui->transformation_combo->addItem(display_name, QString::fromStdString(transformation));
    }
}

void DataAggregationExporter_Widget::_onTransformationChanged()
{
    std::string selected_key = _getSelectedDataKey();
    if (selected_key.empty()) {
        ui->column_name_edit->clear();
        return;
    }
    
    QString transformation_data = ui->transformation_combo->currentData().toString();
    if (transformation_data.isEmpty()) {
        ui->column_name_edit->clear();
        return;
    }
    
    std::string transformation = transformation_data.toStdString();
    QString default_name = _generateDefaultColumnName(selected_key, transformation);
    ui->column_name_edit->setText(default_name);
}

void DataAggregationExporter_Widget::_onAddExportButtonClicked()
{
    std::string selected_key = _getSelectedDataKey();
    if (selected_key.empty()) {
        QMessageBox::warning(this, "No Selection", "Please select a data key from the available data table.");
        return;
    }
    
    if (ui->transformation_combo->currentIndex() < 0) {
        QMessageBox::warning(this, "No Transformation", "Please select a transformation.");
        return;
    }
    
    QString column_name = ui->column_name_edit->text().trimmed();
    if (column_name.isEmpty()) {
        QMessageBox::warning(this, "No Column Name", "Please enter a column name.");
        return;
    }
    
    // Check for duplicate column names
    for (const auto& column : _export_columns) {
        if (column.column_name == column_name.toStdString()) {
            QMessageBox::warning(this, "Duplicate Column Name", 
                                "Column name already exists. Please choose a different name.");
            return;
        }
    }
    
    // Create export column
    ExportColumn column;
    column.data_key = selected_key;
    column.data_type = _getSelectedDataType();
    column.transformation_type = ui->transformation_combo->currentData().toString().toStdString();
    column.column_name = column_name.toStdString();
    
    // For transformations that need reference data, store the data key
    if (column.transformation_type == "interval_id" || 
        column.transformation_type == "interval_count" ||
        column.transformation_type == "interval_id_start" ||
        column.transformation_type == "interval_id_end") {
        column.reference_data_key = selected_key;
    }
    
    _export_columns.push_back(column);
    _updateExportListTable();
}

void DataAggregationExporter_Widget::_onRemoveExportButtonClicked()
{
    int current_row = ui->export_list_table->currentRow();
    if (current_row >= 0 && current_row < static_cast<int>(_export_columns.size())) {
        _export_columns.erase(_export_columns.begin() + current_row);
        _updateExportListTable();
    }
}

void DataAggregationExporter_Widget::_onClearExportButtonClicked()
{
    _export_columns.clear();
    _updateExportListTable();
}

void DataAggregationExporter_Widget::_updateExportListTable()
{
    ui->export_list_table->setRowCount(static_cast<int>(_export_columns.size()));
    
    for (size_t i = 0; i < _export_columns.size(); ++i) {
        const auto& column = _export_columns[i];
        
        ui->export_list_table->setItem(static_cast<int>(i), 0, 
                                      new QTableWidgetItem(QString::fromStdString(column.data_key)));
        ui->export_list_table->setItem(static_cast<int>(i), 1, 
                                      new QTableWidgetItem(QString::fromStdString(convert_data_type_to_string(column.data_type))));
        ui->export_list_table->setItem(static_cast<int>(i), 2, 
                                      new QTableWidgetItem(_getTransformationDisplayName(column.transformation_type)));
        ui->export_list_table->setItem(static_cast<int>(i), 3, 
                                      new QTableWidgetItem(QString::fromStdString(column.column_name)));
    }
}

std::vector<std::string> DataAggregationExporter_Widget::_getAvailableTransformations(DM_DataType data_type, 
                                                                                    const std::string& selected_key,
                                                                                    const std::string& interval_source) const
{
    std::vector<std::string> transformations;
    
    switch (data_type) {
        case DM_DataType::Analog:
            transformations = {"mean", "min", "max", "std_dev"};
            break;
        case DM_DataType::Points:
            transformations = {"mean_x", "mean_y"};
            break;
        case DM_DataType::DigitalInterval:
            // Check if this is the same interval as the source or a different one
            if (selected_key == interval_source) {
                // Same interval: offer metadata transformations
                transformations = {"start", "end", "duration"};
            } else {
                // Different interval: offer relational transformations
                transformations = {"interval_count", "interval_id", "interval_id_start", "interval_id_end"};
            }
            break;
        default:
            break;
    }
    
    return transformations;
}

QString DataAggregationExporter_Widget::_getTransformationDisplayName(const std::string& transformation) const
{
    if (transformation == "mean") return "Mean";
    if (transformation == "min") return "Minimum";
    if (transformation == "max") return "Maximum";
    if (transformation == "std_dev") return "Standard Deviation";
    if (transformation == "mean_x") return "Point Mean X";
    if (transformation == "mean_y") return "Point Mean Y";
    if (transformation == "interval_count") return "Interval Count";
    if (transformation == "interval_id") return "Interval ID";
    if (transformation == "interval_id_start") return "Interval Start ID";
    if (transformation == "interval_id_end") return "Interval End ID";
    if (transformation == "start") return "Interval Start";
    if (transformation == "end") return "Interval End";
    if (transformation == "duration") return "Interval Duration";
    return "Unknown";
}

QString DataAggregationExporter_Widget::_generateDefaultColumnName(const std::string& data_key, const std::string& transformation) const
{
    QString base_name = QString::fromStdString(data_key);
    QString suffix;
    
    if (transformation == "mean") suffix = "_mean";
    else if (transformation == "min") suffix = "_min";
    else if (transformation == "max") suffix = "_max";
    else if (transformation == "std_dev") suffix = "_std";
    else if (transformation == "mean_x") suffix = "_x_mean";
    else if (transformation == "mean_y") suffix = "_y_mean";
    else if (transformation == "interval_count") suffix = "_count";
    else if (transformation == "interval_id") suffix = "_id";
    else if (transformation == "interval_id_start") suffix = "_id_start";
    else if (transformation == "interval_id_end") suffix = "_id_end";
    else if (transformation == "start") return "interval_start";
    else if (transformation == "end") return "interval_end";
    else if (transformation == "duration") return "interval_duration";
    else suffix = "_unknown";
    
    return base_name + suffix;
}

std::string DataAggregationExporter_Widget::_getSelectedDataKey() const
{
    int current_row = ui->available_data_table->currentRow();
    if (current_row >= 0) {
        QTableWidgetItem* item = ui->available_data_table->item(current_row, 0);
        if (item) {
            return item->text().toStdString();
        }
    }
    return "";
}

DM_DataType DataAggregationExporter_Widget::_getSelectedDataType() const
{
    std::string selected_key = _getSelectedDataKey();
    if (!selected_key.empty() && _data_manager) {
        return _data_manager->getType(selected_key);
    }
    return DM_DataType::Unknown;
}

std::string DataAggregationExporter_Widget::_getSelectedIntervalSource() const
{
    return ui->interval_source_combo->currentText().toStdString();
}

void DataAggregationExporter_Widget::_onExportCsvButtonClicked()
{
    if (_export_columns.empty()) {
        QMessageBox::warning(this, "No Export Columns", "Please add at least one column to export.");
        return;
    }
    
    std::string interval_source = _getSelectedIntervalSource();
    if (interval_source.empty()) {
        QMessageBox::warning(this, "No Interval Source", "Please select an interval source.");
        return;
    }
    
    QString filename = QFileDialog::getSaveFileName(this, "Export Aggregated Data", 
                                                   QString(), "CSV Files (*.csv)");
    if (!filename.isEmpty()) {
        // Ensure the filename has .csv extension
        if (!filename.endsWith(".csv", Qt::CaseInsensitive)) {
            filename += ".csv";
        }
        
        try {
            _exportToCsv(filename);
            QMessageBox::information(this, "Export Complete", 
                                   "Data exported successfully to " + filename);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Export Error", 
                                 QString("Failed to export data: %1").arg(e.what()));
        }
    }
}

void DataAggregationExporter_Widget::_exportToCsv(const QString& filename)
{
    if (!_data_manager || !_data_manager_extension) {
        throw std::runtime_error("No DataManager available");
    }
    
    // Build the TableView using the new system
    TableView table = _buildTableView();
    
    // Write results to CSV
    std::ofstream file(filename.toStdString());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename.toStdString());
    }
    
    std::string delimiter = _getDelimiter();
    std::string line_ending = _getLineEnding();
    int precision = _getPrecision();
    
    file << std::fixed << std::setprecision(precision);
    
    // Write header if requested
    if (_shouldIncludeHeader()) {
        for (size_t i = 0; i < _export_columns.size(); ++i) {
            if (i > 0) file << delimiter;
            file << _export_columns[i].column_name;
        }
        file << line_ending;
    }
    
    // Write data rows
    for (size_t row = 0; row < table.getRowCount(); ++row) {
        for (size_t col = 0; col < _export_columns.size(); ++col) {
            if (col > 0) file << delimiter;
            
            const auto& column_name = _export_columns[col].column_name;
            try {
                const auto& values = table.getColumnValues<double>(column_name);
                if (row < values.size()) {
                    file << values[row];
                } else {
                    file << "NaN";
                }
            } catch (const std::exception& e) {
                file << "NaN";
            }
        }
        file << line_ending;
    }
    
    file.close();
}

TableView DataAggregationExporter_Widget::_buildTableView() const
{
    if (!_data_manager_extension) {
        throw std::runtime_error("DataManagerExtension not available");
    }
    
    // Create TableView builder
    TableViewBuilder builder(_data_manager_extension);
    
    // Set row selector
    builder.setRowSelector(_createRowSelector());
    
    // Add columns
    _addColumnsToBuilder(builder);
    
    // Build and return the table
    return builder.build();
}

std::unique_ptr<IRowSelector> DataAggregationExporter_Widget::_createRowSelector() const
{
    std::string interval_source = _getSelectedIntervalSource();
    if (interval_source.empty()) {
        throw std::runtime_error("No interval source selected");
    }
    
    // Get the interval data from DataManager
    auto interval_data = _data_manager->getData<DigitalIntervalSeries>(interval_source);
    if (!interval_data) {
        throw std::runtime_error("Could not retrieve interval data for key: " + interval_source);
    }
    
    // Get the intervals and time frame
    const auto& intervals = interval_data->getDigitalIntervalSeries();
    auto interval_time_frame_key = _data_manager->getTimeKey(interval_source);
    auto time_frame = _data_manager->getTime(interval_time_frame_key);
    if (!time_frame) {
        throw std::runtime_error("Could not retrieve time frame for key: " + interval_source);
    }
    // Convert to TimeFrameIntervals
    std::vector<TimeFrameInterval> time_frame_intervals;
    time_frame_intervals.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        time_frame_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
    }
    
    // Create IntervalSelector
    return std::make_unique<IntervalSelector>(time_frame_intervals, time_frame);
}

void DataAggregationExporter_Widget::_addColumnsToBuilder(TableViewBuilder& builder) const
{
    for (const auto& column : _export_columns) {
        auto computer = _createComputer(column);
        if (computer) {
            builder.addColumn(column.column_name, std::move(computer));
        }
    }
}

std::unique_ptr<IColumnComputer<double>> DataAggregationExporter_Widget::_createComputer(const ExportColumn& column) const
{

    auto time_frame_key = _data_manager->getTimeKey(column.data_key);
    auto source_time_frame = _data_manager->getTime(time_frame_key);

    if (column.transformation_type == "mean" || 
        column.transformation_type == "min" || 
        column.transformation_type == "max" || 
        column.transformation_type == "std_dev") {
        
        // Get analog source
        auto source = _data_manager_extension->getAnalogSource(column.data_key);
        if (!source) {
            throw std::runtime_error("Could not get analog source for: " + column.data_key);
        }
        
        // Map transformation to ReductionType
        ReductionType reduction;
        if (column.transformation_type == "mean") reduction = ReductionType::Mean;
        else if (column.transformation_type == "min") reduction = ReductionType::Min;
        else if (column.transformation_type == "max") reduction = ReductionType::Max;
        else if (column.transformation_type == "std_dev") reduction = ReductionType::StdDev;
        else throw std::runtime_error("Unknown reduction type: " + column.transformation_type);
        
        return std::make_unique<IntervalReductionComputer>(source, reduction, column.data_key);
    }
    else if (column.transformation_type == "mean_x" || column.transformation_type == "mean_y") {
        // For point data, we need to get the component source
        std::string component_key = column.data_key + "." + (column.transformation_type == "mean_x" ? "x" : "y");
        auto source = _data_manager_extension->getAnalogSource(component_key);
        if (!source) {
            throw std::runtime_error("Could not get point component source for: " + component_key);
        }
        
        return std::make_unique<IntervalReductionComputer>(source, ReductionType::Mean, component_key);
    }
    else if (column.transformation_type == "start" || 
             column.transformation_type == "end" || 
             column.transformation_type == "duration") {
        
        // Get interval source
        auto source = _data_manager_extension->getIntervalSource(column.data_key);
        if (!source) {
            throw std::runtime_error("Could not get interval source for: " + column.data_key);
        }
        
        // Map transformation to IntervalProperty
        IntervalProperty property;
        if (column.transformation_type == "start") property = IntervalProperty::Start;
        else if (column.transformation_type == "end") property = IntervalProperty::End;
        else if (column.transformation_type == "duration") property = IntervalProperty::Duration;
        else throw std::runtime_error("Unknown interval property: " + column.transformation_type);
        
        return std::make_unique<IntervalPropertyComputer<double>>(source, property, column.data_key);
    }
    else if (column.transformation_type == "interval_count" || 
             column.transformation_type == "interval_id" ||
             column.transformation_type == "interval_id_start" ||
             column.transformation_type == "interval_id_end") {
        // Get interval source for reference data
        auto source = _data_manager_extension->getIntervalSource(column.reference_data_key);
        if (!source) {
            throw std::runtime_error("Could not get interval source for: " + column.reference_data_key);
        }
        
        // Map transformation to IntervalOverlapOperation
        IntervalOverlapOperation operation;
        if (column.transformation_type == "interval_count") operation = IntervalOverlapOperation::CountOverlaps;
        else if (column.transformation_type == "interval_id") operation = IntervalOverlapOperation::AssignID;
        else if (column.transformation_type == "interval_id_start") operation = IntervalOverlapOperation::AssignID_Start;
        else if (column.transformation_type == "interval_id_end") operation = IntervalOverlapOperation::AssignID_End;
        else throw std::runtime_error("Unknown overlap operation: " + column.transformation_type);
        
        return std::make_unique<IntervalOverlapComputer<double>>(source, operation, column.reference_data_key);
    }
    
    throw std::runtime_error("Unknown transformation type: " + column.transformation_type);
}

std::string DataAggregationExporter_Widget::_getDelimiter() const
{
    QString delimiter_text = ui->delimiter_combo->currentText();
    if (delimiter_text == "Comma") return ",";
    if (delimiter_text == "Space") return " ";
    if (delimiter_text == "Tab") return "\t";
    return ","; // Default
}

std::string DataAggregationExporter_Widget::_getLineEnding() const
{
    QString line_ending_text = ui->line_ending_combo->currentText();
    if (line_ending_text == "LF (\\n)") return "\n";
    if (line_ending_text == "CRLF (\\r\\n)") return "\r\n";
    return "\n"; // Default
}

bool DataAggregationExporter_Widget::_shouldIncludeHeader() const
{
    return ui->save_header_checkbox->isChecked();
}

int DataAggregationExporter_Widget::_getPrecision() const
{
    return ui->precision_spinbox->value();
} 
