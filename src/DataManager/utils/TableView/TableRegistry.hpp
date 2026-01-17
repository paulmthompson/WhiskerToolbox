#ifndef TABLE_REGISTRY_HPP
#define TABLE_REGISTRY_HPP

#include "TableEvents.hpp"
#include "utils/TableView/TableInfo.hpp"
#include "datamanager_export.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

class ComputerRegistry;
class DataManager;
class DataManagerExtension;
class TableView;
struct ComputerInfo;

/**
 * @brief Non-Qt registry managing table definitions and built TableView instances.
 */
class DATAMANAGER_EXPORT TableRegistry {
public:
    explicit TableRegistry(DataManager & data_manager);

    ~TableRegistry();

    // Services
    ComputerRegistry & getComputerRegistry() { return *_computer_registry; }
    ComputerRegistry const & getComputerRegistry() const { return *_computer_registry; }
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() const { return _data_manager_extension; }

    // CRUD
    bool createTable(std::string const & table_id, std::string const & table_name, std::string const & table_description = "");
    bool removeTable(std::string const & table_id);
    bool hasTable(std::string const & table_id) const;
    TableInfo getTableInfo(std::string const & table_id) const;
    std::vector<std::string> getTableIds() const;
    std::vector<TableInfo> getAllTableInfo() const;
    bool setTableView(std::string const & table_id, std::shared_ptr<TableView> table_view);
    bool updateTableInfo(std::string const & table_id, std::string const & table_name, std::string const & table_description = "");
    bool updateTableRowSource(std::string const & table_id, std::string const & row_source_name);

    // Columns
    bool addTableColumn(std::string const & table_id, ColumnInfo const & column_info);
    bool updateTableColumn(std::string const & table_id, size_t column_index, ColumnInfo const & column_info);
    bool removeTableColumn(std::string const & table_id, size_t column_index);
    ColumnInfo getTableColumn(std::string const & table_id, size_t column_index) const;

    // Built views
    bool storeBuiltTable(std::string const & table_id, std::unique_ptr<TableView> table_view);
    std::shared_ptr<TableView> getBuiltTable(std::string const & table_id) const;

    // Utilities
    std::string generateUniqueTableId(std::string const & base_name = "Table") const;

    // Type-aware helpers
    bool addTableColumnWithTypeInfo(std::string const & table_id, ColumnInfo & column_info);
    std::vector<std::string> getAvailableComputersForDataSource(std::string const & row_selector_type, std::string const & data_source_name) const;
    std::tuple<std::string, bool, std::string> getComputerTypeInfo(std::string const & computer_name) const;
    ComputerInfo const * getComputerInfo(std::string const & computer_name) const;
    std::vector<std::string> getAvailableOutputTypes() const;

    // Column building utilities
    /**
     * @brief Add a column to a TableViewBuilder from a ColumnInfo specification
     * @param builder The TableViewBuilder to add the column to
     * @param column_info The column specification including data source, computer, and parameters
     * @return True if the column was successfully added, false otherwise
     */
    bool addColumnToBuilder(class TableViewBuilder & builder, ColumnInfo const & column_info) const;

private:
    DataManager & _data_manager;
    std::shared_ptr<DataManagerExtension> _data_manager_extension;
    std::unique_ptr<ComputerRegistry> _computer_registry;

    std::map<std::string, TableInfo> _table_info;
    std::map<std::string, std::shared_ptr<TableView>> _table_views;
    mutable int _next_table_counter = 1;

    void notify(TableEventType type, std::string const & table_id) const;
};

#endif// TABLE_REGISTRY_HPP
