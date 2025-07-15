#include "ExecutionPlan.h"

ExecutionPlan::ExecutionPlan(std::vector<TimeFrameIndex> indices)
    : m_indices(std::move(indices))
{
}

ExecutionPlan::ExecutionPlan(std::vector<TimeFrameInterval> intervals)
    : m_intervals(std::move(intervals))
{
}

const std::vector<TimeFrameIndex>& ExecutionPlan::getIndices() const {
    return m_indices;
}

const std::vector<TimeFrameInterval>& ExecutionPlan::getIntervals() const {
    return m_intervals;
}

bool ExecutionPlan::hasIndices() const {
    return !m_indices.empty();
}

bool ExecutionPlan::hasIntervals() const {
    return !m_intervals.empty();
}

void ExecutionPlan::setIndices(std::vector<TimeFrameIndex> indices) {
    m_indices = std::move(indices);
    // Clear intervals to maintain consistency
    m_intervals.clear();
}

void ExecutionPlan::setIntervals(std::vector<TimeFrameInterval> intervals) {
    m_intervals = std::move(intervals);
    // Clear indices to maintain consistency
    m_indices.clear();
}
