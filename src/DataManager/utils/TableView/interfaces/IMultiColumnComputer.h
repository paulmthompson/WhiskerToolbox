#ifndef IMULTI_COLUMN_COMPUTER_H
#define IMULTI_COLUMN_COMPUTER_H

#include "Entity/EntityTypes.hpp"
#include "utils/TableView/columns/IColumn.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Templated interface for computing multiple output columns in one pass.
 *
 * A multi-column computer produces N outputs of the same element type T,
 * typically representing closely related measures that should be computed in a
 * single pass for performance.
 */
template<typename T>
class IMultiColumnComputer {
public:
    virtual ~IMultiColumnComputer() = default;

    // Make this class non-copyable and non-movable since it's a pure interface
    IMultiColumnComputer(IMultiColumnComputer const &) = delete;
    IMultiColumnComputer & operator=(IMultiColumnComputer const &) = delete;
    IMultiColumnComputer(IMultiColumnComputer &&) = delete;
    IMultiColumnComputer & operator=(IMultiColumnComputer &&) = delete;

    /**
     * @brief Computes all output columns for the provided plan in one batch.
     * @param plan The pre-computed execution plan for indexed/interval access.
     * @return A vector of outputs, one entry per output column; each is a full
     *         column vector of values of type T with size equal to the number
     *         of rows dictated by the plan.
     */
    [[nodiscard]] virtual std::pair<std::vector<std::vector<T>>, ColumnEntityIds> computeBatch(ExecutionPlan const & plan) const = 0;

    /**
     * @brief Names for each output (suffixes to be appended to a base name).
     * @return Vector of suffix strings; size determines the number of outputs.
     */
    [[nodiscard]] virtual std::vector<std::string> getOutputNames() const = 0;

    /**
     * @brief Declares dependencies on other columns.
     */
    [[nodiscard]] virtual std::vector<std::string> getDependencies() const { return {}; }

    /**
     * @brief Declares the required data source name for this computation.
     */
    [[nodiscard]] virtual std::string getSourceDependency() const = 0;


    [[nodiscard]] virtual EntityIdStructure getEntityIdStructure() const {
        return EntityIdStructure::None;
    }

    /**
     * @brief Gets EntityIDs for each row in the computed columns.
     * 
     * This method returns EntityIDs that correspond to the data sources
     * used to compute each row's values. Since this is a multi-column computer,
     * all output columns from this computer will share the same EntityIDs.
     * 
     * For computers where each row comes from a single entity, this returns
     * one EntityID per row. For computers that aggregate data from multiple
     * entities, this returns the primary or representative EntityID.
     * 
     * @param plan The execution plan used for computation.
     * @return Vector of EntityIDs, one per row. Empty if not available.
     */
    [[nodiscard]] virtual ColumnEntityIds computeColumnEntityIds(ExecutionPlan const & plan) const {
        (void) plan;
        return {};
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

protected:
    IMultiColumnComputer() = default;
};

#endif// IMULTI_COLUMN_COMPUTER_H
