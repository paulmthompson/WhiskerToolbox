#ifndef TABLEDESIGNERWIDGET_HPP
#define TABLEDESIGNERWIDGET_HPP

#include "TableInfo.hpp"

#include <QStringList>
#include <QWidget>

#include <memory>

class TableManager;
class DataManager;
class ComputerRegistry;
class IRowSelector;
class TableViewBuilder;

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
    explicit TableDesignerWidget(TableManager * table_manager, std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
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
     * @brief Handle adding a new column
     */
    void onAddColumn();

    /**
     * @brief Handle removing a column
     */
    void onRemoveColumn();

    /**
     * @brief Handle moving a column up in the list
     */
    void onMoveColumnUp();

    /**
     * @brief Handle moving a column down in the list
     */
    void onMoveColumnDown();

    /**
     * @brief Handle changes to the column data source selection
     */
    void onColumnDataSourceChanged();

    /**
     * @brief Handle changes to the column computer selection
     */
    void onColumnComputerChanged();

    /**
     * @brief Handle changes to the column list selection
     */
    void onColumnSelectionChanged();

    /**
     * @brief Handle changes to the column name text
     */
    void onColumnNameChanged();

    /**
     * @brief Handle changes to the column description text
     */
    void onColumnDescriptionChanged();

    /**
     * @brief Handle building the table
     */
    void onBuildTable();

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
    TableManager * _table_manager;
    std::shared_ptr<DataManager> _data_manager;

    QString _current_table_id;

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
     * @brief Refresh the column data source combo box
     */
    void refreshColumnDataSourceCombo();

    /**
     * @brief Refresh the column computer combo box based on current selections
     */
    void refreshColumnComputerCombo();

    /**
     * @brief Load table information into the UI
     * @param table_id The table ID to load
     */
    void loadTableInfo(QString const & table_id);

    /**
     * @brief Clear the UI when no table is selected
     */
    void clearUI();

    /**
     * @brief Update the build status label
     * @param message The status message to display
     * @param is_error Whether this is an error message
     */
    void updateBuildStatus(QString const & message, bool is_error = false);

    /**
     * @brief Update the row info label without triggering signals
     * @param selected_source The selected row data source
     */
    void updateRowInfoLabel(QString const & selected_source);

    /**
     * @brief Update the visibility of interval settings based on selected data type
     */
    void updateIntervalSettingsVisibility();

    /**
     * @brief Get the capture range value in samples
     * @return Capture range value
     */
    int getCaptureRange() const;

    /**
     * @brief Set the capture range value in samples
     * @param value Capture range value
     */
    void setCaptureRange(int value);

    /**
     * @brief Check if interval beginning is selected
     * @return True if beginning is selected, false if end is selected
     */
    bool isIntervalBeginningSelected() const;

    /**
     * @brief Check if interval itself is selected
     * @return True if interval itself is selected, false otherwise
     */
    bool isIntervalItselfSelected() const;

    /**
     * @brief Load column configuration from table manager into UI
     * @param column_index The index of the column to load
     */
    void loadColumnConfiguration(int column_index);

    /**
     * @brief Save current column configuration from UI to table manager
     */
    void saveCurrentColumnConfiguration();

    /**
     * @brief Clear the column configuration UI
     */
    void clearColumnConfiguration();

    /**
     * @brief Create a row selector based on the selected row source
     * @param row_source The selected row source string
     * @return Unique pointer to the created row selector, or nullptr if creation failed
     */
    std::unique_ptr<IRowSelector> createRowSelector(QString const & row_source);

    /**
     * @brief Add a column to the table view builder
     * @param builder Reference to the TableViewBuilder
     * @param column_info The column configuration
     * @return True if the column was added successfully, false otherwise
     */
    bool addColumnToBuilder(TableViewBuilder & builder, ColumnInfo const & column_info);

    /**
     * @brief Get available data sources from the data manager
     * @return List of data source names
     */
    QStringList getAvailableDataSources() const;

    /**
     * @brief Get available table columns that can be used as data sources
     * @return List of table column references
     */
    QStringList getAvailableTableColumns() const;
};

#endif// TABLEDESIGNERWIDGET_HPP
