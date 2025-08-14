#include "TableRegistry.hpp"
#include "DataManager.hpp"
#include "TableObserverBridge.hpp"

#include <iostream>

TableRegistry::TableRegistry(DataManager & data_manager)
    : _data_manager(data_manager),
      _data_manager_extension(std::make_shared<DataManagerExtension>(data_manager)),
      _computer_registry(std::make_unique<ComputerRegistry>()) {
    std::cout << "TableRegistry initialized" << std::endl;
}

TableRegistry::~TableRegistry() {
    std::cout << "TableRegistry destroyed" << std::endl;
}

bool TableRegistry::createTable(std::string const & table_id, std::string const & table_name, std::string const & table_description) {
    if (hasTable(table_id)) {
        std::cout << "Table with ID already exists: " << table_id << std::endl;
        return false;
    }
    TableInfo info(table_id, table_name, table_description);
    _table_info[table_id] = info;
    std::cout << "Created table: " << table_id << std::endl;
    notify(TableEventType::Created, table_id);
    return true;
}

bool TableRegistry::removeTable(std::string const & table_id) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info.erase(table_id);
    _table_views.erase(table_id);
    std::cout << "Removed table: " << table_id << std::endl;
    notify(TableEventType::Removed, table_id);
    return true;
}

bool TableRegistry::hasTable(std::string const & table_id) const {
    return _table_info.find(table_id) != _table_info.end();
}

TableInfo TableRegistry::getTableInfo(std::string const & table_id) const {
    auto it = _table_info.find(table_id);
    if (it != _table_info.end()) {
        return it->second;
    }
    return TableInfo();
}

std::vector<std::string> TableRegistry::getTableIds() const {
    std::vector<std::string> ids;
    ids.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        ids.push_back(it->first);
    }
    return ids;
}

std::vector<TableInfo> TableRegistry::getAllTableInfo() const {
    std::vector<TableInfo> infos;
    infos.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        infos.push_back(it->second);
    }
    return infos;
}

bool TableRegistry::setTableView(std::string const & table_id, std::shared_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_views[table_id] = std::move(table_view);
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        std::vector<std::string> qt_column_names;
        for (auto const & name : column_names) {
            qt_column_names.push_back(name);
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Set TableView for table: " << table_id << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

bool TableRegistry::updateTableInfo(std::string const & table_id, std::string const & table_name, std::string const & table_description) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].name = table_name;
    _table_info[table_id].description = table_description;
    std::cout << "Updated table info for: " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableRowSource(std::string const & table_id, std::string const & row_source_name) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].rowSourceName = row_source_name;
    std::cout << "Updated row source for: " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::addTableColumn(std::string const & table_id, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    table.columns.push_back(column_info);
    table.columnNames.clear();
    for (auto const & column : table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Added column " << column_info.name << " to table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableColumn(std::string const & table_id, int column_index, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size()) {
        return false;
    }
    table.columns[column_index] = column_info;
    table.columnNames.clear();
    for (auto const & column : table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Updated column index " << column_index << " in table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::removeTableColumn(std::string const & table_id, int column_index) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size()) {
        return false;
    }
    std::string removed = table.columns[column_index].name;
    table.columns.erase(table.columns.begin() + column_index);
    table.columnNames.clear();
    for (auto const & column : table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Removed column " << removed << " from table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

ColumnInfo TableRegistry::getTableColumn(std::string const & table_id, int column_index) const {
    if (!hasTable(table_id)) {
        return ColumnInfo();
    }
    auto const & table = _table_info.at(table_id);
    if (column_index < 0 || column_index >= table.columns.size()) {
        return ColumnInfo();
    }
    return table.columns[column_index];
}

bool TableRegistry::storeBuiltTable(std::string const & table_id, TableView table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_views[table_id] = std::make_shared<TableView>(std::move(table_view));
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        std::vector<std::string> qt_column_names;
        for (auto const & name : column_names) {
            qt_column_names.push_back(name);
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Stored built table for: " << table_id << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

std::shared_ptr<TableView> TableRegistry::getBuiltTable(std::string const & table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it->second;
    }
    return nullptr;
}

std::string TableRegistry::generateUniqueTableId(std::string const & base_name) const {
    std::string candidate;
    while (true) {
        candidate = base_name + "_" + std::to_string(_next_table_counter);
        const_cast<TableRegistry*>(this)->_next_table_counter++;
        if (!hasTable(candidate)) {
            break;
        }
    }
    return candidate;
}

bool TableRegistry::addTableColumnWithTypeInfo(std::string const & table_id, ColumnInfo & column_info) {
    if (!hasTable(table_id)) {
        std::cout << "Table does not exist: " << table_id<< std::endl;
        return false;
    }
    auto computer_info = _computer_registry->findComputerInfo(column_info.computerName);
    if (!computer_info) {
        std::cout << "Computer not found in registry: " << column_info.computerName << std::endl;
        return false;
    }
    column_info.outputType = computer_info->outputType;
    column_info.outputTypeName = computer_info->outputTypeName;
    column_info.isVectorType = computer_info->isVectorType;
    column_info.elementType = computer_info->elementType;
    column_info.elementTypeName = computer_info->elementTypeName;
    return addTableColumn(table_id, column_info);
}

std::vector<std::string> TableRegistry::getAvailableComputersForDataSource(std::string const & row_selector_type, std::string const & data_source_name) const {
    std::vector<std::string> computers;
    auto all_computer_names = _computer_registry->getAllComputerNames();
    for (auto const & name : all_computer_names) {
        computers.push_back(name);
    }
    return computers;
}

std::tuple<std::string, bool, std::string> TableRegistry::getComputerTypeInfo(std::string const & computer_name) const {
    auto computer_info = _computer_registry->findComputerInfo(computer_name);
    if (!computer_info) {
        return std::make_tuple(std::string("unknown"), false, std::string("unknown"));
    }
    return std::make_tuple(
        computer_info->outputTypeName,
        computer_info->isVectorType,
        computer_info->elementTypeName
    );
}

ComputerInfo const * TableRegistry::getComputerInfo(std::string const & computer_name) const {
    return _computer_registry->findComputerInfo(computer_name);
}

std::vector<std::string> TableRegistry::getAvailableOutputTypes() const {
    auto type_names = _computer_registry->getOutputTypeNames();
    std::vector<std::string> result;
    for (auto const & [type_index, name] : type_names) {
        (void)type_index;
        result.push_back(name);
    }
    return result;
}

void TableRegistry::notify(TableEventType type, std::string const & table_id) const {
    TableEvent ev{type, table_id};
    DataManager__NotifyTableObservers(_data_manager, ev);
}


