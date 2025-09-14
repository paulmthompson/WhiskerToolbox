#ifndef DATAMANAGER_TABLE_INFO_HPP
#define DATAMANAGER_TABLE_INFO_HPP

#include "utils/TableView/columns/ColumnTypeInfo.hpp"

#include <map>
#include <string>
#include <typeindex>
#include <vector>

struct ColumnInfo {
    std::string name;
    std::string description;
    std::string dataSourceName;
    std::string computerName;

    ColumnTypeInfo typeInfo;

    std::type_index outputType = typeid(void);
    std::string outputTypeName;
    bool isVectorType = false;
    std::type_index elementType = typeid(void);
    std::string elementTypeName;

    std::map<std::string, std::string> parameters;

    ColumnInfo() = default;
    ColumnInfo(std::string column_name, std::string column_description = "",
               std::string data_source = "", std::string computer = "")
        : name(std::move(column_name)),
          description(std::move(column_description)),
          dataSourceName(std::move(data_source)),
          computerName(std::move(computer)) {}

    ColumnInfo(std::string column_name, std::string column_description,
               std::string data_source, std::string computer,
               std::type_index output_type, std::string output_type_name,
               bool is_vector_type = false,
               std::type_index element_type = typeid(void), std::string element_type_name = "")
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
    std::string id;
    std::string name;
    std::string description;
    std::string rowSourceName;
    std::vector<std::string> columnNames;
    std::vector<ColumnInfo> columns;

    TableInfo() = default;
    TableInfo(std::string table_id, std::string table_name, std::string table_description = "")
        : id(std::move(table_id)),
          name(std::move(table_name)),
          description(std::move(table_description)) {}
};

#endif// DATAMANAGER_TABLE_INFO_HPP
