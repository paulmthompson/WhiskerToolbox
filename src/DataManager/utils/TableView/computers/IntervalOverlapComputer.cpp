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
                                 const std::vector<Interval>& columnIntervals) {
    int64_t count = 0;
    
    for (const auto& colInterval : columnIntervals) {
        // Convert Interval to TimeFrameInterval for comparison
        TimeFrameInterval colTimeFrameInterval{TimeFrameIndex(colInterval.start), TimeFrameIndex(colInterval.end)};
        
        if (intervalsOverlap(rowInterval, colTimeFrameInterval)) {
            ++count;
        }
    }
    
    return count;
}

// Template specialization for size_t
template<>
auto IntervalOverlapComputer<size_t>::compute(const ExecutionPlan& plan) const -> std::vector<size_t> {
    if (m_operation != IntervalOverlapOperation::CountOverlaps) {
        throw std::runtime_error("IntervalOverlapComputer<size_t> can only be used with CountOverlaps operation");
    }
    
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalOverlapComputer requires an ExecutionPlan with intervals");
    }
    
    auto rowIntervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    
    std::vector<size_t> results;
    results.reserve(rowIntervals.size());
    
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
        int64_t count = countOverlappingIntervals(rowInterval, columnIntervals);
        results.push_back(static_cast<size_t>(count));
    }
    
    return results;
}

// Template specializations for different data types
template std::vector<int64_t> IntervalOverlapComputer<int64_t>::compute(ExecutionPlan const & plan) const;
template std::vector<size_t> IntervalOverlapComputer<size_t>::compute(ExecutionPlan const & plan) const;
template std::vector<double> IntervalOverlapComputer<double>::compute(ExecutionPlan const & plan) const;
