#ifndef DATAMANAGER_TABLE_INFO_HPP
#define DATAMANAGER_TABLE_INFO_HPP

#include "utils/TableView/columns/ColumnTypeInfo.hpp"

#include <QString>
#include <QStringList>
#include <typeindex>
#include <map>
#include <string>

struct ColumnInfo {
    QString name;
    QString description;
    QString dataSourceName;
    QString computerName;

    ColumnTypeInfo typeInfo;

    std::type_index outputType = typeid(void);
    QString outputTypeName;
    bool isVectorType = false;
    std::type_index elementType = typeid(void);
    QString elementTypeName;

    std::map<std::string, std::string> parameters;

    ColumnInfo() = default;
    ColumnInfo(QString column_name, QString column_description = "",
               QString data_source = "", QString computer = "")
        : name(std::move(column_name)),
          description(std::move(column_description)),
          dataSourceName(std::move(data_source)),
          computerName(std::move(computer)) {}

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
    QString id;
    QString name;
    QString description;
    QString rowSourceName;
    QStringList columnNames;
    QList<ColumnInfo> columns;

    TableInfo() = default;
    TableInfo(QString table_id, QString table_name, QString table_description = "")
        : id(std::move(table_id)),
          name(std::move(table_name)),
          description(std::move(table_description)) {}
};

#endif // DATAMANAGER_TABLE_INFO_HPP

