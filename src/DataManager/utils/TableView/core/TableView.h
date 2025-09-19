#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include "utils/TableView/columns/ColumnTypeInfo.hpp"
#include "utils/TableView/columns/Column.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/core/RowDescriptor.h"

#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

class DataManagerExtension;
class IColumn;
class IRowSelector;
class TableViewBuilder;

/**
 * @brief The main orchestrator for tabular data views with lazy evaluation.
 * 
 * TableView manages a collection of heterogeneous columns and provides unified 
 * access to tabular data. It implements lazy evaluation with caching for both 
 * individual columns and ExecutionPlans. The TableView handles dependency 
 * resolution and ensures columns are computed in the correct order.
 */
class TableView {
public:

    // Movable but not copyable
    TableView(TableView && other) noexcept;
    TableView & operator=(TableView && other);
    TableView(const TableView & other) = delete;
    TableView & operator=(const TableView & other) = delete;

    /**
     * @brief Gets the number of rows in the table.
     * @return The row count as determined by the row selector.
     */
    [[nodiscard]] auto getRowCount() const -> size_t;

    /**
     * @brief Gets the number of columns in the table.
     * @return The column count.
     */
    [[nodiscard]] auto getColumnCount() const -> size_t;

    /**
     * @brief Gets the values of a column with the specified type.
     * 
     * This method provides type-safe access to column data. It performs a
     * dynamic_cast to ensure the column is of the correct type, and triggers
     * computation if the column is not yet materialized.
     * 
     * @tparam T The expected type of the column data.
     * @param name The name of the column to retrieve.
     * @return Reference to the column's data vector.
     * @throws std::runtime_error if the column is not found or type mismatch.
     */
    template<SupportedColumnType T>
    [[nodiscard]] auto getColumnValues(std::string const & name) -> std::vector<T> const &;

    /**
     * @brief Gets the names of all columns in the table.
     * @return Vector of column names.
     */
    [[nodiscard]] auto getColumnNames() const -> std::vector<std::string>;

    /**
     * @brief Checks if a column exists in the table.
     * @param name The column name to check.
     * @return True if the column exists, false otherwise.
     */
    [[nodiscard]] auto hasColumn(std::string const & name) const -> bool;

    /**
     * @brief Gets the runtime type information for a column.
     * @param name The column name.
     * @return The std::type_info for the column's data type.
     * @throws std::runtime_error if the column is not found.
     */
    [[nodiscard]] auto getColumnType(std::string const & name) const -> std::type_info const &;

    /**
     * @brief Gets the type index for a column.
     * @param name The column name.
     * @return The std::type_index for the column's data type.
     * @throws std::runtime_error if the column is not found.
     */
    [[nodiscard]] auto getColumnTypeIndex(std::string const & name) const -> std::type_index;

    /**
     * @brief Gets column data as a variant, avoiding try/catch for type detection.
     * 
     * This method returns column data in a type-safe variant that contains
     * all possible column types. Consumers can use std::visit or pattern
     * matching to handle the data without try/catch blocks.
     * 
     * @param name The column name.
     * @return ColumnDataVariant containing the column data.
     * @throws std::runtime_error if the column is not found or type not supported.
     */
    [[nodiscard]] auto getColumnDataVariant(std::string const & name) -> ColumnDataVariant;

    /**
     * @brief Applies a visitor to column data in a type-safe manner.
     * @tparam Visitor The visitor type that implements visit methods for all supported types.
     * @param name The column name.
     * @param visitor The visitor instance.
     * @return The result of the visitor.
     * @throws std::runtime_error if the column is not found or type not supported.
     */
    template<typename Visitor>
    auto visitColumnData(std::string const & name, Visitor&& visitor) -> decltype(auto);

    /**
     * @brief Materializes all columns in the table.
     * 
     * This method computes all columns that haven't been materialized yet.
     * It respects dependencies and computes columns in the correct order.
     */
    void materializeAll();

    /**
     * @brief Clears all cached data, forcing recomputation on next access.
     */
    void clearCache();

    /**
     * @brief Gets a descriptor containing the source information for a given row index.
     * 
     * This method provides reverse lookup capability, allowing clients to trace
     * a row back to its original source definition (e.g., timestamp, interval).
     * This is particularly useful for interactive applications like plotting libraries
     * that need to display tooltips or navigate back to source data.
     * 
     * @param row_index The index of the row to get the descriptor for.
     * @return RowDescriptor containing the source information for the row.
     */
    [[nodiscard]] auto getRowDescriptor(size_t row_index) const -> RowDescriptor;

    /**
     * @brief Get contributing EntityIds for a given row, if available.
     * @return Vector of EntityIds; empty if not available.
     */
    [[nodiscard]] auto getRowEntityIds(size_t row_index) const -> std::vector<EntityId>;

    /**
     * @brief Check if this table has EntityID information available.
     * @return True if EntityIDs are available for rows, false otherwise.
     */
    [[nodiscard]] bool hasEntityColumn() const;

    /**
     * @brief Get all EntityIds for all rows in the table.
     * 
     * For tables where each row corresponds to a single entity, this returns
     * a vector with one EntityId per row. For tables where rows can have multiple
     * contributing entities, this returns the primary EntityId for each row.
     * 
     * @return Vector of EntityIds, one per row. Empty vector if no EntityIDs available.
     */
    [[nodiscard]] auto getEntityIds() const -> std::vector<EntityId>;

    /**
     * @brief Set EntityIds directly for transformed tables.
     * 
     * This method allows transforms to preserve EntityId information
     * when creating new tables that don't have execution plans linking
     * back to original data sources.
     * 
     * @param entity_ids Vector of EntityIds, one per row
     */
    void setDirectEntityIds(std::vector<EntityId> entity_ids);

    /**
     * @brief Check if a specific column has EntityID information available.
     * @param name The column name to check.
     * @return True if EntityIDs are available for this column, false otherwise.
     */
    [[nodiscard]] bool hasColumnEntityIds(std::string const & name) const;

    /**
     * @brief Get EntityIds for a specific column.
     * 
     * This method returns EntityIDs that correspond to the data sources
     * used to compute the specified column's values. Each EntityID corresponds
     * to a row in the table.
     * 
     * @param name The column name.
     * @return Vector of EntityIds, one per row. Empty if not available.
     * @throws std::runtime_error if the column is not found.
     */
    [[nodiscard]] ColumnEntityIds getColumnEntityIds(std::string const & name) const;

    /**
     * @brief Get all contributing EntityIDs for a specific cell.
     * 
     * This method returns all EntityIDs that contributed to the computation
     * of a specific cell in the table. For simple columns, this will return
     * the same as getColumnEntityIds()[row_index]. For complex columns that
     * aggregate data from multiple entities, this may return multiple EntityIDs.
     * 
     * @param column_name The column name.
     * @param row_index The row index.
     * @return Vector of EntityIDs that contributed to this cell. Empty if not available.
     * @throws std::runtime_error if the column is not found or row_index is out of bounds.
     */
    [[nodiscard]] auto getCellEntityIds(std::string const & column_name, size_t row_index) const -> std::vector<EntityId>;

    /**
     * @brief Create a new row selector of the same concrete type, filtered to a subset of rows.
     *
     * @param keep_indices Indices of rows to keep (relative to this table's current rows), in ascending order.
     * @return A new row selector that preserves the original selector's semantics while containing only the kept rows.
     */
    [[nodiscard]] auto cloneRowSelectorFiltered(std::vector<size_t> const & keep_indices) const -> std::unique_ptr<IRowSelector>;

    /**
     * @brief Access the data manager extension backing this table.
     * @return Shared pointer to the `DataManagerExtension`.
     */
    [[nodiscard]] auto getDataManagerExtension() const -> std::shared_ptr<DataManagerExtension> { return m_dataManager; }

private:
    friend class TableViewBuilder;
    // Grant friend access to the templated Column class
    template<SupportedColumnType T>
    friend class Column;

    /**
     * @brief Private constructor for TableViewBuilder.
     * @param rowSelector The row selector defining table rows.
     * @param dataManager The data manager for accessing data sources.
     */
    TableView(std::unique_ptr<IRowSelector> rowSelector,
              std::shared_ptr<DataManagerExtension> dataManager);

    /**
     * @brief Gets or creates the ExecutionPlan for a given data source.
     * 
     * This method is critical for the caching system. It checks the plan cache
     * first, and if not found, uses the IRowSelector to generate the necessary
     * indices for the given data source, then stores the new plan in the cache.
     * 
     * @param sourceName The name of the data source (e.g., "LFP", "Spikes.x").
     * @return Reference to the ExecutionPlan for the source.
     */
    [[nodiscard]] auto getExecutionPlanFor(std::string const & sourceName) -> ExecutionPlan const &;

    /**
     * @brief Adds a column to the table.
     * @param column Shared pointer to the column to add.
     * @throws std::runtime_error if a column with the same name already exists.
     */
    void addColumn(std::shared_ptr<IColumn> column);

    /**
     * @brief Materializes a column and its dependencies.
     * 
     * This method ensures that all dependencies are materialized before
     * materializing the target column. It handles circular dependency detection.
     * 
     * @param columnName The name of the column to materialize.
     * @param materializing Set of columns currently being materialized (for cycle detection).
     */
    void materializeColumn(std::string const & columnName, std::set<std::string> & materializing);

    /**
     * @brief Generates an ExecutionPlan for a specific data source.
     * 
     * This method uses the row selector to create the appropriate ExecutionPlan
     * based on the type of row selector and the requirements of the data source.
     * 
     * @param sourceName The name of the data source.
     * @return The generated ExecutionPlan.
     */
    [[nodiscard]] auto generateExecutionPlan(std::string const & sourceName) -> ExecutionPlan;

    std::unique_ptr<IRowSelector> m_rowSelector;
    std::shared_ptr<DataManagerExtension> m_dataManager;
    std::vector<std::shared_ptr<IColumn>> m_columns;
    std::map<std::string, size_t> m_colNameToIndex;

    // Caches ExecutionPlans, keyed by data source name
    std::map<std::string, ExecutionPlan> m_planCache;
    
    // Direct EntityId storage for transformed tables
    std::vector<EntityId> m_direct_entity_ids;
};

// Template method implementation for getColumnValues
template<SupportedColumnType T>
auto TableView::getColumnValues(std::string const & name) -> std::vector<T> const & {
    // 1. Find the IColumn pointer by name
    auto it = m_colNameToIndex.find(name);
    if (it == m_colNameToIndex.end()) {
        throw std::runtime_error("Column '" + name + "' not found in table");
    }

    // 2. Get the column and attempt dynamic_cast to Column<T>
    auto & column = m_columns[it->second];
    auto * typedColumn = dynamic_cast<Column<T> *>(column.get());

    // 3. If cast fails, throw exception for type mismatch
    if (!typedColumn) {
        throw std::runtime_error("Column '" + name + "' is not of the requested type");
    }

    // 4. Call getValues on the typed column
    return typedColumn->getValues(this);
}

// Template method implementation for visitColumnData
template<typename Visitor>
auto TableView::visitColumnData(std::string const & name, Visitor&& visitor) -> decltype(auto) {
    auto variant = getColumnDataVariant(name);
    return std::visit(std::forward<Visitor>(visitor), variant);
}

#endif// TABLE_VIEW_H
