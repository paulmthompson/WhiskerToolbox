#ifndef TABLEDESIGNERWIDGET_HPP
#define TABLEDESIGNERWIDGET_HPP

#include "DataManager/utils/TableView/TableInfo.hpp"
#include "utils/TableView/ComputerRegistryTypes.hpp"
#include "TableViewerWidget/TableViewerWidget.hpp"

#include <QStringList>
#include <QWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>

#include <memory>
#include <map>

class DataManager;
class ComputerRegistry;
class IRowSelector;
class TableViewBuilder;
class IParameterDescriptor;
class TableViewerWidget;
class QTimer;
class QTreeWidgetItem;

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
    bool _loading_column_configuration = false; // Flag to prevent infinite loops
    bool _updating_computers_tree = false; // Flag to prevent recursive updates during tree refresh
    
    // Parameter UI management
    QWidget * _parameter_widget = nullptr;
    QVBoxLayout * _parameter_layout = nullptr;
    std::map<std::string, QWidget*> _parameter_controls;

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
    createDataSourceVariant(const QString& data_source_string, 
                           std::shared_ptr<DataManagerExtension> data_manager_extension) const;

    /**
     * @brief Create a row selector based on the selected row source
     * @param row_source The selected row source string
     * @return Unique pointer to the created row selector, or nullptr if creation failed
     */
    std::unique_ptr<IRowSelector> createRowSelector(QString const & row_source);

    /**
     * @brief Check if a computer is compatible with a data source
     * @param computer_name The computer name
     * @param data_source The data source string
     * @return True if compatible
     */
    bool isComputerCompatibleWithDataSource(const std::string& computer_name, const QString& data_source) const;

    /**
     * @brief Generate a default column name for a data source and computer combination
     * @param data_source The data source string
     * @param computer_name The computer name
     * @return Generated column name
     */
    QString generateDefaultColumnName(const QString& data_source, const QString& computer_name) const;

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
    std::vector<std::string> parseCommaSeparatedList(QString const & text) const;
    QString promptSaveCsvFilename() const;
    bool addColumnToBuilder(TableViewBuilder & builder, ColumnInfo const & column_info);
   
};

#endif// TABLEDESIGNERWIDGET_HPP


