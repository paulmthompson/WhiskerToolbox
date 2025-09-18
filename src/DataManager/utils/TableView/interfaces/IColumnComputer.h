#ifndef ICOLUMN_COMPUTER_H
#define ICOLUMN_COMPUTER_H

#include "utils/TableView/core/ExecutionPlan.h"
#include "Entity/EntityTypes.hpp"

#include <string>
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
    [[nodiscard]] virtual auto compute(ExecutionPlan const & plan) const -> std::vector<T> = 0;

    /**
     * @brief Declares dependencies on other columns.
     * 
     * For transformed columns that depend on other columns, this method
     * returns the names of the columns that must be computed first.
     * 
     * @return Vector of column names this computer depends on.
     */
    [[nodiscard]] virtual auto getDependencies() const -> std::vector<std::string> {
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
    [[nodiscard]] virtual auto getSourceDependency() const -> std::string = 0;

    /**
     * @brief Checks if this computer can provide EntityID information.
     * @return True if EntityIDs are available, false otherwise.
     */
    [[nodiscard]] virtual bool hasEntityIds() const { return false; }

    /**
     * @brief Gets EntityIDs for each row in the computed column.
     * 
     * This method returns EntityIDs that correspond to the data sources
     * used to compute each row's values.
     * 
     * @param plan The execution plan used for computation.
     * @return Vector of EntityIDs, one per row. Empty if not available.
     */
    [[nodiscard]] virtual std::vector<EntityId> computeEntityIds(ExecutionPlan const & plan) const { 
        (void)plan; 
        return {}; 
    }

    /**
     * @brief Gets all contributing EntityIDs for a specific row.
     * 
     * @param plan The execution plan used for computation.
     * @param row_index The row index to get EntityIDs for.
     * @return Vector of EntityIDs that contributed to this row. Empty if not available.
     */
    [[nodiscard]] virtual std::vector<EntityId> computeRowEntityIds(ExecutionPlan const & plan, size_t row_index) const {
        auto allIds = computeEntityIds(plan);
        if (row_index < allIds.size()) {
            return {allIds[row_index]};
        }
        return {};
    }

protected:
    // Protected constructor to allow derived classes to construct
    IColumnComputer() = default;
};

#endif// ICOLUMN_COMPUTER_H
