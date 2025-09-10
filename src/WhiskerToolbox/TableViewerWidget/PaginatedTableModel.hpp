#ifndef PAGINATEDTABLEMODEL_HPP
#define PAGINATEDTABLEMODEL_HPP

#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/TableInfo.hpp"


#include <QAbstractTableModel>
#include <QVariant>
#include <QStringList>

#include <memory>
#include <map>
#include <string>
#include <vector>

class IRowSelector;
class DataManager;

/**
 * @brief Table model that uses mini selectors to efficiently display large tables
 * 
 * This model creates small TableView windows on-the-fly rather than materializing
 * the entire table, allowing efficient scrolling through very large datasets.
 */
class PaginatedTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PaginatedTableModel(QObject * parent = nullptr);
    ~PaginatedTableModel() override;

    /**
     * @brief Set the source table configuration for pagination
     * @param row_selector The row selector defining all rows
     * @param column_infos Column definitions for the table
     * @param data_manager Data manager for building mini tables
     */
    void setSourceTable(std::unique_ptr<IRowSelector> row_selector,
                       std::vector<ColumnInfo> column_infos,
                       std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Set a complete TableView (for cases where we already have one built)
     * @param table_view Pre-built table view
     */
    void setTableView(std::shared_ptr<TableView> table_view);

    /**
     * @brief Clear the current table
     */
    void clearTable();

    /**
     * @brief Set the page size for mini table windows
     * @param page_size Number of rows per page (default: 1000)
     */
    void setPageSize(size_t page_size);

    // QAbstractTableModel API
    [[nodiscard]] int rowCount(QModelIndex const & parent) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /**
     * @brief Get the count of mini pages materialized during model lifetime
     * @return Number of pages created and cached at least once
     * @pre None
     * @post No side-effects
     */
    [[nodiscard]] size_t getMaterializedPageCount() const { return _materialized_page_count; }

private:
    // Source configuration for pagination
    std::unique_ptr<IRowSelector> _source_row_selector;
    std::vector<ColumnInfo> _column_infos;
    std::shared_ptr<DataManager> _data_manager;
    
    // Pre-built table view (alternative to pagination)
    std::shared_ptr<TableView> _complete_table_view;
    
    // Pagination state
    size_t _total_rows = 0;
    size_t _page_size = 1000;
    QStringList _column_names;
    
    // Cache for mini tables
    mutable std::map<size_t, std::shared_ptr<TableView>> _page_cache;
    static constexpr size_t MAX_CACHED_PAGES = 10;
    // Testing/diagnostics: count pages created
    mutable size_t _materialized_page_count = 0;
    
    /**
     * @brief Get or create a mini table for the page containing the given row
     * @param row_index Global row index
     * @return Pair of (mini_table, local_row_index_within_mini_table)
     */
    std::pair<std::shared_ptr<TableView>, size_t> getMiniTableForRow(size_t row_index) const;
    
    /**
     * @brief Create a mini table for a specific page
     * @param page_start_row Starting row for this page
     * @param page_size Number of rows in this page
     * @return Mini table view for this page
     */
    std::shared_ptr<TableView> createMiniTable(size_t page_start_row, size_t page_size) const;
    
    /**
     * @brief Clean up old cached pages if we exceed the cache limit
     */
    void cleanupCache() const;

    /**
     * @brief Format data value for display
     */
    static QString formatValue(std::shared_ptr<TableView> const & mini_table, 
                              std::string const & column_name, 
                              size_t local_row);
    
    template<typename T>
    static QString joinVector(std::vector<T> const & values);
};

#endif // PAGINATEDTABLEMODEL_HPP
