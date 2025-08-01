#include "DataSourceRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <QTableView>
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

// ==================== TableViewSource Implementation ====================

TableViewSource::TableViewSource(QTableView* table_view, const QString& name, QObject* parent)
    : AbstractDataSource(parent)
    , table_view_(table_view)
    , name_(name)
{
    if (table_view_ && table_view_->model()) {
        // Connect to model signals for change notifications
        connect(table_view_->model(), &QAbstractItemModel::dataChanged,
                this, &AbstractDataSource::dataChanged);
        connect(table_view_->model(), &QAbstractItemModel::modelReset,
                this, &AbstractDataSource::dataChanged);
        connect(table_view_->model(), &QAbstractItemModel::rowsInserted,
                this, &AbstractDataSource::dataChanged);
        connect(table_view_->model(), &QAbstractItemModel::rowsRemoved,
                this, &AbstractDataSource::dataChanged);
        
        qDebug() << "TableViewSource: Created for table" << name_;
    } else {
        qWarning() << "TableViewSource: Created with invalid table view";
    }
}

bool TableViewSource::isAvailable() const
{
    return table_view_ && table_view_->model() && 
           table_view_->model()->rowCount() > 0 && 
           table_view_->model()->columnCount() > 0;
}

int TableViewSource::getDataPointCount() const
{
    if (!isAvailable()) {
        return -1;
    }
    
    return table_view_->model()->rowCount();
}

QStringList TableViewSource::getAvailableColumns() const
{
    if (!isAvailable()) {
        return QStringList();
    }
    
    QStringList columns;
    QAbstractItemModel* model = table_view_->model();
    
    for (int col = 0; col < model->columnCount(); ++col) {
        QString header = model->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
        if (header.isEmpty()) {
            header = QString("Column_%1").arg(col);
        }
        columns.append(header);
    }
    
    return columns;
}

QVariant TableViewSource::getColumnData(const QString& column_name) const
{
    if (!isAvailable()) {
        return QVariant();
    }
    
    QAbstractItemModel* model = table_view_->model();
    QStringList columns = getAvailableColumns();
    
    int col_index = columns.indexOf(column_name);
    if (col_index < 0) {
        qWarning() << "TableViewSource::getColumnData: Column not found:" << column_name;
        return QVariant();
    }
    
    // Extract all values for this column
    QVector<QVariant> column_data;
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex index = model->index(row, col_index);
        column_data.append(model->data(index, Qt::DisplayRole));
    }
    
    return QVariant::fromValue(column_data);
}

QVariant TableViewSource::getValue(int row, const QString& column_name) const
{
    if (!isAvailable() || row < 0) {
        return QVariant();
    }
    
    QStringList columns = getAvailableColumns();
    int col_index = columns.indexOf(column_name);
    
    if (col_index < 0 || row >= table_view_->model()->rowCount()) {
        return QVariant();
    }
    
    QModelIndex index = table_view_->model()->index(row, col_index);
    return table_view_->model()->data(index, Qt::DisplayRole);
}

QAbstractItemModel* TableViewSource::getModel() const
{
    return table_view_ ? table_view_->model() : nullptr;
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
