#include "ExecutionPlan.h"

ExecutionPlan::ExecutionPlan(std::vector<TimeFrameIndex> indices, std::shared_ptr<TimeFrame> timeFrame)
    : m_indices(std::move(indices)),
      m_timeFrame(std::move(timeFrame)) {
}

ExecutionPlan::ExecutionPlan(std::vector<TimeFrameInterval> intervals, std::shared_ptr<TimeFrame> timeFrame)
    : m_intervals(std::move(intervals)),
      m_timeFrame(std::move(timeFrame)) {
}

std::vector<TimeFrameIndex> const & ExecutionPlan::getIndices() const {
    return m_indices;
}

std::vector<TimeFrameInterval> const & ExecutionPlan::getIntervals() const {
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
