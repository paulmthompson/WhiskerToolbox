#ifndef TABLEMANAGER_HPP
#define TABLEMANAGER_HPP

#include <QObject>
#include <QString>
#include <QMap>
#include <memory>
#include <vector>

// Forward declarations
class TableView;
class DataManager;
class ComputerRegistry;
class DataManagerExtension;

/**
 * @brief Manages user-created table views for the analysis dashboard
 * 
 * This class provides a centralized system for creating, storing, and managing
 * TableView instances that users create through the table designer interface.
 */
class TableManager : public QObject {
    Q_OBJECT

public:
    struct TableInfo {
        QString id;              ///< Unique identifier for the table
        QString name;            ///< User-friendly name for the table
        QString description;     ///< Optional description
        QString rowSourceName;   ///< Name of the data source used for rows
        QStringList columnNames; ///< Names of columns in the table
        
        TableInfo() = default;
        TableInfo(QString table_id, QString table_name, QString table_description = "")
            : id(std::move(table_id)), name(std::move(table_name)), description(std::move(table_description)) {}
    };

    explicit TableManager(std::shared_ptr<DataManager> data_manager, QObject* parent = nullptr);
    ~TableManager();

    /**
     * @brief Get the computer registry for querying available computers
     * @return Reference to the computer registry
     */
    ComputerRegistry& getComputerRegistry() { return *_computer_registry; }
    const ComputerRegistry& getComputerRegistry() const { return *_computer_registry; }

    /**
     * @brief Get the data manager extension for accessing data sources
     * @return Shared pointer to the data manager extension
     */
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() const { return _data_manager_extension; }

    /**
     * @brief Create a new table with the given ID and name
     * @param table_id Unique identifier for the table
     * @param table_name User-friendly name for the table
     * @param table_description Optional description
     * @return True if the table was created successfully, false if ID already exists
     */
    bool createTable(const QString& table_id, const QString& table_name, const QString& table_description = "");

    /**
     * @brief Remove a table by ID
     * @param table_id The ID of the table to remove
     * @return True if the table was removed, false if it didn't exist
     */
    bool removeTable(const QString& table_id);

    /**
     * @brief Check if a table with the given ID exists
     * @param table_id The table ID to check
     * @return True if the table exists
     */
    bool hasTable(const QString& table_id) const;

    /**
     * @brief Get information about a table
     * @param table_id The table ID
     * @return TableInfo structure, or default-constructed if not found
     */
    TableInfo getTableInfo(const QString& table_id) const;

    /**
     * @brief Get a list of all table IDs
     * @return Vector of table IDs
     */
    std::vector<QString> getTableIds() const;

    /**
     * @brief Get a list of all table information structures
     * @return Vector of TableInfo structures
     */
    std::vector<TableInfo> getAllTableInfo() const;

    /**
     * @brief Get a table view by ID
     * @param table_id The table ID
     * @return Shared pointer to the TableView, or nullptr if not found
     */
    std::shared_ptr<TableView> getTableView(const QString& table_id) const;

    /**
     * @brief Set the TableView instance for a table
     * @param table_id The table ID
     * @param table_view The TableView instance
     * @return True if successful, false if table doesn't exist
     */
    bool setTableView(const QString& table_id, std::shared_ptr<TableView> table_view);

    /**
     * @brief Update table metadata (name, description, etc.)
     * @param table_id The table ID
     * @param table_name New name
     * @param table_description New description
     * @return True if successful, false if table doesn't exist
     */
    bool updateTableInfo(const QString& table_id, const QString& table_name, const QString& table_description = "");

    /**
     * @brief Update the row source name for a table
     * @param table_id The table ID
     * @param row_source_name The row source name
     * @return True if successful, false if table doesn't exist
     */
    bool updateTableRowSource(const QString& table_id, const QString& row_source_name);

    /**
     * @brief Generate a unique table ID
     * @param base_name Base name for the ID generation
     * @return Unique table ID
     */
    QString generateUniqueTableId(const QString& base_name = "Table") const;

signals:
    /**
     * @brief Emitted when a new table is created
     * @param table_id The ID of the created table
     */
    void tableCreated(const QString& table_id);

    /**
     * @brief Emitted when a table is removed
     * @param table_id The ID of the removed table
     */
    void tableRemoved(const QString& table_id);

    /**
     * @brief Emitted when a table's information is updated
     * @param table_id The ID of the updated table
     */
    void tableInfoUpdated(const QString& table_id);

    /**
     * @brief Emitted when a table's data/structure changes
     * @param table_id The ID of the changed table
     */
    void tableDataChanged(const QString& table_id);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<DataManagerExtension> _data_manager_extension;
    std::unique_ptr<ComputerRegistry> _computer_registry;
    
    QMap<QString, TableInfo> _table_info;
    QMap<QString, std::shared_ptr<TableView>> _table_views;
    
    int _next_table_counter = 1;
};

#endif // TABLEMANAGER_HPP
