#include "IRowSelector.h"

// IndexSelector implementation
IndexSelector::IndexSelector(std::vector<size_t> indices)
    : m_indices(std::move(indices))
{
}

size_t IndexSelector::getRowCount() const {
    return m_indices.size();
}

const std::vector<size_t>& IndexSelector::getIndices() const {
    return m_indices;
}

// TimestampSelector implementation
TimestampSelector::TimestampSelector(std::vector<double> timestamps)
    : m_timestamps(std::move(timestamps))
{
}

size_t TimestampSelector::getRowCount() const {
    return m_timestamps.size();
}

const std::vector<double>& TimestampSelector::getTimestamps() const {
    return m_timestamps;
}

// IntervalSelector implementation
IntervalSelector::IntervalSelector(std::vector<TimeFrameInterval> intervals)
    : m_intervals(std::move(intervals))
{
}

size_t IntervalSelector::getRowCount() const {
    return m_intervals.size();
}

const std::vector<TimeFrameInterval>& IntervalSelector::getIntervals() const {
    return m_intervals;
}
