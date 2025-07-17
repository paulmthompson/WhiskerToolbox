#include "IntervalOverlapComputer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

template<typename T>
bool IntervalOverlapComputer<T>::intervalsOverlap(const TimeFrameInterval& a, const TimeFrameInterval& b) const {
    // Two intervals overlap if: a.start <= b.end && b.start <= a.end
    return a.start.getValue() <= b.end.getValue() && b.start.getValue() <= a.end.getValue();
}

template<typename T>
int64_t IntervalOverlapComputer<T>::findContainingInterval(const TimeFrameInterval& rowInterval,
                                                          const std::vector<Interval>& columnIntervals) const {
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

template<typename T>
int64_t IntervalOverlapComputer<T>::countOverlappingIntervals(const TimeFrameInterval& rowInterval,
                                                             const std::vector<Interval>& columnIntervals) const {
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

// Template specialization for int64_t
template<>
auto IntervalOverlapComputer<int64_t>::compute(const ExecutionPlan& plan) const -> std::vector<int64_t> {
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalOverlapComputer requires an ExecutionPlan with intervals");
    }
    
    auto rowIntervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    
    std::vector<int64_t> results;
    results.reserve(rowIntervals.size());
    
    // Get all column intervals from the source
    // We need to get the full range of column intervals
    // For now, we'll get all intervals from the source
    auto columnIntervals = m_source->getIntervalsInRange(
        TimeFrameIndex(std::numeric_limits<int64_t>::min()),
        TimeFrameIndex(std::numeric_limits<int64_t>::max()),
        destinationTimeFrame.get()
    );
    
    for (const auto& rowInterval : rowIntervals) {
        switch (m_operation) {
            case IntervalOverlapOperation::AssignID:
                results.push_back(findContainingInterval(rowInterval, columnIntervals));
                break;
            case IntervalOverlapOperation::CountOverlaps:
                results.push_back(countOverlappingIntervals(rowInterval, columnIntervals));
                break;
            default:
                throw std::runtime_error("Unknown IntervalOverlapOperation");
        }
    }
    
    return results;
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
    auto columnIntervals = m_source->getIntervalsInRange(
        TimeFrameIndex(std::numeric_limits<int64_t>::min()),
        TimeFrameIndex(std::numeric_limits<int64_t>::max()),
        destinationTimeFrame.get()
    );
    
    for (const auto& rowInterval : rowIntervals) {
        int64_t count = countOverlappingIntervals(rowInterval, columnIntervals);
        results.push_back(static_cast<size_t>(count));
    }
    
    return results;
}
