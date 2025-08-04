#ifndef TABLE_INFO_HPP
#define TABLE_INFO_HPP

#include <QString>
#include <QStringList>
#include <typeindex>

struct ColumnInfo {
    QString name;                ///< User-friendly name for the column
    QString description;         ///< Optional description
    QString dataSourceName;      ///< Name/ID of the data source (e.g., "analog:LFP", "events:Spikes")
    QString computerName;        ///< Name of the computer to use
    
    // Enhanced type information
    std::type_index outputType = typeid(void);     ///< Runtime type of the column output
    QString outputTypeName;                        ///< Human-readable name of the output type
    bool isVectorType = false;                     ///< True if output is std::vector<T>
    std::type_index elementType = typeid(void);    ///< For vector types, the element type
    QString elementTypeName;                       ///< Human-readable name of the element type

    ColumnInfo() = default;
    ColumnInfo(QString column_name, QString column_description = "",
               QString data_source = "", QString computer = "")
        : name(std::move(column_name)),
          description(std::move(column_description)),
          dataSourceName(std::move(data_source)),
          computerName(std::move(computer)) {}
          
    // Enhanced constructor with type information
    ColumnInfo(QString column_name, QString column_description,
               QString data_source, QString computer,
               std::type_index output_type, QString output_type_name,
               bool is_vector_type = false,
               std::type_index element_type = typeid(void), QString element_type_name = "")
        : name(std::move(column_name)),
          description(std::move(column_description)),
          dataSourceName(std::move(data_source)),
          computerName(std::move(computer)),
          outputType(output_type),
          outputTypeName(std::move(output_type_name)),
          isVectorType(is_vector_type),
          elementType(element_type),
          elementTypeName(std::move(element_type_name)) {}
};

struct TableInfo {
    QString id;               ///< Unique identifier for the table
    QString name;             ///< User-friendly name for the table
    QString description;      ///< Optional description
    QString rowSourceName;    ///< Name of the data source used for rows
    QStringList columnNames;  ///< Names of columns in the table (for backward compatibility)
    QList<ColumnInfo> columns;///< Detailed column configurations

    TableInfo() = default;
    TableInfo(QString table_id, QString table_name, QString table_description = "")
        : id(std::move(table_id)),
          name(std::move(table_name)),
          description(std::move(table_description)) {}
};

#endif// TABLE_INFO_HPP