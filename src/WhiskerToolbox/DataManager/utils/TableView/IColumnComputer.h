#ifndef ICOLUMN_COMPUTER_H
#define ICOLUMN_COMPUTER_H

#include "ExecutionPlan.h"

#include <string>
#include <vector>

/**
 * @brief Interface for computing column values in a batch operation.
 * 
 * This interface defines the strategy for computing all values in a column
 * in a single batch operation. Different implementations can provide
 * different computation strategies (direct access, interval reductions,
 * transformations, etc.).
 */
class IColumnComputer {
public:
    virtual ~IColumnComputer() = default;
    
    // Make this class non-copyable and non-movable since it's a pure interface
    IColumnComputer(const IColumnComputer&) = delete;
    IColumnComputer& operator=(const IColumnComputer&) = delete;
    IColumnComputer(IColumnComputer&&) = delete;
    IColumnComputer& operator=(IColumnComputer&&) = delete;

protected:
    // Protected constructor to allow derived classes to construct
    IColumnComputer() = default;

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
    [[nodiscard]] virtual auto compute(const ExecutionPlan& plan) const -> std::vector<double> = 0;

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
};

#endif // ICOLUMN_COMPUTER_H
