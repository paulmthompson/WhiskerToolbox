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
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<bool> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        results.push_back(!events.empty());
    }
    
    return results;
}

// Template specialization for int (Count operation)
template<>
auto EventInIntervalComputer<int>::compute(const ExecutionPlan& plan) const -> std::vector<int> {
    if (m_operation != EventOperation::Count) {
        throw std::runtime_error("EventInIntervalComputer<int> can only be used with EventOperation::Count");
    }
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<int> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        results.push_back(static_cast<int>(events.size()));
    }

    return results;
}

// Template specialization for std::vector<TimeFrameIndex> (Gather operation)
template<>
auto EventInIntervalComputer<std::vector<float>>::compute(const ExecutionPlan& plan) const -> std::vector<std::vector<float>> {
    if (m_operation != EventOperation::Gather) {
        throw std::runtime_error("EventInIntervalComputer<std::vector<TimeFrameIndex>> can only be used with EventOperation::Gather");
    }
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<std::vector<float>> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        results.push_back(std::move(events));
    }

    return results;
}

