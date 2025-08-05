#include "DataSourceRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "Analysis_Dashboard/Tables/TableManager.hpp"
#include "DataManager/utils/TableView/core/TableView.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QDebug>
#include <QVector>

// ==================== DataManagerSource Implementation ====================

DataManagerSource::DataManagerSource(DataManager* data_manager, QObject* parent)
    : AbstractDataSource(parent)
    , data_manager_(data_manager)
    , data_manager_observer_id_(-1)
{
    if (data_manager_) {
        // Register a global observer for DataManager state changes (data added/removed)
        data_manager_->addObserver([this]() {
            // Emit our dataChanged signal when DataManager state changes
            emit dataChanged();
            
            // Check if availability changed
            bool current_availability = isAvailable();
            if (current_availability != last_known_availability_) {
                last_known_availability_ = current_availability;
                emit availabilityChanged(current_availability);
            }
        });
        
        // Store initial availability
        last_known_availability_ = isAvailable();
        
        qDebug() << "DataManagerSource: Created for DataManager with observer callback";
    } else {
        qWarning() << "DataManagerSource: Created with null DataManager";
    }
}

QString DataManagerSource::getName() const
{
    if (!data_manager_) {
        return "Invalid DataManager";
    }
    
    // Since DataManager doesn't have getCurrentDatasetName(), use a generic name
    // with the count of data keys for information
    auto keys = data_manager_->getAllKeys();
    return QString("DataManager (%1 datasets)").arg(keys.size());
}

bool DataManagerSource::isAvailable() const
{
    if (!data_manager_) {
        return false;
    }
    
    // Check if DataManager has any data keys
    auto keys = data_manager_->getAllKeys();
    return !keys.empty();
}

int DataManagerSource::getDataPointCount() const
{
    if (!isAvailable()) {
        return -1;
    }
    
    // DataManager doesn't have a direct getFrameCount() method
    // We'll need to examine the actual data to determine point count
    // For now, return the number of data keys as a placeholder
    auto keys = data_manager_->getAllKeys();
    return static_cast<int>(keys.size());
}

QStringList DataManagerSource::getAvailableColumns() const
{
    if (!isAvailable()) {
        return QStringList();
    }
    
    // Convert std::vector<std::string> to QStringList
    auto keys = data_manager_->getAllKeys();
    QStringList columns;
    for (const auto& key : keys) {
        columns.append(QString::fromStdString(key));
    }
    return columns;
}

QVariant DataManagerSource::getColumnData(const QString& column_name) const
{
    if (!isAvailable()) {
        return QVariant();
    }
    
    // Convert QString to std::string and get data variant
    std::string key = column_name.toStdString();
    auto data_variant = data_manager_->getDataVariant(key);
    
    if (!data_variant.has_value()) {
        qWarning() << "DataManagerSource::getColumnData: Data key not found:" << column_name;
        return QVariant();
    }
    
    // Convert the DataManager variant to QVariant
    // This is a simplified conversion - in a real implementation,
    // we'd need to handle the specific data types properly
    return QVariant::fromValue(QString("DataManager data for key: %1").arg(column_name));
}

QVariant DataManagerSource::getValue(int row, const QString& column_name) const
{
    if (!isAvailable() || row < 0) {
        return QVariant();
    }
    
    // For now, return a placeholder since accessing individual values
    // from DataManager variants requires type-specific handling
    return QVariant::fromValue(QString("Value at row %1, column %2").arg(row).arg(column_name));
}

// ==================== DataSourceRegistry Implementation ====================

DataSourceRegistry::DataSourceRegistry(QObject* parent)
    : QObject(parent)
{
    qDebug() << "DataSourceRegistry: Initialized";
}

bool DataSourceRegistry::registerDataSource(const QString& source_id, std::unique_ptr<AbstractDataSource> data_source)
{
    if (source_id.isEmpty()) {
        qWarning() << "DataSourceRegistry::registerDataSource: Empty source ID";
        return false;
    }
    
    if (!data_source) {
        qWarning() << "DataSourceRegistry::registerDataSource: Null data source for ID:" << source_id;
        return false;
    }
    
    if (data_sources_.contains(source_id)) {
        qWarning() << "DataSourceRegistry::registerDataSource: Source ID already exists:" << source_id;
        return false;
    }
    
    // Connect to the data source's signals
    connectToDataSource(data_source.get());
    
    // Store the data source
    data_sources_[source_id] = std::move(data_source);
    
    qDebug() << "DataSourceRegistry::registerDataSource: Registered" << source_id 
             << "Total sources:" << data_sources_.size();
    
    emit dataSourceRegistered(source_id);
    return true;
}

bool DataSourceRegistry::unregisterDataSource(const QString& source_id)
{
    auto it = data_sources_.find(source_id);
    if (it == data_sources_.end()) {
        qDebug() << "DataSourceRegistry::unregisterDataSource: Source not found:" << source_id;
        return false;
    }
    
    // Disconnect from the data source's signals
    disconnectFromDataSource(it->second.get());
    
    // Remove the data source
    data_sources_.erase(it);
    
    qDebug() << "DataSourceRegistry::unregisterDataSource: Unregistered" << source_id
             << "Remaining sources:" << data_sources_.size();
    
    emit dataSourceUnregistered(source_id);
    return true;
}

AbstractDataSource* DataSourceRegistry::getDataSource(const QString& source_id) const
{
    auto it = data_sources_.find(source_id);
    return (it != data_sources_.end()) ? it->second.get() : nullptr;
}

QStringList DataSourceRegistry::getRegisteredSourceIds() const
{
    QStringList ids;
    for (auto it = data_sources_.begin(); it != data_sources_.end(); ++it) {
        ids.append(it->first);
    }
    return ids;
}

QStringList DataSourceRegistry::getAvailableSourceIds() const
{
    QStringList available_ids;
    for (auto it = data_sources_.begin(); it != data_sources_.end(); ++it) {
        if (it->second && it->second->isAvailable()) {
            available_ids.append(it->first);
        }
    }
    return available_ids;
}

bool DataSourceRegistry::isSourceRegistered(const QString& source_id) const
{
    return data_sources_.find(source_id) != data_sources_.end();
}

void DataSourceRegistry::handleDataSourceChanged()
{
    AbstractDataSource* source = qobject_cast<AbstractDataSource*>(sender());
    if (!source) {
        return;
    }
    
    // Find the source ID for logging
    QString source_id;
    for (auto it = data_sources_.begin(); it != data_sources_.end(); ++it) {
        if (it->second.get() == source) {
            source_id = it->first;
            break;
        }
    }
    
    qDebug() << "DataSourceRegistry::handleDataSourceChanged: Data changed for source" << source_id;
    
    // Note: We could emit a signal here if plots need to know about specific source changes
    // For now, plots should connect directly to the data sources they're using
}

void DataSourceRegistry::handleDataSourceAvailabilityChanged(bool available)
{
    AbstractDataSource* source = qobject_cast<AbstractDataSource*>(sender());
    if (!source) {
        return;
    }
    
    // Find the source ID
    QString source_id;
    for (auto it = data_sources_.begin(); it != data_sources_.end(); ++it) {
        if (it->second.get() == source) {
            source_id = it->first;
            break;
        }
    }
    
    if (!source_id.isEmpty()) {
        qDebug() << "DataSourceRegistry::handleDataSourceAvailabilityChanged: Source" 
                 << source_id << "availability:" << available;
        emit dataSourceAvailabilityChanged(source_id, available);
    }
}

void DataSourceRegistry::connectToDataSource(AbstractDataSource* data_source)
{
    if (!data_source) {
        return;
    }
    
    connect(data_source, &AbstractDataSource::dataChanged,
            this, &DataSourceRegistry::handleDataSourceChanged);
    
    connect(data_source, &AbstractDataSource::availabilityChanged,
            this, &DataSourceRegistry::handleDataSourceAvailabilityChanged);
    
    qDebug() << "DataSourceRegistry::connectToDataSource: Connected to data source signals";
}

void DataSourceRegistry::disconnectFromDataSource(AbstractDataSource* data_source)
{
    if (!data_source) {
        return;
    }
    
    disconnect(data_source, nullptr, this, nullptr);
    qDebug() << "DataSourceRegistry::disconnectFromDataSource: Disconnected from data source signals";
}

// ==================== TableManagerSource Implementation ====================

TableManagerSource::TableManagerSource(TableManager* table_manager, const QString& name, QObject* parent)
    : AbstractDataSource(parent)
    , table_manager_(table_manager)
    , name_(name)
{
    if (table_manager_) {
        // Connect to table manager signals to update availability
        connect(table_manager_, &TableManager::tableCreated, this, [this](const QString&) {
            emit availabilityChanged(isAvailable());
        });
        connect(table_manager_, &TableManager::tableRemoved, this, [this](const QString&) {
            emit availabilityChanged(isAvailable());
        });
        connect(table_manager_, &TableManager::tableDataChanged, this, [this](const QString&) {
            emit dataChanged();
        });
    }
}

bool TableManagerSource::isAvailable() const {
    return table_manager_ != nullptr && !getAvailableTableIds().isEmpty();
}

int TableManagerSource::getDataPointCount() const {
    if (!table_manager_) {
        return 0;
    }
    
    // Return the total number of rows across all tables
    int total_rows = 0;
    auto table_ids = getAvailableTableIds();
    for (const auto& table_id : table_ids) {
        auto table_view = table_manager_->getBuiltTable(table_id);
        if (table_view) {
            total_rows += table_view->getRowCount();
        }
    }
    return total_rows;
}

QStringList TableManagerSource::getAvailableColumns() const {
    if (!table_manager_) {
        return QStringList();
    }
    
    QStringList all_columns;
    auto table_ids = getAvailableTableIds();
    
    for (const auto& table_id : table_ids) {
        auto table_view = table_manager_->getBuiltTable(table_id);
        if (table_view) {
            auto column_names = table_view->getColumnNames();
            for (const auto& name : column_names) {
                QString qualified_name = QString("%1.%2").arg(table_id, QString::fromStdString(name));
                if (!all_columns.contains(qualified_name)) {
                    all_columns.append(qualified_name);
                }
            }
        }
    }
    
    return all_columns;
}

QVariant TableManagerSource::getColumnData(const QString& column_name) const {
    // Parse qualified name: table_id.column_name
    QStringList parts = column_name.split('.');
    if (parts.size() != 2) {
        qWarning() << "TableManagerSource: Invalid column name format, expected 'table_id.column_name':" << column_name;
        return QVariant();
    }
    
    return getTableColumnData(parts[0], parts[1]);
}

QVariant TableManagerSource::getValue(int row, const QString& column_name) const {
    // This would need to be implemented based on how you want to map
    // row indices across multiple tables
    Q_UNUSED(row)
    Q_UNUSED(column_name)
    return QVariant();
}

QStringList TableManagerSource::getAvailableTableIds() const {
    if (!table_manager_) {
        return QStringList();
    }
    
    QStringList available_tables;
    auto all_table_ids = table_manager_->getTableIds();
    
    for (const auto& table_id : all_table_ids) {
        auto table_view = table_manager_->getBuiltTable(table_id);
        if (table_view) {
            available_tables.append(table_id);
        }
    }
    
    return available_tables;
}

QVariant TableManagerSource::getTableColumnData(const QString& table_id, const QString& column_name) const {
    if (!table_manager_) {
        return QVariant();
    }
    
    auto table_view = table_manager_->getBuiltTable(table_id);
    if (!table_view) {
        qWarning() << "TableManagerSource: Table not found:" << table_id;
        return QVariant();
    }
    
    // Try to get the data as different types and return as QVariant
    // First, check what type the column actually contains
    auto column_names = table_view->getColumnNames();
    std::string std_column_name = column_name.toStdString();
    
    if (std::find(column_names.begin(), column_names.end(), std_column_name) == column_names.end()) {
        qWarning() << "TableManagerSource: Column not found:" << column_name << "in table:" << table_id;
        return QVariant();
    }
    
    // Try to get as vector<float> first (common for event data)
    try {
        auto data = table_view->getColumnValues<std::vector<float>>(std_column_name);
        QVariantList result;
        for (const auto& vector : data) {
            QVariantList inner_list;
            for (float value : vector) {
                inner_list.append(value);
            }
            result.append(QVariant::fromValue(inner_list));
        }
        return QVariant::fromValue(result);
    } catch (...) {
        // Try other types
    }
    
    // Try double
    try {
        auto data = table_view->getColumnValues<double>(std_column_name);
        QVariantList result;
        for (double value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    } catch (...) {
        // Try other types
    }
    
    // Try int
    try {
        auto data = table_view->getColumnValues<int>(std_column_name);
        QVariantList result;
        for (int value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    } catch (...) {
        // Try other types
    }
    
    qWarning() << "TableManagerSource: Could not get data for column:" << column_name << "in table:" << table_id;
    return QVariant();
}

template<typename T>
std::vector<T> TableManagerSource::getTypedTableColumnData(const QString& table_id, const QString& column_name) const {
    if (!table_manager_) {
        return std::vector<T>();
    }
    
    auto table_view = table_manager_->getBuiltTable(table_id);
    if (!table_view) {
        return std::vector<T>();
    }
    
    try {
        return table_view->getColumnValues<T>(column_name.toStdString());
    } catch (...) {
        return std::vector<T>();
    }
}

// Explicit template instantiations
template std::vector<float> TableManagerSource::getTypedTableColumnData<float>(const QString&, const QString&) const;
template std::vector<double> TableManagerSource::getTypedTableColumnData<double>(const QString&, const QString&) const;
template std::vector<int> TableManagerSource::getTypedTableColumnData<int>(const QString&, const QString&) const;
template std::vector<std::vector<float>> TableManagerSource::getTypedTableColumnData<std::vector<float>>(const QString&, const QString&) const;
