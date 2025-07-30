#ifndef TABLEDESIGNERWIDGET_HPP
#define TABLEDESIGNERWIDGET_HPP

#include <QWidget>
#include <QStringList>
#include <memory>

class QVBoxLayout;
class QHBoxLayout;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLineEdit;
class QTextEdit;
class QGroupBox;
class QLabel;
class QSplitter;

class TableManager;
class DataManager;
class ComputerRegistry;

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
    explicit TableDesignerWidget(TableManager* table_manager, std::shared_ptr<DataManager> data_manager, QWidget* parent = nullptr);
    ~TableDesignerWidget() override;

signals:
    /**
     * @brief Emitted when a table is successfully created or updated
     * @param table_id The ID of the created/updated table
     */
    void tableCreated(const QString& table_id);

    /**
     * @brief Emitted when a table is deleted
     * @param table_id The ID of the deleted table
     */
    void tableDeleted(const QString& table_id);

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
    void onTableManagerTableCreated(const QString& table_id);
    void onTableManagerTableRemoved(const QString& table_id);
    void onTableManagerTableInfoUpdated(const QString& table_id);

private:
    Ui::TableDesignerWidget* ui;
    TableManager* _table_manager;

    std::shared_ptr<DataManager> _data_manager;
    
    // UI components (manually created for now, could use .ui file later)
    QVBoxLayout* _main_layout;
    
    // Table selection section
    QGroupBox* _table_selection_group;
    QComboBox* _table_combo;
    QPushButton* _new_table_btn;
    QPushButton* _delete_table_btn;
    
    // Table info section
    QGroupBox* _table_info_group;
    QLineEdit* _table_name_edit;
    QTextEdit* _table_description_edit;
    QPushButton* _save_info_btn;
    
    // Row data source section
    QGroupBox* _row_source_group;
    QComboBox* _row_data_source_combo;
    QLabel* _row_info_label;
    
    // Column design section
    QGroupBox* _column_design_group;
    QSplitter* _column_splitter;
    
    // Column list (left side)
    QWidget* _column_list_widget;
    QListWidget* _column_list;
    QPushButton* _add_column_btn;
    QPushButton* _remove_column_btn;
    QPushButton* _move_up_btn;
    QPushButton* _move_down_btn;
    
    // Column configuration (right side)
    QWidget* _column_config_widget;
    QComboBox* _column_data_source_combo;
    QComboBox* _column_computer_combo;
    QLineEdit* _column_name_edit;
    QTextEdit* _column_description_edit;
    
    // Build section
    QGroupBox* _build_group;
    QPushButton* _build_table_btn;
    QLabel* _build_status_label;
    
    QString _current_table_id;
    
    /**
     * @brief Initialize the UI layout and components
     */
    void initializeUI();
    
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
    void loadTableInfo(const QString& table_id);
    
    /**
     * @brief Clear the UI when no table is selected
     */
    void clearUI();
    
    /**
     * @brief Update the build status label
     * @param message The status message to display
     * @param is_error Whether this is an error message
     */
    void updateBuildStatus(const QString& message, bool is_error = false);
    
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

#endif // TABLEDESIGNERWIDGET_HPP
