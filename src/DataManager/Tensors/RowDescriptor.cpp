#include "RowDescriptor.hpp"

#include <stdexcept>

RowDescriptor RowDescriptor::ordinal(std::size_t count) {
    RowDescriptor rd;
    rd._type = RowType::Ordinal;
    rd._ordinal_count = count;
    return rd;
}

RowDescriptor RowDescriptor::fromTimeIndices(
    std::shared_ptr<TimeIndexStorage> storage,
    std::shared_ptr<TimeFrame> time_frame) {
    if (!storage) {
        throw std::invalid_argument("RowDescriptor::fromTimeIndices: storage must not be null");
    }
    if (!time_frame) {
        throw std::invalid_argument("RowDescriptor::fromTimeIndices: time_frame must not be null");
    }
    RowDescriptor rd;
    rd._type = RowType::TimeFrameIndex;
    rd._time_storage = std::move(storage);
    rd._time_frame = std::move(time_frame);
    return rd;
}

RowDescriptor RowDescriptor::fromIntervals(
    std::vector<TimeFrameInterval> intervals,
    std::shared_ptr<TimeFrame> time_frame) {
    if (!time_frame) {
        throw std::invalid_argument("RowDescriptor::fromIntervals: time_frame must not be null");
    }
    RowDescriptor rd;
    rd._type = RowType::Interval;
    rd._intervals = std::move(intervals);
    rd._time_frame = std::move(time_frame);
    return rd;
}

std::size_t RowDescriptor::count() const noexcept {
    switch (_type) {
        case RowType::Ordinal:
            return _ordinal_count;
        case RowType::TimeFrameIndex:
            return _time_storage ? _time_storage->size() : 0;
        case RowType::Interval:
            return _intervals.size();
    }
    return 0; // unreachable
}

TimeIndexStorage const & RowDescriptor::timeStorage() const {
    if (_type != RowType::TimeFrameIndex) {
        throw std::logic_error(
            "RowDescriptor::timeStorage: row type is not TimeFrameIndex");
    }
    return *_time_storage;
}

std::shared_ptr<TimeIndexStorage> RowDescriptor::timeStoragePtr() const {
    if (_type != RowType::TimeFrameIndex) {
        throw std::logic_error(
            "RowDescriptor::timeStoragePtr: row type is not TimeFrameIndex");
    }
    return _time_storage;
}

std::span<TimeFrameInterval const> RowDescriptor::intervals() const {
    if (_type != RowType::Interval) {
        throw std::logic_error(
            "RowDescriptor::intervals: row type is not Interval");
    }
    return _intervals;
}

RowDescriptor::RowLabel RowDescriptor::labelAt(std::size_t row) const {
    if (row >= count()) {
        throw std::out_of_range(
            "RowDescriptor::labelAt: row " + std::to_string(row) +
            " out of range (count=" + std::to_string(count()) + ")");
    }
    switch (_type) {
        case RowType::Ordinal:
            return row;
        case RowType::TimeFrameIndex:
            return _time_storage->getTimeFrameIndexAt(row);
        case RowType::Interval:
            return _intervals[row];
    }
    return std::monostate{}; // unreachable
}

bool RowDescriptor::operator==(RowDescriptor const & other) const {
    if (_type != other._type) {
        return false;
    }
    switch (_type) {
        case RowType::Ordinal:
            return _ordinal_count == other._ordinal_count;
        case RowType::TimeFrameIndex:
            // Compare by size and contents (both storages must produce same indices)
            if (_time_storage->size() != other._time_storage->size()) {
                return false;
            }
            for (std::size_t i = 0; i < _time_storage->size(); ++i) {
                if (_time_storage->getTimeFrameIndexAt(i) !=
                    other._time_storage->getTimeFrameIndexAt(i)) {
                    return false;
                }
            }
            return true;
        case RowType::Interval:
            return _intervals == other._intervals;
    }
    return false; // unreachable
}
