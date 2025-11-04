#ifndef TABLEDESIGNERWIDGET_HPP
#define TABLEDESIGNERWIDGET_HPP

#include "DataManager/utils/TableView/TableInfo.hpp"
#include "utils/TableView/ComputerRegistryTypes.hpp"

#include <QStringList>
#include <QWidget>

#include <QMap>
#include <fstream>
#include <map>
#include <memory>

class DataManager;
class DataManagerExtension;
class ComputerRegistry;
class IRowSelector;
class TableViewBuilder;
class IParameterDescriptor;
class TableViewerWidget;
class QTimer;
class QTreeWidgetItem;
class QVBoxLayout;
class TableInfoWidget;
class Section;
class TableTransformWidget;
class TableExportWidget;
class TableJSONWidget;
class TableView;

namespace Ui {
class TableDesignerWidget;
}

/**
 * @brief Widget for designing and creating table views
 * 
 * This widget provides an interface for users to:
 * 1. Create new tables or modify existing ones
 * 2. Select row data sources (TimeFrame, DigitalEventSeries, DigitalIntervalSeries)
 * 3. Add columns by selecting data sources and computers
 * 4. Build and store the resulting TableView
 */
class TableDesignerWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableDesignerWidget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~TableDesignerWidget() override;

    /**
     * @brief Refresh all data sources (useful if data is loaded after widget creation)
     */
    void refreshAllDataSources();

signals:
    /**
     * @brief Emitted when a table is successfully created or updated
     * @param table_id The ID of the created/updated table
     */
    void tableCreated(QString const & table_id);

    /**
     * @brief Emitted when a table is deleted
     * @param table_id The ID of the deleted table
     */
    void tableDeleted(QString const & table_id);

public:
    /**
     * @brief Get enabled column infos from the computers tree (for testing)
     * @return Vector of ColumnInfo for enabled computers
     */
    std::vector<ColumnInfo> getEnabledColumnInfos() const;

    /**
     * @brief Build table from enabled computers in the tree
     * @return True if build was successful
     */
    bool buildTableFromTree();

private slots:
    /**
     * @brief Handle changes to the table selection
     */
    void onTableSelectionChanged();

    /**
     * @brief Handle creation of a new table
     */
    void onCreateNewTable();

    /**
     * @brief Handle deletion of the current table
     */
    void onDeleteTable();

    /**
     * @brief Handle changes to the row data source selection
     */
    void onRowDataSourceChanged();

    /**
     * @brief Handle changes to the capture range
     */
    void onCaptureRangeChanged();

    /**
     * @brief Handle changes to the interval settings
     */
    void onIntervalSettingChanged();

    /**
     * @brief Handle changes to the computer tree (checkbox state changes)
     */
    void onComputersTreeItemChanged();

    /**
     * @brief Handle changes to column names in the tree
     */
    void onComputersTreeItemEdited(QTreeWidgetItem * item, int column);

    /**
     * @brief Handle group mode toggle button
     */
    void onGroupModeToggled(bool enabled); /**
     * @brief Handle building the table
     */
    void onBuildTable();
    void onApplyTransform();
    void onExportCsv();

    /**
     * @brief Handle saving the table metadata
     */
    void onSaveTableInfo();

    /**
     * @brief Handle table manager signals
     */
    void onTableManagerTableCreated(QString const & table_id);
    void onTableManagerTableRemoved(QString const & table_id);
    void onTableManagerTableInfoUpdated(QString const & table_id);

private:
    Ui::TableDesignerWidget * ui;
    std::shared_ptr<DataManager> _data_manager;

    QString _current_table_id;
    bool _loading_column_configuration = false;    // Flag to prevent infinite loops
    bool _updating_computers_tree = false;         // Flag to prevent recursive updates during tree refresh
    QMap<QString, QStringList> _table_column_order;// Persist preview column order per table id

    // Grouping functionality
    std::string _grouping_pattern = "(.+)_\\d+$";// Default pattern: name_number (same as Feature_Tree_Widget)
    bool _group_mode = true;                     // Toggle between group and individual mode

    // Parameter UI management
    QWidget * _parameter_widget = nullptr;
    QVBoxLayout * _parameter_layout = nullptr;
    std::map<std::string, QWidget *> _parameter_controls;
    std::map<QTreeWidgetItem *, QWidget *> _computer_parameter_widgets;// Map computer items to their parameter widgets

    // Persisted computer states across refreshes: key = dataSource||computerName
    // Value = pair(checkState, customColumnName)
    QMap<QString, QPair<Qt::CheckState, QString>> _persisted_computer_states;

    TableInfoWidget * _table_info_widget = nullptr;
    Section * _table_info_section = nullptr;
    TableTransformWidget * _table_transform_widget = nullptr;
    Section * _table_transform_section = nullptr;
    TableExportWidget * _table_export_widget = nullptr;
    Section * _table_export_section = nullptr;
    TableJSONWidget * _table_json_widget = nullptr;
    Section * _table_json_section = nullptr;

    // Preview support
    TableViewerWidget * _table_viewer = nullptr;
    QTimer * _preview_debounce_timer = nullptr;

    /**
     * @brief Connect all signals and slots
     */
    void connectSignals();

    /**
     * @brief Refresh the table combo box
     */
    void refreshTableCombo();

    /**
     * @brief Refresh the row data source combo box
     */
    void refreshRowDataSourceCombo();

    /**
     * @brief Refresh the computers tree with available data sources and computers
     */
    void refreshComputersTree();

    /**
     * @brief Get available data sources from the data manager
     * @return List of data source names
     */
    QStringList getAvailableDataSources() const;

    /**
     * @brief Create a DataSourceVariant and determine RowSelectorType from a data source string
     * @param data_source_string The data source string (e.g., "Events: myEvents")
     * @param data_manager_extension The data manager extension to use for lookups
     * @return Pair of optional DataSourceVariant and RowSelectorType
     */
    std::pair<std::optional<DataSourceVariant>, RowSelectorType>
    createDataSourceVariant(QString const & data_source_string,
                            std::shared_ptr<DataManagerExtension> data_manager_extension) const;

    /**
     * @brief Create a DataSourceVariant for a column data source (without determining row selector type)
     * @param data_source_string The data source string (e.g., "analog:mySignal")
     * @param data_manager_extension The data manager extension to use for lookups
     * @return Optional DataSourceVariant
     */
    std::optional<DataSourceVariant>
    createColumnDataSourceVariant(QString const & data_source_string,
                                   std::shared_ptr<DataManagerExtension> data_manager_extension) const;

    /**
     * @brief Determine the row selector type from the currently selected row source
     * @return The RowSelectorType for the current selection, or nullopt if none selected
     */
    std::optional<RowSelectorType> getCurrentRowSelectorType() const;

    /**
     * @brief Create a row selector based on the selected row source
     * @param row_source The selected row source string
     * @return Unique pointer to the created row selector, or nullptr if creation failed
     */
    std::unique_ptr<IRowSelector> createRowSelector(QString const & row_source);

    /**
     * @brief Generate a default column name for a data source and computer combination
     * @param data_source The data source string
     * @param computer_name The computer name
     * @return Generated column name
     */
    QString generateDefaultColumnName(QString const & data_source, QString const & computer_name) const;

    /**
     * @brief Extract group name from data source using grouping pattern
     * @param data_source The data source string to extract group from
     * @return The extracted group name, or the original string if no group found
     */
    std::string extractGroupName(QString const & data_source) const;

    /**
     * @brief Create parameter UI widget for a computer with parameters
     * @param computer_name The name of the computer
     * @param parameter_descriptors The parameter descriptors from ComputerInfo
     * @return Widget containing parameter controls, or nullptr if no parameters
     */
    QWidget * createParameterWidget(QString const & computer_name,
                                    std::vector<std::unique_ptr<IParameterDescriptor>> const & parameter_descriptors);

    /**
     * @brief Get parameter values from a computer's parameter widget
     * @param computer_name The name of the computer
     * @return Map of parameter name to parameter value
     */
    std::map<std::string, std::string> getParameterValues(QString const & computer_name) const;

    // Persist state helper (updates _persisted_computer_states from a tree item)
    void persistComputerItemState(QTreeWidgetItem * item, int column);

    // Helper methods for the new tree-based approach
    void loadTableInfo(QString const & table_id);
    void clearUI();
    void updateBuildStatus(QString const & message, bool is_error = false);
    void updateRowInfoLabel(QString const & selected_source);
    void updateIntervalSettingsVisibility();
    int getCaptureRange() const;
    void setCaptureRange(int value);
    bool isIntervalBeginningSelected() const;
    bool isIntervalItselfSelected() const;
    void triggerPreviewDebounced();
    void rebuildPreviewNow();
    std::vector<ColumnInfo> reorderColumnsBySavedOrder(std::vector<ColumnInfo> column_infos) const;
    std::vector<std::string> parseCommaSeparatedList(QString const & text) const;
    QString promptSaveCsvFilename() const;
    QString promptSaveDirectoryForGroupExport() const;
    bool addColumnToBuilder(TableViewBuilder & builder, ColumnInfo const & column_info);

    /**
     * @brief Format vector data for CSV export
     * @param file Output file stream
     * @param values Vector of values to format
     * @param precision Decimal precision for floating point values
     */
    template<typename T>
    void formatVectorForCsv(std::ofstream & file, std::vector<T> const & values, int precision);
    
    /**
     * @brief Export table data to a single CSV file
     * @param view The TableView to export
     * @param filename The output filename
     * @param delim The delimiter character/string
     * @param eol The end-of-line string
     * @param precision Decimal precision for floating point values
     * @param includeHeader Whether to include column headers
     * @return True if export succeeded, false otherwise
     */
    bool exportTableToSingleCsv(TableView * view, QString const & filename,
                               std::string const & delim, std::string const & eol,
                               int precision, bool includeHeader);
    
    /**
     * @brief Export table data grouped by entity groups to separate CSV files
     * @param view The TableView to export
     * @param directory The output directory for CSV files
     * @param base_name The base filename (will be appended with group names)
     * @param delim The delimiter character/string
     * @param eol The end-of-line string
     * @param precision Decimal precision for floating point values
     * @param includeHeader Whether to include column headers
     * @return Number of files successfully exported
     */
    int exportTableByGroups(TableView * view, QString const & directory,
                           QString const & base_name,
                           std::string const & delim, std::string const & eol,
                           int precision, bool includeHeader);
    
    void setJsonTemplateFromCurrentState();
    void applyJsonTemplateToUI(QString const & jsonText);
    friend class TableDesignerWidgetJSONTestAccessor;// test helper
};

#endif// TABLEDESIGNERWIDGET_HPP
