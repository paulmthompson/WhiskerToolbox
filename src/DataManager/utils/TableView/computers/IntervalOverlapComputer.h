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
    AssignID,       ///< Assigns the index of the column interval that contains/overlaps with the row interval
    CountOverlaps,   ///< Counts the number of column intervals that overlap with each row interval
    AssignID_Start,  ///< Finds the start index of the column interval that contains/overlaps with the row interval
    AssignID_End     ///< Finds the end index of the column interval that contains/overlaps with the row interval
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
* @param sourceTimeFrame The timeframe for the column intervals.
* @param destinationTimeFrame The timeframe for the row interval.
* @return Number of overlapping column intervals.
*/
[[nodiscard]] int64_t countOverlappingIntervals(TimeFrameInterval const & rowInterval,
                                                std::vector<Interval> const & columnIntervals,
                                                TimeFrame const * sourceTimeFrame,
                                                TimeFrame const * destinationTimeFrame);

/**
 * @brief Templated computer for analyzing overlaps between row intervals and column intervals.
 * 
 * Source type: IIntervalSource
 * Selector type: Interval
 * Output type: T
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
    [[nodiscard]] std::pair<std::vector<T>, ColumnEntityIds> compute(ExecutionPlan const & plan) const override {
        if (!plan.hasIntervals()) {
            throw std::runtime_error("IntervalOverlapComputer requires an ExecutionPlan with intervals");
        }

        auto rowIntervals = plan.getIntervals();
        auto destinationTimeFrame = plan.getTimeFrame();
        auto sourceTimeFrame = m_source->getTimeFrame();

        std::vector<T> results;
        results.reserve(rowIntervals.size());
        std::vector<std::vector<EntityId>> entity_ids;
        //entity_ids.reserve(rowIntervals.size());
        if (m_operation == IntervalOverlapOperation::AssignID ||
            m_operation == IntervalOverlapOperation::AssignID_Start ||
            m_operation == IntervalOverlapOperation::AssignID_End) {
                for (auto const & rowInterval: rowIntervals) {
                    auto columnIntervals = m_source->getIntervalsInRange(
                        TimeFrameIndex(0), 
                        rowInterval.end, 
                        destinationTimeFrame.get());
                    if (columnIntervals.empty()) {
                        results.push_back(static_cast<T>(-1));
                        //entity_ids.push_back(0);
                        continue;
                    }
                    // Need to convert to their time coordinates
                    auto source_start = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(columnIntervals.back().start));
                    auto source_end = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(columnIntervals.back().end));
                    auto destination_start = destinationTimeFrame->getTimeAtIndex(rowInterval.start);
                    auto destination_end = destinationTimeFrame->getTimeAtIndex(rowInterval.end);

                    if (source_start <= destination_end && destination_start <= source_end) {

                        if (m_operation == IntervalOverlapOperation::AssignID_Start) {
                            // Convert into row time frame
                            auto source_start_index = destinationTimeFrame->getIndexAtTime(static_cast<float>(source_start));
                            results.push_back(static_cast<T>(source_start_index.getValue()));
                            //entity_ids.push_back(m_source->getEntityIdAt(columnIntervals.size() - 1));
                        } else if (m_operation == IntervalOverlapOperation::AssignID_End) {
                            // Convert into row time frame
                            auto source_end_index = destinationTimeFrame->getIndexAtTime(static_cast<float>(source_end));
                            results.push_back(static_cast<T>(source_end_index.getValue()));
                            //entity_ids.push_back(m_source->getEntityIdAt(columnIntervals.size() - 1));
                        } else {
                            results.push_back(static_cast<T>(columnIntervals.size() - 1));
                            //entity_ids.push_back(m_source->getEntityIdAt(columnIntervals.size() - 1));
                        }
                    } else {
                        results.push_back(static_cast<T>(-1));
                        //entity_ids.push_back(0);
                    }
                }
        } else if (m_operation == IntervalOverlapOperation::CountOverlaps) {
            for (auto const & rowInterval: rowIntervals) {
                auto columnIntervals = m_source->getIntervalsInRange(
                    rowInterval.start, 
                    rowInterval.end, 
                    destinationTimeFrame.get());

                results.push_back(static_cast<T>(countOverlappingIntervals(rowInterval, 
                    columnIntervals, 
                    sourceTimeFrame.get(), 
                    destinationTimeFrame.get())));
                for (auto const & columnInterval: columnIntervals) {
                    //entity_ids.push_back(m_source->getEntityIdAt(columnInterval.start));
                }
            }
        }

        return {results, entity_ids};
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

    [[nodiscard]] EntityIdStructure getEntityIdStructure() const override {
        switch (m_operation) {
            case IntervalOverlapOperation::AssignID:
            case IntervalOverlapOperation::AssignID_Start:
            case IntervalOverlapOperation::AssignID_End:
                return EntityIdStructure::Simple;  // One EntityID per row (the assigned interval)
            case IntervalOverlapOperation::CountOverlaps:
                return EntityIdStructure::Complex; // Multiple EntityIDs per row (all overlapping intervals)
            default:
                return EntityIdStructure::None;
        }
    }

private:
    std::shared_ptr<IIntervalSource> m_source;
    IntervalOverlapOperation m_operation;
    std::string m_sourceName;
};


#endif// INTERVAL_OVERLAP_COMPUTER_H
