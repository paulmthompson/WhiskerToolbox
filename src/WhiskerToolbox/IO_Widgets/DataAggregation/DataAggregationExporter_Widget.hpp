#ifndef DATA_AGGREGATION_EXPORTER_WIDGET_HPP
#define DATA_AGGREGATION_EXPORTER_WIDGET_HPP

#include "DataManager/DataManagerTypes.hpp"

#include <QWidget>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QTableWidget;
class QComboBox;
class QLineEdit;

namespace Ui {
class DataAggregationExporter_Widget;
}

/**
 * @brief Enumeration of available transformation operations for different data types
 */
enum class TransformationType {
    // For AnalogTimeSeries
    Mean,
    Min,
    Max,
    StdDev,
    
    // For PointData (using first point only)
    PointX_Mean,
    PointY_Mean,
    PointX_Min,
    PointX_Max,
    PointY_Min,
    PointY_Max,
    
    // For DigitalIntervalSeries
    IntervalCount,
    TotalDuration,
    
    // For interval metadata (always available)
    IntervalStart,
    IntervalEnd,
    IntervalDuration
};

/**
 * @brief Structure representing an export column configuration
 */
struct ExportColumn {
    std::string data_key;           // Key in DataManager
    DM_DataType data_type;          // Type of the data
    TransformationType transformation; // Transformation to apply
    std::string column_name;        // Name for CSV column
};

/**
 * @brief Widget for exporting aggregated data across time intervals to CSV
 * 
 * This widget allows users to:
 * 1. Select a DigitalIntervalSeries as the basis for aggregation
 * 2. Choose data keys and transformations to apply within each interval
 * 3. Configure CSV export options
 * 4. Export the aggregated data to a CSV file
 */
class DataAggregationExporter_Widget : public QWidget {
    Q_OBJECT

public:
    explicit DataAggregationExporter_Widget(QWidget *parent = nullptr);
    ~DataAggregationExporter_Widget() override;

    /**
     * @brief Set the DataManager instance to use for data access
     * @param data_manager Shared pointer to the DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

private slots:
    void _onDataManagerUpdated();
    void _onAvailableDataSelectionChanged();
    void _onTransformationChanged();
    void _onAddExportButtonClicked();
    void _onRemoveExportButtonClicked();
    void _onClearExportButtonClicked();
    void _onExportCsvButtonClicked();

private:
    Ui::DataAggregationExporter_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::vector<ExportColumn> _export_columns;

    // UI setup and population methods
    void _setupTables();
    void _populateAvailableDataTable();
    void _populateIntervalSourceCombo();
    void _populateTransformationCombo();
    void _updateExportListTable();
    
    // Transformation and naming methods
    std::vector<TransformationType> _getAvailableTransformations(DM_DataType data_type) const;
    QString _getTransformationDisplayName(TransformationType transformation) const;
    QString _generateDefaultColumnName(const std::string& data_key, TransformationType transformation) const;
    
    // Data processing methods
    std::string _getSelectedDataKey() const;
    DM_DataType _getSelectedDataType() const;
    std::string _getSelectedIntervalSource() const;
    
    // CSV export methods
    void _exportToCsv(const QString& filename);
    std::string _getDelimiter() const;
    std::string _getLineEnding() const;
    bool _shouldIncludeHeader() const;
    int _getPrecision() const;
    
    // Data aggregation methods
    double _calculateTransformation(const ExportColumn& column, int64_t start_time, int64_t end_time) const;
    double _calculateAnalogTransformation(const std::string& data_key, TransformationType transformation, 
                                        int64_t start_time, int64_t end_time) const;
    double _calculatePointTransformation(const std::string& data_key, TransformationType transformation, 
                                       int64_t start_time, int64_t end_time) const;
    double _calculateIntervalTransformation(const std::string& data_key, TransformationType transformation, 
                                          int64_t start_time, int64_t end_time) const;
    double _calculateIntervalMetadata(TransformationType transformation, 
                                    int64_t start_time, int64_t end_time) const;
};

#endif // DATA_AGGREGATION_EXPORTER_WIDGET_HPP 