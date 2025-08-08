#ifndef TABLE_REGISTRY_HPP
#define TABLE_REGISTRY_HPP

#include "TableEvents.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/ComputerRegistry.hpp"

#include "utils/TableView/TableInfo.hpp"

#include <QMap>
#include <QString>
#include <QStringList>

#include <memory>
#include <tuple>
#include <vector>

class DataManager;
struct ComputerInfo;

/**
 * @brief Non-Qt registry managing table definitions and built TableView instances.
 */
class TableRegistry {
public:
    explicit TableRegistry(DataManager & data_manager);

    ~TableRegistry();

    // Services
    ComputerRegistry & getComputerRegistry() { return *_computer_registry; }
    ComputerRegistry const & getComputerRegistry() const { return *_computer_registry; }
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() const { return _data_manager_extension; }

    // CRUD
    bool createTable(QString const & table_id, QString const & table_name, QString const & table_description = "");
    bool removeTable(QString const & table_id);
    bool hasTable(QString const & table_id) const;
    TableInfo getTableInfo(QString const & table_id) const;
    std::vector<QString> getTableIds() const;
    std::vector<TableInfo> getAllTableInfo() const;
    bool setTableView(QString const & table_id, std::shared_ptr<TableView> table_view);
    bool updateTableInfo(QString const & table_id, QString const & table_name, QString const & table_description = "");
    bool updateTableRowSource(QString const & table_id, QString const & row_source_name);

    // Columns
    bool addTableColumn(QString const & table_id, ColumnInfo const & column_info);
    bool updateTableColumn(QString const & table_id, int column_index, ColumnInfo const & column_info);
    bool removeTableColumn(QString const & table_id, int column_index);
    ColumnInfo getTableColumn(QString const & table_id, int column_index) const;

    // Built views
    bool storeBuiltTable(QString const & table_id, TableView table_view);
    std::shared_ptr<TableView> getBuiltTable(QString const & table_id) const;

    // Utilities
    QString generateUniqueTableId(QString const & base_name = "Table") const;

    // Type-aware helpers
    bool addTableColumnWithTypeInfo(QString const & table_id, ColumnInfo & column_info);
    QStringList getAvailableComputersForDataSource(QString const & row_selector_type, QString const & data_source_name) const;
    std::tuple<QString, bool, QString> getComputerTypeInfo(QString const & computer_name) const;
    ComputerInfo const * getComputerInfo(QString const & computer_name) const;
    QStringList getAvailableOutputTypes() const;

private:
    DataManager & _data_manager;
    std::shared_ptr<DataManagerExtension> _data_manager_extension;
    std::unique_ptr<ComputerRegistry> _computer_registry;

    QMap<QString, TableInfo> _table_info;
    QMap<QString, std::shared_ptr<TableView>> _table_views;
    mutable int _next_table_counter = 1;

    void notify(TableEventType type, QString const & table_id) const;
};

#endif // TABLE_REGISTRY_HPP

