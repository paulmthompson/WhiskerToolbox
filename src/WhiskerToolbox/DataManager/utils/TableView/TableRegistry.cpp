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

bool TableRegistry::createTable(QString const & table_id, QString const & table_name, QString const & table_description) {
    if (hasTable(table_id)) {
        std::cout << "Table with ID already exists: " << table_id.toStdString() << std::endl;
        return false;
    }
    TableInfo info(table_id, table_name, table_description);
    _table_info[table_id] = info;
    std::cout << "Created table: " << table_id.toStdString() << std::endl;
    notify(TableEventType::Created, table_id);
    return true;
}

bool TableRegistry::removeTable(QString const & table_id) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info.remove(table_id);
    _table_views.remove(table_id);
    std::cout << "Removed table: " << table_id.toStdString() << std::endl;
    notify(TableEventType::Removed, table_id);
    return true;
}

bool TableRegistry::hasTable(QString const & table_id) const {
    return _table_info.contains(table_id);
}

TableInfo TableRegistry::getTableInfo(QString const & table_id) const {
    auto it = _table_info.find(table_id);
    if (it != _table_info.end()) {
        return it.value();
    }
    return TableInfo();
}

std::vector<QString> TableRegistry::getTableIds() const {
    std::vector<QString> ids;
    ids.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        ids.push_back(it.key());
    }
    return ids;
}

std::vector<TableInfo> TableRegistry::getAllTableInfo() const {
    std::vector<TableInfo> infos;
    infos.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        infos.push_back(it.value());
    }
    return infos;
}

bool TableRegistry::setTableView(QString const & table_id, std::shared_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_views[table_id] = std::move(table_view);
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        QStringList qt_column_names;
        for (auto const & name : column_names) {
            qt_column_names.append(QString::fromStdString(name));
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Set TableView for table: " << table_id.toStdString() << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

bool TableRegistry::updateTableInfo(QString const & table_id, QString const & table_name, QString const & table_description) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].name = table_name;
    _table_info[table_id].description = table_description;
    std::cout << "Updated table info for: " << table_id.toStdString() << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableRowSource(QString const & table_id, QString const & row_source_name) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].rowSourceName = row_source_name;
    std::cout << "Updated row source for: " << table_id.toStdString() << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::addTableColumn(QString const & table_id, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    table.columns.append(column_info);
    table.columnNames.clear();
    for (auto const & column : table.columns) {
        table.columnNames.append(column.name);
    }
    std::cout << "Added column " << column_info.name.toStdString() << " to table " << table_id.toStdString() << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableColumn(QString const & table_id, int column_index, ColumnInfo const & column_info) {
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
        table.columnNames.append(column.name);
    }
    std::cout << "Updated column index " << column_index << " in table " << table_id.toStdString() << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::removeTableColumn(QString const & table_id, int column_index) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size()) {
        return false;
    }
    QString removed = table.columns[column_index].name;
    table.columns.removeAt(column_index);
    table.columnNames.clear();
    for (auto const & column : table.columns) {
        table.columnNames.append(column.name);
    }
    std::cout << "Removed column " << removed.toStdString() << " from table " << table_id.toStdString() << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

ColumnInfo TableRegistry::getTableColumn(QString const & table_id, int column_index) const {
    if (!hasTable(table_id)) {
        return ColumnInfo();
    }
    auto const & table = _table_info.value(table_id);
    if (column_index < 0 || column_index >= table.columns.size()) {
        return ColumnInfo();
    }
    return table.columns[column_index];
}

bool TableRegistry::storeBuiltTable(QString const & table_id, TableView table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_views[table_id] = std::make_shared<TableView>(std::move(table_view));
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        QStringList qt_column_names;
        for (auto const & name : column_names) {
            qt_column_names.append(QString::fromStdString(name));
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Stored built table for: " << table_id.toStdString() << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

std::shared_ptr<TableView> TableRegistry::getBuiltTable(QString const & table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it.value();
    }
    return nullptr;
}

QString TableRegistry::generateUniqueTableId(QString const & base_name) const {
    QString candidate;
    do {
        candidate = QString("%1_%2").arg(base_name).arg(_next_table_counter);
        const_cast<TableRegistry*>(this)->_next_table_counter++;
    } while (hasTable(candidate));
    return candidate;
}

bool TableRegistry::addTableColumnWithTypeInfo(QString const & table_id, ColumnInfo & column_info) {
    if (!hasTable(table_id)) {
        std::cout << "Table does not exist: " << table_id.toStdString() << std::endl;
        return false;
    }
    auto computer_info = _computer_registry->findComputerInfo(column_info.computerName.toStdString());
    if (!computer_info) {
        std::cout << "Computer not found in registry: " << column_info.computerName.toStdString() << std::endl;
        return false;
    }
    column_info.outputType = computer_info->outputType;
    column_info.outputTypeName = QString::fromStdString(computer_info->outputTypeName);
    column_info.isVectorType = computer_info->isVectorType;
    column_info.elementType = computer_info->elementType;
    column_info.elementTypeName = QString::fromStdString(computer_info->elementTypeName);
    return addTableColumn(table_id, column_info);
}

QStringList TableRegistry::getAvailableComputersForDataSource(QString const & row_selector_type, QString const & data_source_name) const {
    QStringList computers;
    auto all_computer_names = _computer_registry->getAllComputerNames();
    for (auto const & name : all_computer_names) {
        computers.append(QString::fromStdString(name));
    }
    return computers;
}

std::tuple<QString, bool, QString> TableRegistry::getComputerTypeInfo(QString const & computer_name) const {
    auto computer_info = _computer_registry->findComputerInfo(computer_name.toStdString());
    if (!computer_info) {
        return std::make_tuple(QString("unknown"), false, QString("unknown"));
    }
    return std::make_tuple(
        QString::fromStdString(computer_info->outputTypeName),
        computer_info->isVectorType,
        QString::fromStdString(computer_info->elementTypeName)
    );
}

ComputerInfo const * TableRegistry::getComputerInfo(QString const & computer_name) const {
    return _computer_registry->findComputerInfo(computer_name.toStdString());
}

QStringList TableRegistry::getAvailableOutputTypes() const {
    auto type_names = _computer_registry->getOutputTypeNames();
    QStringList result;
    for (auto const & [type_index, name] : type_names) {
        (void)type_index;
        result.append(QString::fromStdString(name));
    }
    return result;
}

void TableRegistry::notify(TableEventType type, QString const & table_id) const {
    TableEvent ev{type, table_id.toStdString()};
    DataManager__NotifyTableObservers(_data_manager, ev);
}


