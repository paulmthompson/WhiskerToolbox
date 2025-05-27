#include "DataAggregationExporter_Widget.hpp"
#include "ui_DataAggregationExporter_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

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
    auto transformations = _getAvailableTransformations(data_type);
    
    for (auto transformation : transformations) {
        QString display_name = _getTransformationDisplayName(transformation);
        ui->transformation_combo->addItem(display_name, static_cast<int>(transformation));
    }
    
    // Add interval metadata transformations (always available)
    ui->transformation_combo->addItem(_getTransformationDisplayName(TransformationType::IntervalStart), 
                                     static_cast<int>(TransformationType::IntervalStart));
    ui->transformation_combo->addItem(_getTransformationDisplayName(TransformationType::IntervalEnd), 
                                     static_cast<int>(TransformationType::IntervalEnd));
    ui->transformation_combo->addItem(_getTransformationDisplayName(TransformationType::IntervalDuration), 
                                     static_cast<int>(TransformationType::IntervalDuration));
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

std::vector<TransformationType> DataAggregationExporter_Widget::_getAvailableTransformations(DM_DataType data_type) const
{
    std::vector<TransformationType> transformations;
    
    switch (data_type) {
        case DM_DataType::Analog:
            transformations = {TransformationType::Mean, TransformationType::Min, 
                             TransformationType::Max, TransformationType::StdDev};
            break;
        case DM_DataType::Points:
            transformations = {TransformationType::PointX_Mean, TransformationType::PointY_Mean,
                             TransformationType::PointX_Min, TransformationType::PointX_Max,
                             TransformationType::PointY_Min, TransformationType::PointY_Max};
            break;
        case DM_DataType::DigitalInterval:
            transformations = {TransformationType::IntervalCount, TransformationType::TotalDuration};
            break;
        default:
            break;
    }
    
    return transformations;
}

QString DataAggregationExporter_Widget::_getTransformationDisplayName(TransformationType transformation) const
{
    switch (transformation) {
        case TransformationType::Mean: return "Mean";
        case TransformationType::Min: return "Minimum";
        case TransformationType::Max: return "Maximum";
        case TransformationType::StdDev: return "Standard Deviation";
        case TransformationType::PointX_Mean: return "Point X Mean";
        case TransformationType::PointY_Mean: return "Point Y Mean";
        case TransformationType::PointX_Min: return "Point X Minimum";
        case TransformationType::PointX_Max: return "Point X Maximum";
        case TransformationType::PointY_Min: return "Point Y Minimum";
        case TransformationType::PointY_Max: return "Point Y Maximum";
        case TransformationType::IntervalCount: return "Interval Count";
        case TransformationType::TotalDuration: return "Total Duration";
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
        case TransformationType::Mean: suffix = "_mean"; break;
        case TransformationType::Min: suffix = "_min"; break;
        case TransformationType::Max: suffix = "_max"; break;
        case TransformationType::StdDev: suffix = "_std"; break;
        case TransformationType::PointX_Mean: suffix = "_x_mean"; break;
        case TransformationType::PointY_Mean: suffix = "_y_mean"; break;
        case TransformationType::PointX_Min: suffix = "_x_min"; break;
        case TransformationType::PointX_Max: suffix = "_x_max"; break;
        case TransformationType::PointY_Min: suffix = "_y_min"; break;
        case TransformationType::PointY_Max: suffix = "_y_max"; break;
        case TransformationType::IntervalCount: suffix = "_count"; break;
        case TransformationType::TotalDuration: suffix = "_duration"; break;
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
    
    // Process each interval
    const auto& intervals = interval_data->getDigitalIntervalSeries();
    for (const auto& interval : intervals) {
        for (size_t i = 0; i < _export_columns.size(); ++i) {
            if (i > 0) file << delimiter;
            
            double value = _calculateTransformation(_export_columns[i], interval.start, interval.end);
            file << value;
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

double DataAggregationExporter_Widget::_calculateTransformation(const ExportColumn& column, 
                                                               int64_t start_time, int64_t end_time) const
{
    switch (column.data_type) {
        case DM_DataType::Analog:
            return _calculateAnalogTransformation(column.data_key, column.transformation, start_time, end_time);
        case DM_DataType::Points:
            return _calculatePointTransformation(column.data_key, column.transformation, start_time, end_time);
        case DM_DataType::DigitalInterval:
            return _calculateIntervalTransformation(column.data_key, column.transformation, start_time, end_time);
        default:
            // Check if it's interval metadata
            if (column.transformation == TransformationType::IntervalStart ||
                column.transformation == TransformationType::IntervalEnd ||
                column.transformation == TransformationType::IntervalDuration) {
                return _calculateIntervalMetadata(column.transformation, start_time, end_time);
            }
            return std::nan("");
    }
}

double DataAggregationExporter_Widget::_calculateAnalogTransformation(const std::string& data_key, 
                                                                     TransformationType transformation, 
                                                                     int64_t start_time, int64_t end_time) const
{
    auto analog_data = _data_manager->getData<AnalogTimeSeries>(data_key);
    if (!analog_data) {
        return std::nan("");
    }
    
    switch (transformation) {
        case TransformationType::Mean:
            return calculate_mean(*analog_data, start_time, end_time);
        case TransformationType::Min:
            return calculate_min(*analog_data, start_time, end_time);
        case TransformationType::Max:
            return calculate_max(*analog_data, start_time, end_time);
        case TransformationType::StdDev:
            return calculate_std_dev(*analog_data, start_time, end_time);
        default:
            return std::nan("");
    }
}

double DataAggregationExporter_Widget::_calculatePointTransformation(const std::string& data_key, 
                                                                    TransformationType transformation, 
                                                                    int64_t start_time, int64_t end_time) const
{
    auto point_data = _data_manager->getData<PointData>(data_key);
    if (!point_data) {
        return std::nan("");
    }
    
    // Collect first points in the time range
    std::vector<float> x_values, y_values;
    for (int64_t t = start_time; t <= end_time; ++t) {
        const auto& points = point_data->getPointsAtTime(static_cast<size_t>(t));
        if (!points.empty()) {
            x_values.push_back(points[0].x); // Use first point only
            y_values.push_back(points[0].y);
        }
    }
    
    if (x_values.empty()) {
        return std::nan("");
    }
    
    switch (transformation) {
        case TransformationType::PointX_Mean:
            return std::accumulate(x_values.begin(), x_values.end(), 0.0) / x_values.size();
        case TransformationType::PointY_Mean:
            return std::accumulate(y_values.begin(), y_values.end(), 0.0) / y_values.size();
        case TransformationType::PointX_Min:
            return *std::min_element(x_values.begin(), x_values.end());
        case TransformationType::PointX_Max:
            return *std::max_element(x_values.begin(), x_values.end());
        case TransformationType::PointY_Min:
            return *std::min_element(y_values.begin(), y_values.end());
        case TransformationType::PointY_Max:
            return *std::max_element(y_values.begin(), y_values.end());
        default:
            return std::nan("");
    }
}

double DataAggregationExporter_Widget::_calculateIntervalTransformation(const std::string& data_key, 
                                                                       TransformationType transformation, 
                                                                       int64_t start_time, int64_t end_time) const
{
    auto interval_data = _data_manager->getData<DigitalIntervalSeries>(data_key);
    if (!interval_data) {
        return std::nan("");
    }
    
    auto overlapping_intervals = interval_data->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
        start_time, end_time);
    
    switch (transformation) {
        case TransformationType::IntervalCount: {
            return static_cast<double>(std::distance(overlapping_intervals.begin(), overlapping_intervals.end()));
        }
        case TransformationType::TotalDuration: {
            int64_t total_duration = 0;
            for (const auto& interval : overlapping_intervals) {
                int64_t overlap_start = std::max(interval.start, start_time);
                int64_t overlap_end = std::min(interval.end, end_time);
                if (overlap_end >= overlap_start) {
                    total_duration += (overlap_end - overlap_start + 1);
                }
            }
            return static_cast<double>(total_duration);
        }
        default:
            return std::nan("");
    }
}

double DataAggregationExporter_Widget::_calculateIntervalMetadata(TransformationType transformation, 
                                                                 int64_t start_time, int64_t end_time) const
{
    switch (transformation) {
        case TransformationType::IntervalStart:
            return static_cast<double>(start_time);
        case TransformationType::IntervalEnd:
            return static_cast<double>(end_time);
        case TransformationType::IntervalDuration:
            return static_cast<double>(end_time - start_time + 1);
        default:
            return std::nan("");
    }
} 
