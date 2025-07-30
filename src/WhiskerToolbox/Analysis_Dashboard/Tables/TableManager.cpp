#include "TableManager.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/core/TableView.h"

#include <QDebug>

TableManager::TableManager(std::shared_ptr<DataManager> data_manager, QObject* parent)
    : QObject(parent),
      _data_manager(std::move(data_manager)),
      _data_manager_extension(std::make_shared<DataManagerExtension>(*_data_manager)),
      _computer_registry(std::make_unique<ComputerRegistry>()) {
    
    qDebug() << "TableManager initialized";
}

TableManager::~TableManager() {
    qDebug() << "TableManager destroyed";
}

bool TableManager::createTable(const QString& table_id, const QString& table_name, const QString& table_description) {
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

bool TableManager::removeTable(const QString& table_id) {
    if (!hasTable(table_id)) {
        return false;
    }
    
    _table_info.remove(table_id);
    _table_views.remove(table_id);
    
    qDebug() << "Removed table:" << table_id;
    emit tableRemoved(table_id);
    
    return true;
}

bool TableManager::hasTable(const QString& table_id) const {
    return _table_info.contains(table_id);
}

TableManager::TableInfo TableManager::getTableInfo(const QString& table_id) const {
    auto it = _table_info.find(table_id);
    if (it != _table_info.end()) {
        return it.value();
    }
    return TableInfo(); // Return default-constructed info
}

std::vector<QString> TableManager::getTableIds() const {
    std::vector<QString> ids;
    ids.reserve(_table_info.size());
    
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        ids.push_back(it.key());
    }
    
    return ids;
}

std::vector<TableManager::TableInfo> TableManager::getAllTableInfo() const {
    std::vector<TableInfo> infos;
    infos.reserve(_table_info.size());
    
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        infos.push_back(it.value());
    }
    
    return infos;
}

std::shared_ptr<TableView> TableManager::getTableView(const QString& table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it.value();
    }
    return nullptr;
}

bool TableManager::setTableView(const QString& table_id, std::shared_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    
    _table_views[table_id] = std::move(table_view);
    
    // Update column names in table info
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        QStringList qt_column_names;
        for (const auto& name : column_names) {
            qt_column_names.append(QString::fromStdString(name));
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    
    qDebug() << "Set TableView for table:" << table_id;
    emit tableDataChanged(table_id);
    
    return true;
}

bool TableManager::updateTableInfo(const QString& table_id, const QString& table_name, const QString& table_description) {
    if (!hasTable(table_id)) {
        return false;
    }
    
    _table_info[table_id].name = table_name;
    _table_info[table_id].description = table_description;
    
    qDebug() << "Updated table info for:" << table_id;
    emit tableInfoUpdated(table_id);
    
    return true;
}

QString TableManager::generateUniqueTableId(const QString& base_name) const {
    QString candidate;
    do {
        candidate = QString("%1_%2").arg(base_name).arg(_next_table_counter);
        const_cast<TableManager*>(this)->_next_table_counter++;
    } while (hasTable(candidate));
    
    return candidate;
}
