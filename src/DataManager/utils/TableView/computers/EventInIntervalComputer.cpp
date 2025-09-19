#include "EventInIntervalComputer.h"

#include "utils/TableView/interfaces/IEventSource.h"

#include <algorithm>
#include <stdexcept>

/**
 * @brief Finds events within a specific interval using binary search.
 * 
 * This private helper method efficiently locates all events that fall within
 * the specified interval using binary search algorithms. It assumes the events
 * are sorted in ascending order for optimal performance.
 * 
 * @param events Span of all events, must be sorted in ascending order
 * @param startIdx Start index of the interval (inclusive)
 * @param endIdx End index of the interval (inclusive)
 * @return Vector of TimeFrameIndex values representing events within the interval
 * 
 * @pre @p events is sorted in ascending order
 * @pre @p startIdx <= @p endIdx
 * 
 * @post Result contains only events where startIdx <= event <= endIdx
 * @post Result is sorted in ascending order
 * 
 * @note Time complexity: O(log n) where n is the number of events
 */
template<typename T>
auto EventInIntervalComputer<T>::findEventsInInterval(std::span<TimeFrameIndex const> events,
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

/**
 * @brief Template specialization for bool (Presence operation).
 * 
 * Computes whether any events exist within each interval of the execution plan.
 * Returns a boolean vector where each element indicates the presence (true) or
 * absence (false) of events in the corresponding interval.
 * 
 * This specialization is optimized for detecting event occurrence patterns and
 * is commonly used for binary classification of time intervals based on event
 * presence.
 * 
 * @param plan The execution plan containing interval boundaries and time frame
 * @return Vector of boolean values indicating event presence in each interval
 * 
 * @pre @p m_operation == EventOperation::Presence
 * @pre @p plan contains valid intervals and non-null time frame
 * 
 * @post Result vector size equals plan.getIntervals().size()
 * @post Each result is true if any events exist in the corresponding interval, false otherwise
 * 
 * @throws std::runtime_error if operation type doesn't match EventOperation::Presence
 */
template<>
auto EventInIntervalComputer<bool>::compute(ExecutionPlan const & plan) const -> std::vector<bool> {
    if (m_operation != EventOperation::Presence) {
        throw std::runtime_error("EventInIntervalComputer<bool> can only be used with EventOperation::Presence");
    }

    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<bool> results;
    results.reserve(intervals.size());

    for (auto const & interval: intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        results.push_back(!events.empty());
    }

    return results;
}

/**
 * @brief Template specialization for int (Count operation).
 * 
 * Computes the number of events within each interval of the execution plan.
 * Returns an integer vector where each element represents the count of events
 * in the corresponding interval.
 * 
 * This specialization is useful for quantifying event frequency and density
 * across different time periods, commonly used in spike rate analysis and
 * event frequency studies.
 * 
 * @param plan The execution plan containing interval boundaries and time frame
 * @return Vector of integer values representing event counts in each interval
 * 
 * @pre @p m_operation == EventOperation::Count
 * @pre @p plan contains valid intervals and non-null time frame
 * 
 * @post Result vector size equals plan.getIntervals().size()
 * @post Each result is the number of events in the corresponding interval (>= 0)
 * 
 * @throws std::runtime_error if operation type doesn't match EventOperation::Count
 */
template<>
auto EventInIntervalComputer<int>::compute(ExecutionPlan const & plan) const -> std::vector<int> {
    if (m_operation != EventOperation::Count) {
        throw std::runtime_error("EventInIntervalComputer<int> can only be used with EventOperation::Count");
    }

    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<int> results;
    results.reserve(intervals.size());

    for (auto const & interval: intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        results.push_back(static_cast<int>(events.size()));
    }

    return results;
}

/**
 * @brief Template specialization for std::vector<float> (Gather operations).
 * 
 * Computes the actual event times within each interval of the execution plan.
 * Returns a vector of float vectors where each inner vector contains the
 * event times that occurred within the corresponding interval.
 * 
 * This specialization supports two modes:
 * - EventOperation::Gather: Returns absolute event times within each interval
 * - EventOperation::Gather_Center: Returns event times relative to interval center
 * 
 * This is particularly useful for detailed event analysis, spike timing studies,
 * and when the exact timing of events within intervals is required.
 * 
 * @param plan The execution plan containing interval boundaries and time frame
 * @return Vector of float vectors, each containing event times within the corresponding interval
 * 
 * @pre @p m_operation == EventOperation::Gather || m_operation == EventOperation::Gather_Center
 * @pre @p plan contains valid intervals and non-null time frame
 * @pre Source and destination time frames are compatible
 * 
 * @post Result vector size equals plan.getIntervals().size()
 * @post For Gather operation: each inner vector contains absolute event times within the interval
 * @post For Gather_Center operation: each inner vector contains event times relative to interval center
 * @post Each inner vector is sorted in ascending order
 * 
 * @throws std::runtime_error if operation type doesn't match EventOperation::Gather or EventOperation::Gather_Center
 * @throws std::runtime_error if source and destination time frames are incompatible
 */
template<>
auto EventInIntervalComputer<std::vector<float>>::compute(ExecutionPlan const & plan) const -> std::vector<std::vector<float>> {
    if (m_operation != EventOperation::Gather && m_operation != EventOperation::Gather_Center) {
        throw std::runtime_error("EventInIntervalComputer<std::vector<TimeFrameIndex>> can only be used with EventOperation::Gather");
    }

    auto intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();
    auto sourceTimeFrame = m_source->getTimeFrame();

    std::vector<std::vector<float>> results;
    results.reserve(intervals.size());

    for (auto const & interval: intervals) {

        auto events = m_source->getDataInRange(interval.start, interval.end, destinationTimeFrame.get());

        if (m_operation == EventOperation::Gather_Center) {
            auto center = (interval.start + interval.end).getValue() / 2;
            auto center_time_value = destinationTimeFrame->getTimeAtIndex(TimeFrameIndex(center));
            auto source_time_index = sourceTimeFrame->getIndexAtTime(static_cast<float>(center_time_value));
            for (auto & event: events) {
                event = event - static_cast<float>(source_time_index.getValue());
            }
        }

        results.push_back(std::move(events));
    }

    return results;
}

/**
 * @brief Computes all EntityIDs for the column.
 * 
 * For Gather and Gather_Center operations, this returns a vector of vectors where each
 * inner vector contains the EntityIDs of all events that fall within the corresponding interval.
 * For other operations, returns std::monostate (no EntityIDs).
 * 
 * @param plan The execution plan containing interval boundaries and destination time frame.
 * @return ColumnEntityIds variant containing the EntityIDs for this column.
 */
template<typename T>
ColumnEntityIds EventInIntervalComputer<T>::computeColumnEntityIds(ExecutionPlan const & plan) const {
    // Only provide EntityIDs for Gather and Gather_Center operations
    if (m_operation != EventOperation::Gather && m_operation != EventOperation::Gather_Center) {
        return std::monostate{};
    }

    auto const & intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<std::vector<EntityId>> results;
    results.reserve(intervals.size());

    // For each interval, collect EntityIDs of events that fall within it
    for (auto const & interval : intervals) {
        auto events_with_indices = m_source->getDataInRangeWithIndices(interval.start, interval.end, destinationTimeFrame.get());
        
        std::vector<EntityId> interval_entity_ids;
        interval_entity_ids.reserve(events_with_indices.size());

        // Extract EntityIDs using the source indices
        for (auto const & [event_value, source_index] : events_with_indices) {
            (void)event_value; // Suppress unused variable warning
            if (auto entityId = m_source->getEntityIdAt(source_index); entityId != 0) {
                interval_entity_ids.push_back(entityId);
            }
        }

        results.push_back(std::move(interval_entity_ids));
    }

    return results;
}

template ColumnEntityIds EventInIntervalComputer<bool>::computeColumnEntityIds(ExecutionPlan const & plan) const;
template ColumnEntityIds EventInIntervalComputer<int>::computeColumnEntityIds(ExecutionPlan const & plan) const;
template ColumnEntityIds EventInIntervalComputer<std::vector<float>>::computeColumnEntityIds(ExecutionPlan const & plan) const;