#include "IntervalPropertyComputer.h"

#include <stdexcept>

// Template specialization for int64_t
template<>
auto IntervalPropertyComputer<int64_t>::compute(const ExecutionPlan& plan) const -> std::vector<int64_t> {
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalPropertyComputer requires an ExecutionPlan with intervals");
    }
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    
    std::vector<int64_t> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        switch (m_property) {
            case IntervalProperty::Start:
                results.push_back(interval.start.getValue());
                break;
            case IntervalProperty::End:
                results.push_back(interval.end.getValue());
                break;
            case IntervalProperty::Duration:
                results.push_back(interval.end.getValue() - interval.start.getValue());
                break;
            default:
                throw std::runtime_error("Unknown IntervalProperty");
        }
    }
    
    return results;
}

// Template specialization for float
template<>
auto IntervalPropertyComputer<float>::compute(const ExecutionPlan& plan) const -> std::vector<float> {
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalPropertyComputer requires an ExecutionPlan with intervals");
    }
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    
    std::vector<float> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        switch (m_property) {
            case IntervalProperty::Start:
                if (destinationTimeFrame) {
                    results.push_back(static_cast<float>(destinationTimeFrame->getTimeAtIndex(interval.start)));
                } else {
                    results.push_back(static_cast<float>(interval.start.getValue()));
                }
                break;
            case IntervalProperty::End:
                if (destinationTimeFrame) {
                    results.push_back(static_cast<float>(destinationTimeFrame->getTimeAtIndex(interval.end)));
                } else {
                    results.push_back(static_cast<float>(interval.end.getValue()));
                }
                break;
            case IntervalProperty::Duration:
                if (destinationTimeFrame) {
                    auto startTime = destinationTimeFrame->getTimeAtIndex(interval.start);
                    auto endTime = destinationTimeFrame->getTimeAtIndex(interval.end);
                    results.push_back(static_cast<float>(endTime - startTime));
                } else {
                    results.push_back(static_cast<float>(interval.end.getValue() - interval.start.getValue()));
                }
                break;
            default:
                throw std::runtime_error("Unknown IntervalProperty");
        }
    }
    
    return results;
}

// Template specialization for double
template<>
auto IntervalPropertyComputer<double>::compute(const ExecutionPlan& plan) const -> std::vector<double> {
    if (!plan.hasIntervals()) {
        throw std::runtime_error("IntervalPropertyComputer requires an ExecutionPlan with intervals");
    }
    
    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    
    std::vector<double> results;
    results.reserve(intervals.size());
    
    for (const auto& interval : intervals) {
        switch (m_property) {
            case IntervalProperty::Start:
                if (destinationTimeFrame) {
                    results.push_back(destinationTimeFrame->getTimeAtIndex(interval.start));
                } else {
                    results.push_back(static_cast<double>(interval.start.getValue()));
                }
                break;
            case IntervalProperty::End:
                if (destinationTimeFrame) {
                    results.push_back(destinationTimeFrame->getTimeAtIndex(interval.end));
                } else {
                    results.push_back(static_cast<double>(interval.end.getValue()));
                }
                break;
            case IntervalProperty::Duration:
                if (destinationTimeFrame) {
                    auto startTime = destinationTimeFrame->getTimeAtIndex(interval.start);
                    auto endTime = destinationTimeFrame->getTimeAtIndex(interval.end);
                    results.push_back(endTime - startTime);
                } else {
                    results.push_back(static_cast<double>(interval.end.getValue() - interval.start.getValue()));
                }
                break;
            default:
                throw std::runtime_error("Unknown IntervalProperty");
        }
    }
    
    return results;
}
