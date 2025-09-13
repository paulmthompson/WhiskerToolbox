#include "TableRegistry.hpp"

#include "DataManager.hpp"
#include "TableObserverBridge.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"

#include <iostream>

TableRegistry::TableRegistry(DataManager & data_manager)
    : _data_manager(data_manager),
      _data_manager_extension(std::make_shared<DataManagerExtension>(data_manager)),
      _computer_registry(std::make_unique<ComputerRegistry>()) {
    // std::cout << "TableRegistry initialized" << std::endl;
}

TableRegistry::~TableRegistry() {
    // std::cout << "TableRegistry destroyed" << std::endl;
}

bool TableRegistry::createTable(std::string const & table_id, std::string const & table_name, std::string const & table_description) {
    if (hasTable(table_id)) {
        std::cout << "Table with ID already exists: " << table_id << std::endl;
        return false;
    }
    TableInfo info(table_id, table_name, table_description);
    _table_info[table_id] = info;
    std::cout << "Created table: " << table_id << std::endl;
    notify(TableEventType::Created, table_id);
    return true;
}

bool TableRegistry::removeTable(std::string const & table_id) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info.erase(table_id);
    _table_views.erase(table_id);
    std::cout << "Removed table: " << table_id << std::endl;
    notify(TableEventType::Removed, table_id);
    return true;
}

bool TableRegistry::hasTable(std::string const & table_id) const {
    return _table_info.find(table_id) != _table_info.end();
}

TableInfo TableRegistry::getTableInfo(std::string const & table_id) const {
    auto it = _table_info.find(table_id);
    if (it != _table_info.end()) {
        return it->second;
    }
    return TableInfo();
}

std::vector<std::string> TableRegistry::getTableIds() const {
    std::vector<std::string> ids;
    ids.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        ids.push_back(it->first);
    }
    return ids;
}

std::vector<TableInfo> TableRegistry::getAllTableInfo() const {
    std::vector<TableInfo> infos;
    infos.reserve(_table_info.size());
    for (auto it = _table_info.begin(); it != _table_info.end(); ++it) {
        infos.push_back(it->second);
    }
    return infos;
}

bool TableRegistry::setTableView(std::string const & table_id, std::shared_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_views[table_id] = std::move(table_view);
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        std::vector<std::string> qt_column_names;
        for (auto const & name: column_names) {
            qt_column_names.push_back(name);
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Set TableView for table: " << table_id << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

bool TableRegistry::updateTableInfo(std::string const & table_id, std::string const & table_name, std::string const & table_description) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].name = table_name;
    _table_info[table_id].description = table_description;
    std::cout << "Updated table info for: " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableRowSource(std::string const & table_id, std::string const & row_source_name) {
    if (!hasTable(table_id)) {
        return false;
    }
    _table_info[table_id].rowSourceName = row_source_name;
    std::cout << "Updated row source for: " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::addTableColumn(std::string const & table_id, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    table.columns.push_back(column_info);
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Added column " << column_info.name << " to table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::updateTableColumn(std::string const & table_id, size_t column_index, ColumnInfo const & column_info) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    if (column_index >= table.columns.size()) {
        return false;
    }
    table.columns[column_index] = column_info;
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Updated column index " << column_index << " in table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

bool TableRegistry::removeTableColumn(std::string const & table_id, size_t column_index) {
    if (!hasTable(table_id)) {
        return false;
    }
    auto & table = _table_info[table_id];
    if (column_index >= table.columns.size()) {
        return false;
    }
    std::string removed = table.columns[column_index].name;
    table.columns.erase(table.columns.begin() + static_cast<long int>(column_index));
    table.columnNames.clear();
    for (auto const & column: table.columns) {
        table.columnNames.push_back(column.name);
    }
    std::cout << "Removed column " << removed << " from table " << table_id << std::endl;
    notify(TableEventType::InfoUpdated, table_id);
    return true;
}

ColumnInfo TableRegistry::getTableColumn(std::string const & table_id, size_t column_index) const {
    if (!hasTable(table_id)) {
        return ColumnInfo();
    }
    auto const & table = _table_info.at(table_id);
    if (column_index >= table.columns.size()) {
        return ColumnInfo();
    }
    return table.columns[column_index];
}

bool TableRegistry::storeBuiltTable(std::string const & table_id, std::unique_ptr<TableView> table_view) {
    if (!hasTable(table_id)) {
        return false;
    }
    if (!table_view) {
        return false;
    }
    _table_views[table_id] = std::shared_ptr<TableView>(table_view.release());
    if (auto view = _table_views[table_id]) {
        auto column_names = view->getColumnNames();
        std::vector<std::string> qt_column_names;
        for (auto const & name: column_names) {
            qt_column_names.push_back(name);
        }
        _table_info[table_id].columnNames = qt_column_names;
    }
    std::cout << "Stored built table for: " << table_id << std::endl;
    notify(TableEventType::DataChanged, table_id);
    return true;
}

std::shared_ptr<TableView> TableRegistry::getBuiltTable(std::string const & table_id) const {
    auto it = _table_views.find(table_id);
    if (it != _table_views.end()) {
        return it->second;
    }
    return nullptr;
}

std::string TableRegistry::generateUniqueTableId(std::string const & base_name) const {
    std::string candidate;
    while (true) {
        candidate = base_name + "_" + std::to_string(_next_table_counter);
        const_cast<TableRegistry *>(this)->_next_table_counter++;
        if (!hasTable(candidate)) {
            break;
        }
    }
    return candidate;
}

bool TableRegistry::addTableColumnWithTypeInfo(std::string const & table_id, ColumnInfo & column_info) {
    if (!hasTable(table_id)) {
        std::cout << "Table does not exist: " << table_id << std::endl;
        return false;
    }
    auto computer_info = _computer_registry->findComputerInfo(column_info.computerName);
    if (!computer_info) {
        std::cout << "Computer not found in registry: " << column_info.computerName << std::endl;
        return false;
    }
    column_info.outputType = computer_info->outputType;
    column_info.outputTypeName = computer_info->outputTypeName;
    column_info.isVectorType = computer_info->isVectorType;
    column_info.elementType = computer_info->elementType;
    column_info.elementTypeName = computer_info->elementTypeName;
    return addTableColumn(table_id, column_info);
}

std::vector<std::string> TableRegistry::getAvailableComputersForDataSource(std::string const & /*row_selector_type*/, std::string const & /*data_source_name*/) const {
    std::vector<std::string> computers;
    auto all_computer_names = _computer_registry->getAllComputerNames();
    for (auto const & name: all_computer_names) {
        computers.push_back(name);
    }
    return computers;
}

std::tuple<std::string, bool, std::string> TableRegistry::getComputerTypeInfo(std::string const & computer_name) const {
    auto computer_info = _computer_registry->findComputerInfo(computer_name);
    if (!computer_info) {
        return std::make_tuple(std::string("unknown"), false, std::string("unknown"));
    }
    return std::make_tuple(
            computer_info->outputTypeName,
            computer_info->isVectorType,
            computer_info->elementTypeName);
}

ComputerInfo const * TableRegistry::getComputerInfo(std::string const & computer_name) const {
    return _computer_registry->findComputerInfo(computer_name);
}

std::vector<std::string> TableRegistry::getAvailableOutputTypes() const {
    auto type_names = _computer_registry->getOutputTypeNames();
    std::vector<std::string> result;
    for (auto const & [type_index, name]: type_names) {
        (void) type_index;
        result.push_back(name);
    }
    return result;
}

bool TableRegistry::addColumnToBuilder(TableViewBuilder & builder, ColumnInfo const & column_info) const {
    if (column_info.dataSourceName.empty() || column_info.computerName.empty()) {
        std::cout << "Column " << column_info.name << " missing data source or computer configuration" << std::endl;
        return false;
    }

    try {
        // Create DataSourceVariant from column data source
        DataSourceVariant data_source_variant;
        bool valid_source = false;

        std::string column_source = column_info.dataSourceName;

        if (column_source.starts_with("analog:")) {
            std::string source_name = column_source.substr(7);// Remove "analog:" prefix
            auto analog_source = _data_manager_extension->getAnalogSource(source_name);
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.starts_with("events:")) {
            std::string source_name = column_source.substr(7);// Remove "events:" prefix
            auto event_source = _data_manager_extension->getEventSource(source_name);
            if (event_source) {
                data_source_variant = event_source;
                valid_source = true;
            }
        } else if (column_source.starts_with("intervals:")) {
            std::string source_name = column_source.substr(10);// Remove "intervals:" prefix
            auto interval_source = _data_manager_extension->getIntervalSource(source_name);
            if (interval_source) {
                data_source_variant = interval_source;
                valid_source = true;
            }
        } else if (column_source.starts_with("points_x:")) {
            std::string source_name = column_source.substr(9);// Remove "points_x:" prefix
            auto analog_source = _data_manager_extension->getAnalogSource(source_name + ".x");
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.starts_with("points_y:")) {
            std::string source_name = column_source.substr(9);// Remove "points_y:" prefix
            auto analog_source = _data_manager_extension->getAnalogSource(source_name + ".y");
            if (analog_source) {
                data_source_variant = analog_source;
                valid_source = true;
            }
        } else if (column_source.starts_with("lines:")) {
            std::string source_name = column_source.substr(6);// Remove "lines:" prefix
            auto line_source = _data_manager_extension->getLineSource(source_name);
            if (line_source) {
                data_source_variant = line_source;
                valid_source = true;
            }
        } else {
            // Fallback: try to infer source kind when no prefix is provided
            // Prefer events -> intervals -> analog -> lines
            if (auto ev = _data_manager_extension->getEventSource(column_source)) {
                data_source_variant = ev;
                valid_source = true;
            } else if (auto iv = _data_manager_extension->getIntervalSource(column_source)) {
                data_source_variant = iv;
                valid_source = true;
            } else if (auto an = _data_manager_extension->getAnalogSource(column_source)) {
                data_source_variant = an;
                valid_source = true;
            } else if (auto ln = _data_manager_extension->getLineSource(column_source)) {
                data_source_variant = ln;
                valid_source = true;
            }
        }

        if (!valid_source) {
            std::cout << "Failed to create data source for column: " << column_info.name << std::endl;
            return false;
        }

        // Create the computer from the registry
        auto const & registry = *_computer_registry;

        // Get type information for the computer
        auto computer_info_ptr = registry.findComputerInfo(column_info.computerName);
        if (!computer_info_ptr) {
            std::cout << "Computer info not found for " << column_info.computerName << std::endl;
            return false;
        }

        auto computer_base = registry.createComputer(column_info.computerName, data_source_variant, column_info.parameters);
        if (!computer_base) {
            std::cout << "Failed to create computer " << column_info.computerName << " for column: " << column_info.name << std::endl;
            // Continue; the computer may be multi-output and use a different factory
        }

        // Use the actual return type from the computer info instead of hardcoding double
        std::type_index return_type = computer_info_ptr->outputType;
        std::cout << "Computer " << column_info.computerName << " returns type: " << computer_info_ptr->outputTypeName << std::endl;

        // Handle different return types using runtime type dispatch
        bool success = false;

        if (computer_info_ptr->isMultiOutput) {
            // Multi-output: expand into multiple columns using builder.addColumns
            if (return_type == typeid(double)) {
                auto multi = registry.createTypedMultiComputer<double>(
                        column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    std::cout << "Failed to create typed MULTI computer for " << column_info.computerName << std::endl;
                    return false;
                }
                builder.addColumns<double>(column_info.name, std::move(multi));
                success = true;
            } else if (return_type == typeid(int)) {
                auto multi = registry.createTypedMultiComputer<int>(
                        column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    std::cout << "Failed to create typed MULTI computer<int> for " << column_info.computerName << std::endl;
                    return false;
                }
                builder.addColumns<int>(column_info.name, std::move(multi));
                success = true;
            } else if (return_type == typeid(bool)) {
                auto multi = registry.createTypedMultiComputer<bool>(
                        column_info.computerName, data_source_variant, column_info.parameters);
                if (!multi) {
                    std::cout << "Failed to create typed MULTI computer<bool> for " << column_info.computerName << std::endl;
                    return false;
                }
                builder.addColumns<bool>(column_info.name, std::move(multi));
                success = true;
            } else {
                std::cout << "Unsupported multi-output element type for " << column_info.computerName << std::endl;
                return false;
            }
        } else {
            // Single output computers
            if (return_type == typeid(double)) {
                auto computer = registry.createTypedComputer<double>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<double>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(int)) {
                auto computer = registry.createTypedComputer<int>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<int>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(int64_t)) {
                auto computer = registry.createTypedComputer<int64_t>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<int64_t>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(bool)) {
                auto computer = registry.createTypedComputer<bool>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<bool>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(std::vector<double>)) {
                auto computer = registry.createTypedComputer<std::vector<double>>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<std::vector<double>>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(std::vector<int>)) {
                auto computer = registry.createTypedComputer<std::vector<int>>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<std::vector<int>>(column_info.name, std::move(computer));
                    success = true;
                }
            } else if (return_type == typeid(std::vector<float>)) {
                auto computer = registry.createTypedComputer<std::vector<float>>(column_info.computerName, data_source_variant, column_info.parameters);
                if (computer) {
                    builder.addColumn<std::vector<float>>(column_info.name, std::move(computer));
                    success = true;
                }
            } else {
                std::cout << "Unsupported output type for " << column_info.computerName << ": " << computer_info_ptr->outputTypeName << std::endl;
                return false;
            }
        }

        if (!success) {
            std::cout << "Failed to add column " << column_info.name << " with computer " << column_info.computerName << std::endl;
            return false;
        }

        return true;

    } catch (std::exception const & e) {
        std::cout << "Exception adding column " << column_info.name << ": " << e.what() << std::endl;
        return false;
    }
}

void TableRegistry::notify(TableEventType type, std::string const & table_id) const {
    TableEvent ev{type, table_id};
    DataManager__NotifyTableObservers(_data_manager, ev);
}
