#ifndef INTERVAL_OVERLAP_COMPUTER_H
#define INTERVAL_OVERLAP_COMPUTER_H

#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IIntervalSource.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of operations that can be performed on interval overlaps.
 */
enum class IntervalOverlapOperation : std::uint8_t {
    AssignID,    ///< Assigns the index of the column interval that contains/overlaps with the row interval
    CountOverlaps///< Counts the number of column intervals that overlap with each row interval
};

/**
* @brief Checks if two intervals overlap.
* @param a First interval.
* @param b Second interval.
* @return True if intervals overlap, false otherwise.
*/
[[nodiscard]] bool intervalsOverlap(TimeFrameInterval const & a, TimeFrameInterval const & b);

/**
* @brief Finds the index of the column interval that contains the given row interval.
* @param rowInterval The row interval to find a container for.
* @param columnIntervals The column intervals to search through.
* @return Index of the containing column interval, or -1 if none found.
*/
[[nodiscard]] int64_t findContainingInterval(TimeFrameInterval const & rowInterval,
                                             std::vector<Interval> const & columnIntervals);

/**
* @brief Counts the number of column intervals that overlap with the given row interval.
* @param rowInterval The row interval to check overlaps for.
* @param columnIntervals The column intervals to check against.
* @return Number of overlapping column intervals.
*/
[[nodiscard]] int64_t countOverlappingIntervals(TimeFrameInterval const & rowInterval,
                                                std::vector<Interval> const & columnIntervals);

/**
 * @brief Templated computer for analyzing overlaps between row intervals and column intervals.
 * 
 * This computer works with two sets of intervals: the row intervals (from the ExecutionPlan)
 * and the column intervals (from an IIntervalSource). It can perform different operations
 * to analyze their relationships:
 * - AssignID: For each row interval, finds the index of the column interval that contains it
 * - CountOverlaps: For each row interval, counts how many column intervals overlap with it
 * 
 * The template parameter T determines the return type:
 * - IntervalOverlapOperation::AssignID requires T = int64_t (returns -1 if no overlap)
 * - IntervalOverlapOperation::CountOverlaps requires T = int64_t or size_t
 */
template<typename T>
class IntervalOverlapComputer : public IColumnComputer<T> {
public:
    /**
     * @brief Constructor for IntervalOverlapComputer.
     * @param source Shared pointer to the interval source (column intervals).
     * @param operation The operation to perform on interval overlaps.
     * @param sourceName The name of the data source (for dependency tracking).
     */
    IntervalOverlapComputer(std::shared_ptr<IIntervalSource> source,
                            IntervalOverlapOperation operation,
                            std::string sourceName)
        : m_source(std::move(source)),
          m_operation(operation),
          m_sourceName(std::move(sourceName)) {}

    /**
     * @brief Computes the result for all row intervals in the execution plan.
     * @param plan The execution plan containing row interval boundaries.
     * @return Vector of computed results for each row interval.
     */
    [[nodiscard]] auto compute(ExecutionPlan const & plan) const -> std::vector<T> override {
        if (!plan.hasIntervals()) {
            throw std::runtime_error("IntervalOverlapComputer requires an ExecutionPlan with intervals");
        }

        auto rowIntervals = plan.getIntervals();
        auto destinationTimeFrame = plan.getTimeFrame();

        std::vector<T> results;
        results.reserve(rowIntervals.size());

        // Get all column intervals from the source
        // We need to get the full range of column intervals
        // Use the actual size of the destination timeframe
        auto destinationSize = destinationTimeFrame->getTotalFrameCount();
        auto columnIntervals = m_source->getIntervalsInRange(
                TimeFrameIndex(0),
                TimeFrameIndex(destinationSize - 1), // Use the actual size
                destinationTimeFrame.get());

        for (auto const & rowInterval: rowIntervals) {
            switch (m_operation) {
                case IntervalOverlapOperation::AssignID:
                    results.push_back(static_cast<T>(findContainingInterval(rowInterval, columnIntervals)));
                    break;
                case IntervalOverlapOperation::CountOverlaps:
                    results.push_back(static_cast<T>(countOverlappingIntervals(rowInterval, columnIntervals)));
                    break;
                default:
                    throw std::runtime_error("Unknown IntervalOverlapOperation");
            }
        }

        return results;
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

private:
    std::shared_ptr<IIntervalSource> m_source;
    IntervalOverlapOperation m_operation;
    std::string m_sourceName;
};


#endif// INTERVAL_OVERLAP_COMPUTER_H
