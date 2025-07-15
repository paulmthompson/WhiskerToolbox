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

RowDescriptor IndexSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_indices.size()) {
        return m_indices[row_index];
    }
    return std::monostate{};
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

RowDescriptor TimestampSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_timestamps.size()) {
        return m_timestamps[row_index];
    }
    return std::monostate{};
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

RowDescriptor IntervalSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_intervals.size()) {
        return m_intervals[row_index];
    }
    return std::monostate{};
}
