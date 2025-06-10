#include "DataAggregationExporter_Widget.hpp"
#include "ui_DataAggregationExporter_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/utils/DataAggregation/DataAggregation.hpp"

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
    
    // Add observer to DataManager for updates
    if (_data_manager) {
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
        std::string time_frame = _data_manager->getTimeFrame(key);
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
    
    for (auto transformation : transformations) {
        QString display_name = _getTransformationDisplayName(transformation);
        ui->transformation_combo->addItem(display_name, static_cast<int>(transformation));
    }
}

void DataAggregationExporter_Widget::_onTransformationChanged()
{
    std::string selected_key = _getSelectedDataKey();
    if (selected_key.empty()) {
        ui->column_name_edit->clear();
        return;
    }
    
    int transformation_index = ui->transformation_combo->currentData().toInt();
    TransformationType transformation = static_cast<TransformationType>(transformation_index);
    
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
    column.transformation = static_cast<TransformationType>(ui->transformation_combo->currentData().toInt());
    column.column_name = column_name.toStdString();
    
    // For transformations that need reference data, store the data key
    if (column.transformation == TransformationType::IntervalID || 
        column.transformation == TransformationType::IntervalCount) {
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
                                      new QTableWidgetItem(_getTransformationDisplayName(column.transformation)));
        ui->export_list_table->setItem(static_cast<int>(i), 3, 
                                      new QTableWidgetItem(QString::fromStdString(column.column_name)));
    }
}

std::vector<TransformationType> DataAggregationExporter_Widget::_getAvailableTransformations(DM_DataType data_type, 
                                                                                        const std::string& selected_key,
                                                                                        const std::string& interval_source) const
{
    std::vector<TransformationType> transformations;
    
    switch (data_type) {
        case DM_DataType::Analog:
            transformations = {TransformationType::AnalogMean, TransformationType::AnalogMin, 
                             TransformationType::AnalogMax, TransformationType::AnalogStdDev};
            break;
        case DM_DataType::Points:
            transformations = {TransformationType::PointMeanX, TransformationType::PointMeanY};
            break;
        case DM_DataType::DigitalInterval:
            // Check if this is the same interval as the source or a different one
            if (selected_key == interval_source) {
                // Same interval: offer metadata transformations
                transformations = {TransformationType::IntervalStart, 
                                 TransformationType::IntervalEnd,
                                 TransformationType::IntervalDuration};
            } else {
                // Different interval: offer relational transformations
                transformations = {TransformationType::IntervalCount,
                                 TransformationType::IntervalID};
            }
            break;
        default:
            break;
    }
    
    return transformations;
}

QString DataAggregationExporter_Widget::_getTransformationDisplayName(TransformationType transformation) const
{
    switch (transformation) {
        case TransformationType::AnalogMean: return "Mean";
        case TransformationType::AnalogMin: return "Minimum";
        case TransformationType::AnalogMax: return "Maximum";
        case TransformationType::AnalogStdDev: return "Standard Deviation";
        case TransformationType::PointMeanX: return "Point Mean X";
        case TransformationType::PointMeanY: return "Point Mean Y";
        case TransformationType::IntervalCount: return "Interval Count";
        case TransformationType::IntervalID: return "Interval ID";
        case TransformationType::IntervalStart: return "Interval Start";
        case TransformationType::IntervalEnd: return "Interval End";
        case TransformationType::IntervalDuration: return "Interval Duration";
        default: return "Unknown";
    }
}

QString DataAggregationExporter_Widget::_generateDefaultColumnName(const std::string& data_key, TransformationType transformation) const
{
    QString base_name = QString::fromStdString(data_key);
    QString suffix;
    
    switch (transformation) {
        case TransformationType::AnalogMean: suffix = "_mean"; break;
        case TransformationType::AnalogMin: suffix = "_min"; break;
        case TransformationType::AnalogMax: suffix = "_max"; break;
        case TransformationType::AnalogStdDev: suffix = "_std"; break;
        case TransformationType::PointMeanX: suffix = "_x_mean"; break;
        case TransformationType::PointMeanY: suffix = "_y_mean"; break;
        case TransformationType::IntervalCount: suffix = "_count"; break;
        case TransformationType::IntervalID: suffix = "_id"; break;
        case TransformationType::IntervalStart: return "interval_start";
        case TransformationType::IntervalEnd: return "interval_end";
        case TransformationType::IntervalDuration: return "interval_duration";
        default: suffix = "_unknown"; break;
    }
    
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
    if (!_data_manager) {
        throw std::runtime_error("No DataManager available");
    }
    
    std::string interval_source = _getSelectedIntervalSource();
    auto interval_data = _data_manager->getData<DigitalIntervalSeries>(interval_source);
    if (!interval_data) {
        throw std::runtime_error("Could not retrieve interval data for key: " + interval_source);
    }
    
    // Get row intervals from the selected interval source
    const auto& intervals = interval_data->getDigitalIntervalSeries();
    
    // Build reference data maps
    auto reference_intervals = _buildReferenceIntervals();
    auto reference_analog = _buildReferenceAnalog();
    auto reference_points = _buildReferencePoints();
    
    // Build transformation configurations
    auto transformation_configs = _buildTransformationConfigs();
    
    // Use DataAggregation module to compute results
    auto result = DataAggregation::aggregateData(intervals, transformation_configs, 
                                               reference_intervals, reference_analog, reference_points);
    
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
    for (const auto& row : result) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) file << delimiter;
            file << row[i];
        }
        file << line_ending;
    }
    
    file.close();
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

std::map<std::string, std::vector<Interval>> DataAggregationExporter_Widget::_buildReferenceIntervals() const
{
    std::map<std::string, std::vector<Interval>> reference_intervals;
    
    // Collect all DigitalIntervalSeries referenced by export columns
    for (const auto& column : _export_columns) {
        if (column.data_type == DM_DataType::DigitalInterval && 
            !column.reference_data_key.empty()) {
            
            auto interval_data = _data_manager->getData<DigitalIntervalSeries>(column.reference_data_key);
            if (interval_data) {
                reference_intervals[column.reference_data_key] = interval_data->getDigitalIntervalSeries();
            }
        }
    }
    
    return reference_intervals;
}

std::map<std::string, std::shared_ptr<AnalogTimeSeries>> DataAggregationExporter_Widget::_buildReferenceAnalog() const
{
    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog;
    
    // Collect all AnalogTimeSeries referenced by export columns  
    for (const auto& column : _export_columns) {
        if (column.data_type == DM_DataType::Analog) {
            auto analog_data = _data_manager->getData<AnalogTimeSeries>(column.data_key);
            if (analog_data) {
                reference_analog[column.data_key] = analog_data;
            }
        }
    }
    
    return reference_analog;
}

std::map<std::string, std::shared_ptr<PointData>> DataAggregationExporter_Widget::_buildReferencePoints() const
{
    std::map<std::string, std::shared_ptr<PointData>> reference_points;
    
    // Collect all PointData referenced by export columns
    for (const auto& column : _export_columns) {
        if (column.data_type == DM_DataType::Points) {
            auto point_data = _data_manager->getData<PointData>(column.data_key);
            if (point_data) {
                reference_points[column.data_key] = point_data;
            }
        }
    }
    
    return reference_points;
}

std::vector<TransformationConfig> DataAggregationExporter_Widget::_buildTransformationConfigs() const
{
    std::vector<TransformationConfig> configs;
    configs.reserve(_export_columns.size());
    
    for (const auto& column : _export_columns) {
        // Check which transformations need reference data keys
        if (column.transformation == TransformationType::IntervalID || 
            column.transformation == TransformationType::IntervalCount) {
            // Interval transformations need reference data key
            configs.emplace_back(column.transformation, column.column_name, column.reference_data_key);
        } else if (column.transformation == TransformationType::AnalogMean ||
                   column.transformation == TransformationType::AnalogMin ||
                   column.transformation == TransformationType::AnalogMax ||
                   column.transformation == TransformationType::AnalogStdDev) {
            // Analog transformations need reference data key (use data_key as reference)
            configs.emplace_back(column.transformation, column.column_name, column.data_key);
        } else if (column.transformation == TransformationType::PointMeanX ||
                   column.transformation == TransformationType::PointMeanY) {
            // Point transformations need reference data key (use data_key as reference)
            configs.emplace_back(column.transformation, column.column_name, column.data_key);
        } else {
            // Interval metadata transformations don't need reference data key
            configs.emplace_back(column.transformation, column.column_name);
        }
    }
    
    return configs;
} 
