#include "TableManager.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/core/TableView.h"

#include <QDebug>

TableManager::TableManager(std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QObject(parent),
      _data_manager(std::move(data_manager)),
      _data_manager_extension(std::make_shared<DataManagerExtension>(*_data_manager)),
      _computer_registry(std::make_unique<ComputerRegistry>()) {

    qDebug() << "TableManager initialized";
}

TableManager::~TableManager() {
    qDebug() << "TableManager destroyed";
}

bool TableManager::createTable(QString const & table_id, QString const & table_name, QString const & table_description) {
    if (hasTable(table_id)) {
        qDebug() << "Table with ID" << table_id << "already exists";
        return false;
    }

    TableInfo info(table_id, table_name, table_description);
    _table_info[table_id] = info;

    qDebug() << "Created table:" << table_id << "with name:" << table_name;
    emit tableCreated(table_id);

    return true;
}

bool TableManager::removeTable(QString const & table_id) {
    if (!hasTable(table_id)) {
        return false;
    }

    _table_info.remove(table_id);
    _table_views.remove(table_id);

    qDebug() << "Removed table:" << table_id;
    emit tableRemoved(table_id);

    return true;
}

bool TableManager::hasTable(QString const & table_id) const {
    return _table_info.contains(table_id);
}

TableInfo TableManager::getTableInfo(QString const & table_id) const {
    auto it = _table_info.find(table_id);
    if (it != _table_info.end()) {
        return it.value();
    }
    return TableInfo();// Return default-constructed info
}

std::vector<QString> TableManager::getTableIds() const {
    std::vector<QString> ids;
    ids.reserve(_table_info.size());

    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        ids.push_back(it.key());
    }

    return ids;
}

std::vector<TableInfo> TableManager::getAllTableInfo() const {
    std::vector<TableInfo> infos;
    infos.reserve(_table_info.size());

    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        infos.push_back(it.value());
    }

    return infos;
}

std::shared_ptr<TableView> TableManager::getTableView(QString const & table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it.value();
    }
    return nullptr;
}

bool TableManager::setTableView(QString const & table_id, std::shared_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }

    _table_views[table_id] = std::move(table_view);

    // Update column names in table info
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        QStringList qt_column_names;
        for (auto const & name: column_names) {
            qt_column_names.append(QString::fromStdString(name));
        }
        _table_info[table_id].columnNames = qt_column_names;
    }

    qDebug() << "Set TableView for table:" << table_id;
    emit tableDataChanged(table_id);

    return true;
}

bool TableManager::updateTableInfo(QString const & table_id, QString const & table_name, QString const & table_description) {
    if (!hasTable(table_id)) {
        return false;
    }

    _table_info[table_id].name = table_name;
    _table_info[table_id].description = table_description;

    qDebug() << "Updated table info for:" << table_id;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::updateTableRowSource(QString const & table_id, QString const & row_source_name) {
    if (!hasTable(table_id)) {
        return false;
    }

    _table_info[table_id].rowSourceName = row_source_name;

    qDebug() << "Updated row source for table" << table_id << "to:" << row_source_name;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::addTableColumn(QString const & table_id, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }

    auto & table = _table_info[table_id];
    table.columns.append(column_info);

    // Update the columnNames list for backward compatibility
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.append(column.name);
    }

    qDebug() << "Added column" << column_info.name << "to table" << table_id;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::updateTableColumn(QString const & table_id, int column_index, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }

    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size()) {
        return false;
    }

    table.columns[column_index] = column_info;

    // Update the columnNames list for backward compatibility
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.append(column.name);
    }

    qDebug() << "Updated column" << column_index << "in table" << table_id << "to:" << column_info.name;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::removeTableColumn(QString const & table_id, int column_index) {
    if (!hasTable(table_id)) {
        return false;
    }

    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size()) {
        return false;
    }

    QString column_name = table.columns[column_index].name;
    table.columns.removeAt(column_index);

    // Update the columnNames list for backward compatibility
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.append(column.name);
    }

    qDebug() << "Removed column" << column_name << "from table" << table_id;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::moveTableColumnUp(QString const & table_id, int column_index) {
    if (!hasTable(table_id)) {
        return false;
    }

    auto & table = _table_info[table_id];
    if (column_index <= 0 || column_index >= table.columns.size()) {
        return false;
    }

    table.columns.swapItemsAt(column_index - 1, column_index);

    // Update the columnNames list for backward compatibility
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.append(column.name);
    }

    qDebug() << "Moved column" << column_index << "up in table" << table_id;
    emit tableInfoUpdated(table_id);

    return true;
}

bool TableManager::moveTableColumnDown(QString const & table_id, int column_index) {
    if (!hasTable(table_id)) {
        return false;
    }

    auto & table = _table_info[table_id];
    if (column_index < 0 || column_index >= table.columns.size() - 1) {
        return false;
    }

    table.columns.swapItemsAt(column_index, column_index + 1);

    // Update the columnNames list for backward compatibility
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.append(column.name);
    }

    qDebug() << "Moved column" << column_index << "down in table" << table_id;
    emit tableInfoUpdated(table_id);

    return true;
}

ColumnInfo TableManager::getTableColumn(QString const & table_id, int column_index) const {
    if (!hasTable(table_id)) {
        return ColumnInfo();
    }

    auto const & table = _table_info.value(table_id);
    if (column_index < 0 || column_index >= table.columns.size()) {
        return ColumnInfo();
    }

    return table.columns[column_index];
}

bool TableManager::storeBuiltTable(QString const & table_id, TableView table_view) {
    if (!hasTable(table_id)) {
        return false;
    }

    _table_views[table_id] = std::make_shared<TableView>(std::move(table_view));

    qDebug() << "Stored built table for:" << table_id;
    emit tableDataChanged(table_id);

    return true;
}

std::shared_ptr<TableView> TableManager::getBuiltTable(QString const & table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it.value();
    }
    return nullptr;
}

QString TableManager::generateUniqueTableId(QString const & base_name) const {
    QString candidate;
    do {
        candidate = QString("%1_%2").arg(base_name).arg(_next_table_counter);
        const_cast<TableManager *>(this)->_next_table_counter++;
    } while (hasTable(candidate));

    return candidate;
}

bool TableManager::addTableColumnWithTypeInfo(QString const & table_id, ColumnInfo & column_info) {
    if (!hasTable(table_id)) {
        qDebug() << "Table does not exist:" << table_id;
        return false;
    }

    // Get type information from the computer registry
    auto computer_info = _computer_registry->findComputerInfo(column_info.computerName.toStdString());
    if (!computer_info) {
        qDebug() << "Computer not found in registry:" << column_info.computerName;
        return false;
    }

    // Populate type information
    column_info.outputType = computer_info->outputType;
    column_info.outputTypeName = QString::fromStdString(computer_info->outputTypeName);
    column_info.isVectorType = computer_info->isVectorType;
    column_info.elementType = computer_info->elementType;
    column_info.elementTypeName = QString::fromStdString(computer_info->elementTypeName);

    // Add the column with enhanced information
    return addTableColumn(table_id, column_info);
}

QStringList TableManager::getAvailableComputersForDataSource(QString const & row_selector_type, QString const & data_source_name) const {
    QStringList computers;
    
    // This would need to be implemented based on your specific data source resolution logic
    // For now, return all computer names - you might want to filter based on actual data source compatibility
    auto all_computer_names = _computer_registry->getAllComputerNames();
    
    for (auto const& name : all_computer_names) {
        computers.append(QString::fromStdString(name));
    }
    
    return computers;
}

std::tuple<QString, bool, QString> TableManager::getComputerTypeInfo(QString const & computer_name) const {
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

QStringList TableManager::getAvailableOutputTypes() const {
    auto type_names = _computer_registry->getOutputTypeNames();
    
    QStringList result;
    for (auto const& [type_index, name] : type_names) {
        result.append(QString::fromStdString(name));
    }
    
    return result;
}
