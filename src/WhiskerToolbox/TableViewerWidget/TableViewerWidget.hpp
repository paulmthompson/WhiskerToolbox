#ifndef TABLEVIEWERWIDGET_HPP
#define TABLEVIEWERWIDGET_HPP

#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/TableInfo.hpp"

#include <QWidget>
#include <QStringList>

#include <memory>
#include <vector>

class PaginatedTableModel;
class IRowSelector;
class DataManager;

namespace Ui {
class TableViewerWidget;
}

/**
 * @brief Widget for viewing TableView objects with efficient scrolling
 * 
 * This widget provides a read-only interface for viewing table data.
 * It can display either:
 * 1. Pre-built TableView objects (complete tables)
 * 2. Table configurations using mini selectors for efficient pagination
 * 
 * The widget handles large datasets by creating small TableView windows
 * on-demand rather than materializing entire tables.
 */
class TableViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableViewerWidget(QWidget * parent = nullptr);
    ~TableViewerWidget() override;

    /**
     * @brief Display a complete TableView object
     * @param table_view Pre-built table to display
     * @param table_name Display name for the table (optional)
     */
    void setTableView(std::shared_ptr<TableView> table_view, QString const & table_name = "");

    /**
     * @brief Display a table using pagination with mini selectors
     * @param row_selector Row selector defining all rows
     * @param column_infos Column definitions
     * @param data_manager Data manager for building mini tables
     * @param table_name Display name for the table (optional)
     */
    void setTableConfiguration(std::unique_ptr<IRowSelector> row_selector,
                              std::vector<ColumnInfo> column_infos,
                              std::shared_ptr<DataManager> data_manager,
                              QString const & table_name = "");

    /**
     * @brief Clear the current table display
     */
    void clearTable();

    /**
     * @brief Set the page size for pagination (default: 1000 rows)
     * @param page_size Number of rows per page
     */
    void setPageSize(size_t page_size);

    /**
     * @brief Get the current table name being displayed
     */
    QString getTableName() const;

    /**
     * @brief Check if a table is currently loaded
     */
    bool hasTable() const;

signals:
    /**
     * @brief Emitted when the user scrolls to a specific row
     * @param row_index Global row index (0-based)
     */
    void rowScrolled(size_t row_index);

    /**
     * @brief Emitted when columns are reordered via the header
     */
    void columnsReordered(QStringList const & newOrder);

private slots:

    /**
     * @brief Handle scroll events to update row info
     */
    void onTableScrolled();

private:
    Ui::TableViewerWidget * ui;
    PaginatedTableModel * _model;
    QString _table_name;
    size_t _total_rows = 0;
    QStringList _currentColumnOrder;


    /**
     * @brief Connect all signals and slots
     */
    void connectSignals();

public:
    /**
     * @brief Get current visual column order (left-to-right column names)
     */
    QStringList getCurrentColumnOrder() const { return _currentColumnOrder; }

    /**
     * @brief Apply a desired column order by column names
     */
    void applyColumnOrder(QStringList const & desiredOrder);
};

#endif // TABLEVIEWERWIDGET_HPP
