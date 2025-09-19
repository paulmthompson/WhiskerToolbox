#ifndef ICOLUMN_COMPUTER_H
#define ICOLUMN_COMPUTER_H

#include "Entity/EntityTypes.hpp"
#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

/**
 * @brief Templated interface for computing column values in a batch operation.
 * 
 * This interface defines the strategy for computing all values in a column
 * in a single batch operation. Different implementations can provide
 * different computation strategies (direct access, interval reductions,
 * transformations, etc.). The template parameter T allows for heterogeneous
 * column types.
 */
template<typename T>
class IColumnComputer {
public:
    virtual ~IColumnComputer() = default;

    // Make this class non-copyable and non-movable since it's a pure interface
    IColumnComputer(IColumnComputer const &) = delete;
    IColumnComputer & operator=(IColumnComputer const &) = delete;
    IColumnComputer(IColumnComputer &&) = delete;
    IColumnComputer & operator=(IColumnComputer &&) = delete;

    /**
     * @brief The core batch computation method.
     * 
     * This method performs the actual computation of all column values
     * based on the provided execution plan. The execution plan contains
     * the cached access patterns (indices or intervals) for the data source.
     * 
     * @param plan The execution plan containing cached access patterns.
     * @return Vector of computed values for the entire column.
     */
    [[nodiscard]] virtual std::vector<T> compute(ExecutionPlan const & plan) const = 0;

    /**
     * @brief Declares dependencies on other columns.
     * 
     * For transformed columns that depend on other columns, this method
     * returns the names of the columns that must be computed first.
     * 
     * @return Vector of column names this computer depends on.
     */
    [[nodiscard]] virtual std::vector<std::string> getDependencies() const {
        return {};
    }

    /**
     * @brief Declares the required data source.
     * 
     * This method returns the name of the data source that this computer
     * needs to access (e.g., "LFP", "Spikes.x").
     * 
     * @return The name of the required data source.
     */
    [[nodiscard]] virtual std::string getSourceDependency() const = 0;

    /**
     * @brief Gets the EntityID structure type for this computer.
     * 
     * This indicates whether the computer provides no EntityIDs, simple EntityIDs
     * (one per row), or complex EntityIDs (multiple per row).
     * 
     * @return The EntityID structure type for this computer.
     */
    [[nodiscard]] virtual EntityIdStructure getEntityIdStructure() const {
        return EntityIdStructure::None;
    }

    /**
     * @brief Computes all EntityIDs for the column using the high-level variant approach.
     * 
     * The returned variant contains one of:
     * - std::monostate: No EntityIDs available
     * - std::vector<EntityId>: One EntityID per row (simple)
     * - std::vector<std::vector<EntityId>>: Multiple EntityIDs per row (complex)
     * 
     * Note: Shared EntityID collections are typically handled at the Column level
     * for table transforms, not at the computer level.
     * 
     * @param plan The execution plan used for computation.
     * @return ColumnEntityIds variant containing the EntityIDs for this column.
     */
    [[nodiscard]] virtual ColumnEntityIds computeColumnEntityIds(ExecutionPlan const & plan) const {
        (void) plan;
        return std::monostate{};
    }

    /**
     * @brief Computes EntityIDs for a specific row.
     * 
     * This is a convenience method that works across all EntityID structures.
     * 
     * @param plan The execution plan used for computation.
     * @param row_index The row index to get EntityIDs for.
     * @return Vector of EntityIDs for the specified row. Empty if not available.
     */
    [[nodiscard]] virtual std::vector<EntityId> computeCellEntityIds(ExecutionPlan const & plan, size_t row_index) const {
        auto structure = getEntityIdStructure();
        if (structure == EntityIdStructure::None) {
            return {};
        }

        auto column_entities = computeColumnEntityIds(plan);

        switch (structure) {
            case EntityIdStructure::Simple: {
                auto & entities = std::get<std::vector<EntityId>>(column_entities);
                return (row_index < entities.size()) ? std::vector<EntityId>{entities[row_index]} : std::vector<EntityId>{};
            }
            case EntityIdStructure::Complex: {
                auto & entity_matrix = std::get<std::vector<std::vector<EntityId>>>(column_entities);
                return (row_index < entity_matrix.size()) ? entity_matrix[row_index] : std::vector<EntityId>{};
            }
            case EntityIdStructure::Shared:
                // Shared collections not typically used at computer level
                return {};
            case EntityIdStructure::None:
            default:
                return {};
        }
    }

    /**
     * @brief Checks if this computer can provide EntityID information.
     * @return True if EntityIDs are available, false otherwise.
     */
    [[nodiscard]] virtual bool hasEntityIds() const {
        return getEntityIdStructure() != EntityIdStructure::None;
    }

protected:
    // Protected constructor to allow derived classes to construct
    IColumnComputer() = default;
};

#endif// ICOLUMN_COMPUTER_H
