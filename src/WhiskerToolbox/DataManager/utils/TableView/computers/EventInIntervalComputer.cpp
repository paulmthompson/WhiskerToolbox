#include "EventInIntervalComputer.h"

#include <algorithm>
#include <stdexcept>

template<typename T>
auto EventInIntervalComputer<T>::findEventsInInterval(std::span<const TimeFrameIndex> events,
                                                     TimeFrameIndex startIdx,
                                                     TimeFrameIndex endIdx) const -> std::vector<TimeFrameIndex> {
    std::vector<TimeFrameIndex> result;
    
    // Use binary search to find the range of events within the interval
    auto startIt = std::lower_bound(events.begin(), events.end(), startIdx);
    auto endIt = std::upper_bound(events.begin(), events.end(), endIdx);
    
    // Copy events within the interval
    result.reserve(static_cast<size_t>(std::distance(startIt, endIt)));
    result.assign(startIt, endIt);
    
    return result;
}

// Template specialization for bool (Presence operation)
template<>
auto EventInIntervalComputer<bool>::compute(const ExecutionPlan& plan) const -> std::vector<bool> {
    if (m_operation != EventOperation::Presence) {
        throw std::runtime_error("EventInIntervalComputer<bool> can only be used with EventOperation::Presence");
    }
    
    auto events = m_source->getEvents();
    auto intervals = plan.getIntervals();
    
    std::vector<bool> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        // Use binary search to check if any events exist in the interval
        auto startIt = std::lower_bound(events.begin(), events.end(), interval.start);
        auto endIt = std::upper_bound(events.begin(), events.end(), interval.end);
        
        // If startIt != endIt, there are events in the interval
        results.push_back(startIt != endIt);
    }
    
    return results;
}

// Template specialization for int (Count operation)
template<>
auto EventInIntervalComputer<int>::compute(const ExecutionPlan& plan) const -> std::vector<int> {
    if (m_operation != EventOperation::Count) {
        throw std::runtime_error("EventInIntervalComputer<int> can only be used with EventOperation::Count");
    }
    
    auto events = m_source->getEvents();
    auto intervals = plan.getIntervals();
    
    std::vector<int> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        // Use binary search to count events in the interval
        auto startIt = std::lower_bound(events.begin(), events.end(), interval.start);
        auto endIt = std::upper_bound(events.begin(), events.end(), interval.end);
        
        results.push_back(static_cast<int>(std::distance(startIt, endIt)));
    }
    
    return results;
}

// Template specialization for std::vector<TimeFrameIndex> (Gather operation)
template<>
auto EventInIntervalComputer<std::vector<TimeFrameIndex>>::compute(const ExecutionPlan& plan) const -> std::vector<std::vector<TimeFrameIndex>> {
    if (m_operation != EventOperation::Gather) {
        throw std::runtime_error("EventInIntervalComputer<std::vector<TimeFrameIndex>> can only be used with EventOperation::Gather");
    }
    
    auto events = m_source->getEvents();
    auto intervals = plan.getIntervals();
    
    std::vector<std::vector<TimeFrameIndex>> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        results.push_back(findEventsInInterval(events,
                                               interval.start, interval.end));
    }
    
    return results;
}
