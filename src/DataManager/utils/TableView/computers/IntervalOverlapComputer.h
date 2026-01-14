#ifndef INTERVAL_OVERLAP_COMPUTER_H
#define INTERVAL_OVERLAP_COMPUTER_H

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IColumnComputer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of operations that can be performed on interval overlaps.
 */
enum class IntervalOverlapOperation : std::uint8_t {
    AssignID,      ///< Assigns the index of the column interval that contains/overlaps with the row interval
    CountOverlaps, ///< Counts the number of column intervals that overlap with each row interval
    AssignID_Start,///< Finds the start index of the column interval that contains/overlaps with the row interval
    AssignID_End   ///< Finds the end index of the column interval that contains/overlaps with the row interval
};

/**
* @brief Checks if two intervals overlap.
* @param a First interval.
* @param b Second interval.
* @return True if intervals overlap, false otherwise.
*/
[[nodiscard]] bool intervalsOverlap(TimeFrameInterval const & a, TimeFrameInterval const & b);

/**
* @brief Helper function to check if a column interval overlaps with a row interval using absolute time coordinates.
* @param rowInterval The row interval to check against.
* @param columnInterval The column interval to check.
* @param sourceTimeFrame The timeframe for the column interval.
* @param destinationTimeFrame The timeframe for the row interval.
* @return True if intervals overlap, false otherwise.
*/
[[nodiscard]] bool intervalsOverlapInAbsoluteTime(TimeFrameInterval const & rowInterval,
                                                  Interval const & columnInterval,
                                                  TimeFrame const * sourceTimeFrame,
                                                  TimeFrame const * destinationTimeFrame);

/**
* @brief Finds the index of the column interval that contains the given row interval.
* @param rowInterval The row interval to find a container for.
* @param columnIntervals The column intervals to search through.
* @return Index of the containing column interval, or -1 if none found.
*/
[[nodiscard]] int64_t findContainingInterval(TimeFrameInterval const & rowInterval,
                                             std::ranges::range auto & columnIntervals) {
    for (size_t i = 0; i < columnIntervals.size(); ++i) {
        auto const & colInterval = columnIntervals[i];

        // Check if column interval contains the row interval
        // Column interval contains row interval if: colInterval.start <= rowInterval.start && rowInterval.end <= colInterval.end
        if (colInterval.start <= rowInterval.start.getValue() && rowInterval.end.getValue() <= colInterval.end) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;// No containing interval found
}

/**
* @brief Counts the number of column intervals that overlap with the given row interval.
* @param rowInterval The row interval to check overlaps for.
* @param columnIntervals The column intervals to check against.
* @param sourceTimeFrame The timeframe for the column intervals.
* @param destinationTimeFrame The timeframe for the row interval.
* @return Number of overlapping column intervals.
*/
[[nodiscard]] int64_t countOverlappingIntervals(TimeFrameInterval const & rowInterval,
                                                std::ranges::range auto && columnIntervals,
                                                TimeFrame const * sourceTimeFrame,
                                                TimeFrame const * destinationTimeFrame) {
    int64_t count = 0;

    for (auto colInterval: columnIntervals) {
        if (intervalsOverlapInAbsoluteTime(rowInterval, colInterval, sourceTimeFrame, destinationTimeFrame)) {
            ++count;
        }
    }

    return count;
}

/**
* @brief Counts the number of column intervals that overlap with the given row interval and returns their EntityIDs.
* @param rowInterval The row interval to check overlaps for.
* @param columnIntervalsWithIds The column intervals with their EntityIDs to check against.
* @param sourceTimeFrame The timeframe for the column intervals.
* @param destinationTimeFrame The timeframe for the row interval.
* @return Pair of count and vector of EntityIDs of overlapping intervals.
* @note Takes range by forwarding reference because filter_view/transform_view cache begin() internally
*/
[[nodiscard]] std::pair<int64_t, std::vector<EntityId>> countOverlappingIntervalsWithIds(TimeFrameInterval const & rowInterval,
                                                                                         std::ranges::range auto && columnIntervalsWithIds,
                                                                                         TimeFrame const * sourceTimeFrame,
                                                                                         TimeFrame const * destinationTimeFrame) {
    int64_t count = 0;
    std::vector<EntityId> overlappingEntityIds;

    for (auto const & colIntervalWithId: columnIntervalsWithIds) {
        if (intervalsOverlapInAbsoluteTime(rowInterval, colIntervalWithId.interval, sourceTimeFrame, destinationTimeFrame)) {
            ++count;
            overlappingEntityIds.push_back(colIntervalWithId.entity_id);
        }
    }

    return {count, overlappingEntityIds};
}


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
    IntervalOverlapComputer(std::shared_ptr<DigitalIntervalSeries> source,
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

        //entity_ids.reserve(rowIntervals.size());
        if (m_operation == IntervalOverlapOperation::AssignID ||
            m_operation == IntervalOverlapOperation::AssignID_Start ||
            m_operation == IntervalOverlapOperation::AssignID_End) {

            std::vector<EntityId> entity_ids;
            for (auto const & rowInterval: rowIntervals) {
                // Get view and materialize to vector since we need .empty() and indexed access
                auto columnIntervalsView = m_source->viewInRange(TimeFrameIndex(0),
                                                                 rowInterval.end,
                                                                 *destinationTimeFrame);
                std::vector<IntervalWithId> columnIntervalsWithIds;
                for (auto&& interval : columnIntervalsView) {
                    columnIntervalsWithIds.push_back(interval);
                }

                if (columnIntervalsWithIds.empty()) {
                    results.push_back(static_cast<T>(-1));
                    entity_ids.push_back(EntityId(0));
                    continue;
                }

                // Find the first column interval that overlaps with the row interval
                // by converting to absolute time and checking overlap
                auto destination_start = destinationTimeFrame->getTimeAtIndex(rowInterval.start);
                auto destination_end = destinationTimeFrame->getTimeAtIndex(rowInterval.end);
                
                bool found = false;
                size_t found_idx = 0;
                for (size_t i = 0; i < columnIntervalsWithIds.size(); ++i) {
                    auto const& colInterval = columnIntervalsWithIds[i];
                    auto source_start = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(colInterval.interval.start));
                    auto source_end = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(colInterval.interval.end));
                    
                    if (source_start <= destination_end && destination_start <= source_end) {
                        found = true;
                        found_idx = i;
                        break;  // Take the first overlapping interval
                    }
                }

                if (!found) {
                    results.push_back(static_cast<T>(-1));
                    entity_ids.push_back(EntityId(0));
                    continue;
                }

                auto const& matchedInterval = columnIntervalsWithIds[found_idx];
                auto source_start = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(matchedInterval.interval.start));
                auto source_end = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(matchedInterval.interval.end));

                if (m_operation == IntervalOverlapOperation::AssignID_Start) {
                    // Convert into row time frame
                    auto source_start_index = destinationTimeFrame->getIndexAtTime(static_cast<float>(source_start));
                    results.push_back(static_cast<T>(source_start_index.getValue()));
                    entity_ids.push_back(matchedInterval.entity_id);
                } else if (m_operation == IntervalOverlapOperation::AssignID_End) {
                    // Convert into row time frame
                    auto source_end_index = destinationTimeFrame->getIndexAtTime(static_cast<float>(source_end));
                    results.push_back(static_cast<T>(source_end_index.getValue()));
                    entity_ids.push_back(matchedInterval.entity_id);
                } else {
                    // AssignID: return the index of the first overlapping interval
                    results.push_back(static_cast<T>(found_idx));
                    entity_ids.push_back(matchedInterval.entity_id);
                }
            }

            return {results, entity_ids};// std::vector<EntityId>

        } else if (m_operation == IntervalOverlapOperation::CountOverlaps) {

            std::vector<std::vector<EntityId>> entity_ids;

            for (auto const & rowInterval: rowIntervals) {
                auto columnIntervalsWithIds = m_source->viewInRange(rowInterval.start,
                                                                    rowInterval.end,
                                                                    *destinationTimeFrame);

                auto [count, overlappingEntityIds] = countOverlappingIntervalsWithIds(rowInterval,
                                                                                      columnIntervalsWithIds,
                                                                                      sourceTimeFrame.get(),
                                                                                      destinationTimeFrame.get());

                results.push_back(static_cast<T>(count));
                entity_ids.push_back(overlappingEntityIds);
            }

            return {results, entity_ids};// std::vector<std::vector<EntityId>>
        }

        // This should never be reached, but provide a default return
        return {results, std::vector<EntityId>()};
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

    [[nodiscard]] EntityIdStructure getEntityIdStructure() const override {
        switch (m_operation) {
            case IntervalOverlapOperation::AssignID:
            case IntervalOverlapOperation::AssignID_Start:
            case IntervalOverlapOperation::AssignID_End:
                return EntityIdStructure::Simple;// One EntityID per row (the assigned interval)
                break;
            case IntervalOverlapOperation::CountOverlaps:
                return EntityIdStructure::Complex;// Multiple EntityIDs per row (all overlapping intervals)
                break;
            default:
                return EntityIdStructure::None;
        }
    }

private:
    std::shared_ptr<DigitalIntervalSeries> m_source;
    IntervalOverlapOperation m_operation;
    std::string m_sourceName;
};


#endif// INTERVAL_OVERLAP_COMPUTER_H
