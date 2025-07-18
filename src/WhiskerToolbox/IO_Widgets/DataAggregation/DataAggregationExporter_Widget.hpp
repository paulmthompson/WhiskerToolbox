#ifndef DATA_AGGREGATION_EXPORTER_WIDGET_HPP
#define DATA_AGGREGATION_EXPORTER_WIDGET_HPP

#include "DataManager/DataManagerTypes.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/computers/IntervalReductionComputer.h"
#include "DataManager/utils/TableView/computers/IntervalPropertyComputer.h"
#include "DataManager/utils/TableView/computers/IntervalOverlapComputer.h"
#include "DataManager/utils/TableView/computers/AnalogSliceGathererComputer.h"

#include <QWidget>
#include <QString>
#include <QStringList>

#include <map>
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
 * @brief Structure representing an export column configuration for the UI
 */
struct ExportColumn {
    std::string data_key;                    // Key in DataManager
    DM_DataType data_type;                   // Type of the data  
    std::string transformation_type;          // Type of transformation (e.g., "mean", "max", "start", etc.)
    std::string column_name;                 // Name for CSV column
    std::string reference_data_key;          // For transformations that need reference data
};

/**
 * @brief Widget for exporting aggregated data across time intervals to CSV using TableView
 * 
 * This widget allows users to:
 * 1. Select a DigitalIntervalSeries as the basis for aggregation
 * 2. Choose data keys and transformations to apply within each interval
 * 3. Configure CSV export options
 * 4. Export the aggregated data to a CSV file using the TableView system
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
    std::shared_ptr<DataManagerExtension> _data_manager_extension;
    std::vector<ExportColumn> _export_columns;

    // UI setup and population methods
    void _setupTables();
    void _populateAvailableDataTable();
    void _populateIntervalSourceCombo();
    void _populateTransformationCombo();
    void _updateExportListTable();
    
    // Transformation and naming methods
    std::vector<std::string> _getAvailableTransformations(DM_DataType data_type, 
                                                         const std::string& selected_key,
                                                         const std::string& interval_source) const;
    QString _getTransformationDisplayName(const std::string& transformation) const;
    QString _generateDefaultColumnName(const std::string& data_key, const std::string& transformation) const;
    
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
    
    // TableView construction methods
    TableView _buildTableView() const;
    std::unique_ptr<IRowSelector> _createRowSelector() const;
    void _addColumnsToBuilder(TableViewBuilder& builder) const;
    std::unique_ptr<IColumnComputer<double>> _createComputer(const ExportColumn& column) const;
};

#endif // DATA_AGGREGATION_EXPORTER_WIDGET_HPP 