#ifndef DATASOURCEREGISTRY_HPP
#define DATASOURCEREGISTRY_HPP

#include "Analysis_Dashboard/Tables/ColumnTypeInfo.hpp"
#include "DataManager/DataManager.hpp"

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QVariant>

#include <map>
#include <memory>

class TableManager;
class TableView;


/**
 * @brief Abstract interface for data sources that can provide data to plots
 * 
 * This interface allows plots to access data from different sources 
 * (DataManager, TableView, etc.) through a unified API, reducing coupling
 * between plot widgets and specific data source implementations.
 */
class AbstractDataSource : public QObject {
    Q_OBJECT

public:
    explicit AbstractDataSource(QObject * parent = nullptr)
        : QObject(parent) {}
    virtual ~AbstractDataSource() = default;

    /**
     * @brief Get the display name for this data source
     * @return Human-readable name of the data source
     */
    virtual QString getName() const = 0;

    /**
     * @brief Get the type of this data source (e.g., "DataManager", "TableView")
     * @return String identifying the data source type
     */
    virtual QString getType() const = 0;

    /**
     * @brief Check if this data source is currently available/valid
     * @return True if the data source can provide data
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get list of available column/field names
     * @return List of column names that can be accessed
     */
    virtual QStringList getAvailableColumns() const = 0;

    /**
     * @brief Get data for a specific column
     * @param column_name Name of the column to retrieve
     * @return QVariant containing the column data (usually QVector<double> or similar)
     */
    virtual QVariant getColumnData(QString const & column_name) const = 0;

    /**
     * @brief Get a value at a specific row and column
     * @param row The row index
     * @param column_name The column name
     * @return The value at the specified location
     */
    virtual QVariant getValue(int row, QString const & column_name) const = 0;

    /**
     * @brief Get the underlying model if available (for table views)
     * @return Pointer to QAbstractItemModel, or nullptr if not applicable
     */
    virtual QAbstractItemModel * getModel() const { return nullptr; }

signals:
    /**
     * @brief Emitted when the data source's data has changed
     */
    void dataChanged();

    /**
     * @brief Emitted when the data source becomes available/unavailable
     * @param available True if now available, false if unavailable
     */
    void availabilityChanged(bool available);
};

/**
 * @brief DataManager-based data source implementation
 */
class DataManagerSource : public AbstractDataSource {
    Q_OBJECT

public:
    explicit DataManagerSource(DataManager * data_manager, QObject * parent = nullptr);
    ~DataManagerSource() override = default;

    QString getName() const override;
    QString getType() const override { return "DataManager"; }
    bool isAvailable() const override;
    QStringList getAvailableColumns() const override;
    QVariant getColumnData(QString const & column_name) const override;
    QVariant getValue(int row, QString const & column_name) const override;

    /**
     * @brief Get the underlying DataManager pointer for backwards compatibility
     * @return Pointer to the wrapped DataManager, or nullptr if invalid
     */
    DataManager * getDataManager() const { return data_manager_; }


private:
    DataManager * data_manager_;
    int data_manager_observer_id_;
    bool last_known_availability_ = false;
};

/**
 * @brief TableManager-based data source implementation
 * 
 * This provides access to TableViews managed by TableManager through
 * the data source registry interface.
 */
class TableManagerSource : public AbstractDataSource {
    Q_OBJECT

public:
    explicit TableManagerSource(TableManager * table_manager, QString const & name, QObject * parent = nullptr);
    ~TableManagerSource() override = default;

    QString getName() const override { return name_; }
    QString getType() const override { return "TableManager"; }
    bool isAvailable() const override;
    QStringList getAvailableColumns() const override;
    QVariant getColumnData(QString const & column_name) const override;
    QVariant getValue(int row, QString const & column_name) const override;
    QAbstractItemModel * getModel() const override { return nullptr; }// Not applicable

    /**
     * @brief Get type information for a column in a specific table
     * @param table_id The ID of the table
     * @param column_name The name of the column
     * @return ColumnTypeInfo describing the column's type characteristics
     */
    ColumnTypeInfo getColumnTypeInfo(QString const & table_id, QString const & column_name) const;

    /**
     * @brief Get the runtime type index for a column in a specific table
     * @param table_id The ID of the table
     * @param column_name The name of the column
     * @return std::type_index for the column, or typeid(void) if not found
     */
    std::type_index getColumnTypeIndex(QString const & table_id, QString const & column_name) const;

    /**
     * @brief Check if a column contains data that could be used for plotting
     * @param table_id The ID of the table
     * @param column_name The name of the column
     * @return True if the column contains numeric vector data suitable for plotting
     */
    bool isColumnNumericVector(QString const & table_id, QString const & column_name) const;

    /**
     * @brief Get the underlying TableManager pointer
     * @return Pointer to the wrapped TableManager, or nullptr if invalid
     */
    TableManager * getTableManager() const { return table_manager_; }

    /**
     * @brief Get list of available table IDs
     * @return List of table IDs that can be accessed
     */
    QStringList getAvailableTableIds() const;

    /**
     * @brief Get column data from a specific table
     * @param table_id The ID of the table
     * @param column_name The name of the column
     * @return Column data as QVariant
     */
    QVariant getTableColumnData(QString const & table_id, QString const & column_name) const;

    /**
     * @brief Get typed column data from a specific table
     * @tparam T The expected data type
     * @param table_id The ID of the table
     * @param column_name The name of the column
     * @return Vector of typed data
     */
    template<typename T>
    std::vector<T> getTypedTableColumnData(QString const & table_id, QString const & column_name) const;

private:
    TableManager * table_manager_;
    QString name_;
};

/**
 * @brief Registry for managing multiple data sources
 * 
 * This class maintains a collection of data sources and provides
 * a unified interface for plots to discover and access available data.
 */
class DataSourceRegistry : public QObject {
    Q_OBJECT

public:
    explicit DataSourceRegistry(QObject * parent = nullptr);
    ~DataSourceRegistry() override = default;

    /**
     * @brief Register a data source with a unique ID
     * @param source_id Unique identifier for the data source
     * @param data_source The data source to register
     * @return True if successfully registered, false if ID already exists
     */
    bool registerDataSource(QString const & source_id, std::unique_ptr<AbstractDataSource> data_source);

    /**
     * @brief Unregister a data source
     * @param source_id The ID of the data source to remove
     * @return True if successfully removed, false if not found
     */
    bool unregisterDataSource(QString const & source_id);

    /**
     * @brief Get a data source by ID
     * @param source_id The ID of the data source
     * @return Pointer to the data source, or nullptr if not found
     */
    AbstractDataSource * getDataSource(QString const & source_id) const;

    /**
     * @brief Get list of all registered data source IDs
     * @return List of data source IDs
     */
    QStringList getRegisteredSourceIds() const;

    /**
     * @brief Get list of all available (valid) data source IDs
     * @return List of data source IDs that are currently available
     */
    QStringList getAvailableSourceIds() const;

    /**
     * @brief Get the number of registered data sources
     * @return Total number of registered data sources
     */
    int getSourceCount() const { return data_sources_.size(); }

    /**
     * @brief Check if a data source is registered
     * @param source_id The data source ID to check
     * @return True if registered, false otherwise
     */
    bool isSourceRegistered(QString const & source_id) const;

    /**
     * @brief Get typed data from the primary DataManager source
     * @tparam T The data type to retrieve (e.g., PointData, LineData)
     * @param key The data key
     * @return Shared pointer to the data, or nullptr if not found
     */
    template<typename T>
    std::shared_ptr<T> getData(std::string const & key) const {
        AbstractDataSource * primary_source = getDataSource("primary_data_manager");
        if (!primary_source || primary_source->getType() != "DataManager") {
            return nullptr;
        }

        DataManagerSource * dm_source = static_cast<DataManagerSource *>(primary_source);
        DataManager * data_manager = dm_source->getDataManager();

        if (!data_manager) {
            return nullptr;
        }

        return data_manager->getData<T>(key);
    }

signals:
    /**
     * @brief Emitted when a new data source is registered
     * @param source_id The ID of the registered data source
     */
    void dataSourceRegistered(QString const & source_id);

    /**
     * @brief Emitted when a data source is unregistered
     * @param source_id The ID of the unregistered data source
     */
    void dataSourceUnregistered(QString const & source_id);

    /**
     * @brief Emitted when a data source's availability changes
     * @param source_id The ID of the data source
     * @param available True if now available, false if unavailable
     */
    void dataSourceAvailabilityChanged(QString const & source_id, bool available);

private slots:
    /**
     * @brief Handle data changes from registered data sources
     */
    void handleDataSourceChanged();

    /**
     * @brief Handle availability changes from registered data sources
     * @param available The new availability state
     */
    void handleDataSourceAvailabilityChanged(bool available);

private:
    std::map<QString, std::unique_ptr<AbstractDataSource>> data_sources_;

    /**
     * @brief Connect to a data source's signals
     * @param data_source The data source to connect
     */
    void connectToDataSource(AbstractDataSource * data_source);

    /**
     * @brief Disconnect from a data source's signals
     * @param data_source The data source to disconnect
     */
    void disconnectFromDataSource(AbstractDataSource * data_source);
};

#endif// DATASOURCEREGISTRY_HPP
