#include "IRowSelector.h"

IRowSelector::~IRowSelector() = default;

// IndexSelector implementation
IndexSelector::IndexSelector(std::vector<size_t> indices)
    : m_indices(std::move(indices)) {
}

size_t IndexSelector::getRowCount() const {
    return m_indices.size();
}

std::vector<size_t> const & IndexSelector::getIndices() const {
    return m_indices;
}

RowDescriptor IndexSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_indices.size()) {
        return m_indices[row_index];
    }
    return std::monostate{};
}

// TimestampSelector implementation
TimestampSelector::TimestampSelector(std::vector<TimeFrameIndex> timestamps, std::shared_ptr<TimeFrame> timeFrame)
    : m_timestamps(std::move(timestamps)),
      m_timeFrame(std::move(timeFrame)) {
}

size_t TimestampSelector::getRowCount() const {
    return m_timestamps.size();
}

std::vector<TimeFrameIndex> const & TimestampSelector::getTimestamps() const {
    return m_timestamps;
}

RowDescriptor TimestampSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_timestamps.size()) {
        return m_timestamps[row_index];
    }
    return std::monostate{};
}

// IntervalSelector implementation
IntervalSelector::IntervalSelector(std::vector<TimeFrameInterval> intervals, std::shared_ptr<TimeFrame> timeFrame)
    : m_intervals(std::move(intervals)),
      m_timeFrame(std::move(timeFrame)) {
}

size_t IntervalSelector::getRowCount() const {
    return m_intervals.size();
}

std::vector<TimeFrameInterval> const & IntervalSelector::getIntervals() const {
    return m_intervals;
}

RowDescriptor IntervalSelector::getDescriptor(size_t row_index) const {
    if (row_index < m_intervals.size()) {
        return m_intervals[row_index];
    }
    return std::monostate{};
}
