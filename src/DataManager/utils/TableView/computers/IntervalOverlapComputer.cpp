#include "IntervalOverlapComputer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <iostream> // Added for debug output


bool intervalsOverlap(const TimeFrameInterval& a, const TimeFrameInterval& b) {
    // Two intervals overlap if: a.start <= b.end && b.start <= a.end
    return a.start.getValue() <= b.end.getValue() && b.start.getValue() <= a.end.getValue();
}

int64_t findContainingInterval(const TimeFrameInterval& rowInterval,
                              const std::vector<Interval>& columnIntervals) {
    for (size_t i = 0; i < columnIntervals.size(); ++i) {
        const auto& colInterval = columnIntervals[i];
        
        // Check if column interval contains the row interval
        // Column interval contains row interval if: colInterval.start <= rowInterval.start && rowInterval.end <= colInterval.end
        if (colInterval.start <= rowInterval.start.getValue() && rowInterval.end.getValue() <= colInterval.end) {
            return static_cast<int64_t>(i);
        }
    }
    return -1; // No containing interval found
}

int64_t countOverlappingIntervals(const TimeFrameInterval& rowInterval,
                                 const std::vector<Interval>& columnIntervals,
                                 const TimeFrame* sourceTimeFrame,
                                 const TimeFrame* destinationTimeFrame) {
    int64_t count = 0;
    
    // Convert row interval to absolute time coordinates
    auto destination_start = destinationTimeFrame->getTimeAtIndex(rowInterval.start);
    auto destination_end = destinationTimeFrame->getTimeAtIndex(rowInterval.end);
    
    for (const auto& colInterval : columnIntervals) {
        // Convert column interval to absolute time coordinates
        auto source_start = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(colInterval.start));
        auto source_end = sourceTimeFrame->getTimeAtIndex(TimeFrameIndex(colInterval.end));
        
        // Check overlap using absolute time coordinates
        // Two intervals overlap if: source_start <= destination_end && destination_start <= source_end
        if (source_start <= destination_end && destination_start <= source_end) {
            ++count;
        }
    }
    
    return count;
}

// Template specialization for size_t
template<>
std::pair<std::vector<size_t>, ColumnEntityIds> IntervalOverlapComputer<size_t>::compute(const ExecutionPlan& plan) const {
    if (m_operation != IntervalOverlapOperation::CountOverlaps) {
        throw std::runtime_error("IntervalOverlapComputer<size_t> can only be used with CountOverlaps operation");
    }
    
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalOverlapComputer requires an ExecutionPlan with intervals");
    }
    
    auto rowIntervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    auto sourceTimeFrame = m_source->getTimeFrame();
    
    std::vector<size_t> results;
    results.reserve(rowIntervals.size());
    std::vector<std::vector<EntityId>> entity_ids;
    // Get all column intervals from the source
    // Use a reasonable range that covers the entire time frame
    auto columnIntervals = m_source->getIntervalsInRange(
        TimeFrameIndex(0),
        TimeFrameIndex(1000000), // Use a large but reasonable upper bound
        destinationTimeFrame.get()
    );
    
    std::cout << "DEBUG: Retrieved " << columnIntervals.size() << " column intervals" << std::endl;
    for (size_t i = 0; i < columnIntervals.size(); ++i) {
        const auto& interval = columnIntervals[i];
        std::cout << "DEBUG: Column interval " << i << ": [" << interval.start << ", " << interval.end << "]" << std::endl;
    }
    
    for (const auto& rowInterval : rowIntervals) {
        int64_t count = countOverlappingIntervals(rowInterval, columnIntervals, sourceTimeFrame.get(), destinationTimeFrame.get());
        results.push_back(static_cast<size_t>(count));
    }
    
    return {results, entity_ids};
}

// Template specializations for different data types
template std::pair<std::vector<int64_t>, ColumnEntityIds> IntervalOverlapComputer<int64_t>::compute(ExecutionPlan const & plan) const;
template std::pair<std::vector<size_t>, ColumnEntityIds> IntervalOverlapComputer<size_t>::compute(ExecutionPlan const & plan) const;
template std::pair<std::vector<double>, ColumnEntityIds> IntervalOverlapComputer<double>::compute(ExecutionPlan const & plan) const;
