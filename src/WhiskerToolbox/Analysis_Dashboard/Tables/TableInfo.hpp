#ifndef TABLE_INFO_HPP
#define TABLE_INFO_HPP

#include <QString>
#include <QStringList>

struct ColumnInfo {
        QString name;              ///< User-friendly name for the column
        QString description;       ///< Optional description
        QString dataSourceName;    ///< Name/ID of the data source (e.g., "analog:LFP", "events:Spikes")
        QString computerName;      ///< Name of the computer to use
        
        ColumnInfo() = default;
        ColumnInfo(QString column_name, QString column_description = "", 
                   QString data_source = "", QString computer = "")
            : name(std::move(column_name)), description(std::move(column_description)),
              dataSourceName(std::move(data_source)), computerName(std::move(computer)) {}
    };

    struct TableInfo {
        QString id;                        ///< Unique identifier for the table
        QString name;                      ///< User-friendly name for the table
        QString description;               ///< Optional description
        QString rowSourceName;             ///< Name of the data source used for rows
        QStringList columnNames;           ///< Names of columns in the table (for backward compatibility)
        QList<ColumnInfo> columns;         ///< Detailed column configurations
        
        TableInfo() = default;
        TableInfo(QString table_id, QString table_name, QString table_description = "")
            : id(std::move(table_id)), name(std::move(table_name)), description(std::move(table_description)) {}
    };

#endif // TABLE_INFO_HPP