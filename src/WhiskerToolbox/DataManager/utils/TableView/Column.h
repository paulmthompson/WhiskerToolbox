#ifndef COLUMN_H
#define COLUMN_H

#include "IColumnComputer.h"

#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

// Forward declaration
class TableView;

/**
 * @brief Represents a single column in a TableView with lazy evaluation.
 * 
 * The Column class holds the state for a single column, including its name,
 * computation strategy, and cached data. It uses lazy evaluation - data is
 * only computed when first requested via getSpan().
 */
class Column {
public:
    /**
     * @brief Gets a span over the column's data.
     * 
     * This is the main entry point for accessing column data. It triggers
     * computation if the data is not yet materialized, otherwise returns
     * a span over the cached data.
     * 
     * @param table Pointer to the TableView that owns this column.
     * @return Span over the column's data.
     */
    [[nodiscard]] auto getSpan(TableView* table) -> std::span<const double>;

    /**
     * @brief Gets the name of this column.
     * @return The column name.
     */
    [[nodiscard]] auto getName() const -> const std::string& { return m_name; }

    /**
     * @brief Gets the source dependency for this column.
     * @return The name of the required data source.
     */
    [[nodiscard]] auto getSourceDependency() const -> std::string {
        return m_computer->getSourceDependency();
    }

    /**
     * @brief Gets the column dependencies for this column.
     * @return Vector of column names this column depends on.
     */
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> {
        return m_computer->getDependencies();
    }

    /**
     * @brief Checks if the column data has been materialized.
     * @return True if data is cached, false otherwise.
     */
    [[nodiscard]] auto isMaterialized() const -> bool {
        return std::holds_alternative<std::vector<double>>(m_cache);
    }

    /**
     * @brief Clears the cached data, forcing recomputation on next access.
     */
    void clearCache() {
        m_cache = std::monostate{};
    }

private:
    friend class TableViewBuilder;

    /**
     * @brief Private constructor for TableViewBuilder.
     * @param name The name of the column.
     * @param computer The computation strategy for this column.
     */
    Column(std::string name, std::unique_ptr<IColumnComputer> computer);

    /**
     * @brief Materializes the column data if not already cached.
     * 
     * This method performs the actual computation by getting the appropriate
     * ExecutionPlan from the TableView and calling the computer's compute method.
     * 
     * @param table Pointer to the TableView that owns this column.
     */
    void materialize(TableView* table);

    std::string m_name;
    std::unique_ptr<IColumnComputer> m_computer;
    std::variant<std::monostate, std::vector<double>> m_cache;
};

#endif // COLUMN_H
