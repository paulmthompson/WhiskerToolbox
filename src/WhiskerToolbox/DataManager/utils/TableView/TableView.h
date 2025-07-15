#ifndef TABLE_VIEW_H
#define TABLE_VIEW_H

#include "Column.h"
#include "ExecutionPlan.h"
#include "IRowSelector.h"
#include "DataManagerExtension.h"

#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

// Forward declaration
class TableViewBuilder;

/**
 * @brief The main orchestrator for tabular data views with lazy evaluation.
 * 
 * TableView manages a collection of columns and provides unified access to
 * tabular data. It implements lazy evaluation with caching for both individual
 * columns and ExecutionPlans. The TableView handles dependency resolution
 * and ensures columns are computed in the correct order.
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
     * @brief Gets a span over the specified column's data.
     * 
     * This method delegates to the appropriate Column object and triggers
     * computation if the column is not yet materialized.
     * 
     * @param name The name of the column to retrieve.
     * @return Span over the column's data.
     * @throws std::runtime_error if the column is not found.
     */
    [[nodiscard]] auto getColumnSpan(const std::string& name) -> std::span<const double>;

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

private:
    friend class TableViewBuilder;
    friend class Column; // Allows Column to access the plan cache

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
    void addColumn(std::shared_ptr<Column> column);

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
    std::vector<std::shared_ptr<Column>> m_columns;
    std::map<std::string, size_t> m_colNameToIndex;
    
    // Caches ExecutionPlans, keyed by data source name
    std::map<std::string, ExecutionPlan> m_planCache;
};

#endif // TABLE_VIEW_H
