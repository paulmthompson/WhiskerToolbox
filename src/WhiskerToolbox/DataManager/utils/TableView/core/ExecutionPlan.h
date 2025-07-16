#ifndef EXECUTION_PLAN_H
#define EXECUTION_PLAN_H

#include "DigitalTimeSeries/interval_data.hpp"
#include "TimeFrame.hpp"

#include <utility>
#include <vector>

/**
 * @brief Holds a cached, reusable access pattern for a specific data source.
 * 
 * This class holds the result of an expensive intermediate calculation,
 * typically the mapping of row definitions to specific data array indices.
 * It serves as a cache for computations that can be shared between columns.
 */
class ExecutionPlan {
public:
    /**
     * @brief Default constructor.
     */
    ExecutionPlan() = default;

    /**
     * @brief Constructs an ExecutionPlan with indices.
     * @param indices Vector of TimeFrameIndex objects for direct access.
     */
    explicit ExecutionPlan(std::vector<TimeFrameIndex> indices, std::shared_ptr<TimeFrame> timeFrame);

    /**
     * @brief Constructs an ExecutionPlan with interval pairs.
     * @param intervals Vector of TimeFrameInterval objects for interval operations.
     */
    explicit ExecutionPlan(std::vector<TimeFrameInterval> intervals, std::shared_ptr<TimeFrame> timeFrame);

    /**
     * @brief Gets the indices for direct access operations.
     * @return Const reference to the indices vector.
     */
    std::vector<TimeFrameIndex> const & getIndices() const;

    /**
     * @brief Gets the intervals for interval-based operations.
     * @return Const reference to the intervals vector.
     */
    std::vector<TimeFrameInterval> const & getIntervals() const;

    /**
     * @brief Checks if the plan contains indices.
     * @return True if indices are available, false otherwise.
     */
    bool hasIndices() const;

    /**
     * @brief Checks if the plan contains intervals.
     * @return True if intervals are available, false otherwise.
     */
    bool hasIntervals() const;

    /**
     * @brief Sets the indices for the execution plan.
     * @param indices Vector of TimeFrameIndex objects.
     */
    void setIndices(std::vector<TimeFrameIndex> indices);

    /**
     * @brief Sets the intervals for the execution plan.
     * @param intervals Vector of TimeFrameInterval objects.
     */
    void setIntervals(std::vector<TimeFrameInterval> intervals);

private:
    std::vector<TimeFrameIndex> m_indices;
    std::vector<TimeFrameInterval> m_intervals;
    std::shared_ptr<TimeFrame> m_timeFrame;
};

#endif// EXECUTION_PLAN_H
