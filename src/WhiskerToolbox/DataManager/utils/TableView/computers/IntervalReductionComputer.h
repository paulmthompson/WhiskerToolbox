#ifndef INTERVAL_REDUCTION_COMPUTER_H
#define INTERVAL_REDUCTION_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

class IAnalogSource;

/**
 * @brief Reduction operation types for interval computations.
 */
enum class ReductionType : std::uint8_t {
    Mean,      ///< Calculate mean value over interval
    Max,       ///< Calculate maximum value over interval
    Min,       ///< Calculate minimum value over interval
    StdDev,    ///< Calculate standard deviation over interval
    Sum,       ///< Calculate sum of values over interval
    Count      ///< Count number of values in interval
};

/**
 * @brief Column computer that performs reduction operations over intervals.
 * 
 * This computer takes an analog source and performs reduction operations
 * (mean, max, min, std dev, etc.) over specified intervals. It uses the
 * ExecutionPlan to get interval pairs and computes the reduction for each
 * interval using efficient span-based data access.
 */
class IntervalReductionComputer : public IColumnComputer<double> {
public:
    /**
     * @brief Constructs an IntervalReductionComputer.
     * 
     * @param source Shared pointer to the analog source to compute over.
     * @param reduction The type of reduction operation to perform.
     */
    IntervalReductionComputer(std::shared_ptr<IAnalogSource> source, 
                             ReductionType reduction);

    /**
     * @brief Constructs an IntervalReductionComputer with custom source name.
     * 
     * @param source Shared pointer to the analog source to compute over.
     * @param reduction The type of reduction operation to perform.
     * @param sourceName Custom name for the source dependency.
     */
    IntervalReductionComputer(std::shared_ptr<IAnalogSource> source, 
                             ReductionType reduction,
                             std::string sourceName);

    // IColumnComputer interface implementation
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<double> override;
    [[nodiscard]] auto getSourceDependency() const -> std::string override;

private:
    /**
     * @brief Computes the reduction for a single interval.
     * 
     * @param data Span over the data for the interval.
     * @return The computed reduction value.
     */
    [[nodiscard]] auto computeReduction(std::span<const float> data) const -> float;

    /**
     * @brief Computes the mean of the data span.
     * 
     * @param data Span over the data.
     * @return The mean value.
     */
    [[nodiscard]] auto computeMean(std::span<const float> data) const -> float;

    /**
     * @brief Computes the maximum of the data span.
     * 
     * @param data Span over the data.
     * @return The maximum value.
     */
    [[nodiscard]] auto computeMax(std::span<const float> data) const -> float;

    /**
     * @brief Computes the minimum of the data span.
     * 
     * @param data Span over the data.
     * @return The minimum value.
     */
    [[nodiscard]] auto computeMin(std::span<const float> data) const -> float;

    /**
     * @brief Computes the standard deviation of the data span.
     * 
     * @param data Span over the data.
     * @return The standard deviation value.
     */
    [[nodiscard]] auto computeStdDev(std::span<const float> data) const -> float;

    /**
     * @brief Computes the sum of the data span.
     * 
     * @param data Span over the data.
     * @return The sum value.
     */
    [[nodiscard]] auto computeSum(std::span<const float> data) const -> float;

    /**
     * @brief Returns the count of values in the data span.
     * 
     * @param data Span over the data.
     * @return The count as a float.
     */
    [[nodiscard]] auto computeCount(std::span<const float> data) const -> float;

    std::shared_ptr<IAnalogSource> m_source;
    ReductionType m_reduction;
    std::string m_sourceName;
};

#endif // INTERVAL_REDUCTION_COMPUTER_H
