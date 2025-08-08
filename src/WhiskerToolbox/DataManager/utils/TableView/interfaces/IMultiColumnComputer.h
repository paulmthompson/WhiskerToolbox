#ifndef IMULTI_COLUMN_COMPUTER_H
#define IMULTI_COLUMN_COMPUTER_H

#include "utils/TableView/core/ExecutionPlan.h"

#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief Templated interface for computing multiple output columns in one pass.
 *
 * A multi-column computer produces N outputs of the same element type T,
 * typically representing closely related measures that should be computed in a
 * single pass for performance.
 */
template <typename T>
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
    [[nodiscard]] virtual auto computeBatch(ExecutionPlan const & plan) const -> std::vector<std::vector<T>> = 0;

    /**
     * @brief Names for each output (suffixes to be appended to a base name).
     * @return Vector of suffix strings; size determines the number of outputs.
     */
    [[nodiscard]] virtual auto getOutputNames() const -> std::vector<std::string> = 0;

    /**
     * @brief Declares dependencies on other columns.
     */
    [[nodiscard]] virtual auto getDependencies() const -> std::vector<std::string> { return {}; }

    /**
     * @brief Declares the required data source name for this computation.
     */
    [[nodiscard]] virtual auto getSourceDependency() const -> std::string = 0;

protected:
    IMultiColumnComputer() = default;
};

#endif // IMULTI_COLUMN_COMPUTER_H


