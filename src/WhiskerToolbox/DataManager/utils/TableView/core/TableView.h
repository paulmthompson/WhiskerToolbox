#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/columns/Column.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/core/RowDescriptor.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/adapters/DataManagerExtension.h"

#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>

// Forward declaration
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
    template<typename T>
    [[nodiscard]] auto getColumnValues(const std::string& name) -> const std::vector<T>&;

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
    [[nodiscard]] auto hasColumn(const std::string& name) const -> bool;

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

private:
    friend class TableViewBuilder;
    // Grant friend access to the templated Column class
    template<typename T> friend class Column;

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
    [[nodiscard]] auto getExecutionPlanFor(const std::string& sourceName) -> const ExecutionPlan&;

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
    void materializeColumn(const std::string& columnName, std::set<std::string>& materializing);

    /**
     * @brief Generates an ExecutionPlan for a specific data source.
     * 
     * This method uses the row selector to create the appropriate ExecutionPlan
     * based on the type of row selector and the requirements of the data source.
     * 
     * @param sourceName The name of the data source.
     * @return The generated ExecutionPlan.
     */
    [[nodiscard]] auto generateExecutionPlan(const std::string& sourceName) -> ExecutionPlan;

    std::unique_ptr<IRowSelector> m_rowSelector;
    std::shared_ptr<DataManagerExtension> m_dataManager;
    std::vector<std::shared_ptr<IColumn>> m_columns;
    std::map<std::string, size_t> m_colNameToIndex;
    
    // Caches ExecutionPlans, keyed by data source name
    std::map<std::string, ExecutionPlan> m_planCache;
};

// Template method implementation for getColumnValues
template<typename T>
auto TableView::getColumnValues(const std::string& name) -> const std::vector<T>& {
    // 1. Find the IColumn pointer by name
    auto it = m_colNameToIndex.find(name);
    if (it == m_colNameToIndex.end()) {
        throw std::runtime_error("Column '" + name + "' not found in table");
    }
    
    // 2. Get the column and attempt dynamic_cast to Column<T>
    auto& column = m_columns[it->second];
    auto* typedColumn = dynamic_cast<Column<T>*>(column.get());
    
    // 3. If cast fails, throw exception for type mismatch
    if (!typedColumn) {
        throw std::runtime_error("Column '" + name + "' is not of the requested type");
    }
    
    // 4. Call getValues on the typed column
    return typedColumn->getValues(this);
}

#endif // TABLE_VIEW_H
