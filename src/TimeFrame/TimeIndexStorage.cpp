#include "TimeIndexStorage.hpp"

#include <algorithm>
#include <string>
#include <stdexcept>

// ========== Iterator Implementations ==========

namespace {
// Dense time index iterator implementation
class DenseTimeIndexIteratorImpl : public TimeIndexIterator {
public:
    DenseTimeIndexIteratorImpl(TimeFrameIndex start_time, size_t current_offset, size_t end_offset, bool is_end)
        : _start_time(start_time),
          _current_offset(current_offset),
          _end_offset(end_offset),
          _current_value(TimeFrameIndex(0)),
          _is_end(is_end) {
        if (!_is_end) {
            _current_value = TimeFrameIndex(_start_time.getValue() + static_cast<int64_t>(_current_offset));
        }
    }

    reference operator*() const override {
        if (_is_end || _current_offset >= _end_offset) {
            throw std::out_of_range("DenseTimeIndexIterator: attempt to dereference end iterator");
        }
        return _current_value;
    }

    TimeIndexIterator & operator++() override {
        if (_is_end || _current_offset >= _end_offset) {
            _is_end = true;
            return *this;
        }

        ++_current_offset;

        if (_current_offset >= _end_offset) {
            _is_end = true;
        } else {
            _current_value = TimeFrameIndex(_start_time.getValue() + static_cast<int64_t>(_current_offset));
        }

        return *this;
    }

    bool operator==(TimeIndexIterator const & other) const override {
        auto const * other_dense = dynamic_cast<DenseTimeIndexIteratorImpl const *>(&other);
        if (!other_dense) return false;

        return _start_time.getValue() == other_dense->_start_time.getValue() &&
               _current_offset == other_dense->_current_offset &&
               _is_end == other_dense->_is_end;
    }

    bool operator!=(TimeIndexIterator const & other) const override {
        return !(*this == other);
    }

    std::unique_ptr<TimeIndexIterator> clone() const override {
        return std::make_unique<DenseTimeIndexIteratorImpl>(_start_time, _current_offset, _end_offset, _is_end);
    }

private:
    TimeFrameIndex _start_time;
    size_t _current_offset;
    size_t _end_offset;
    mutable TimeFrameIndex _current_value;
    bool _is_end;
};

// Sparse time index iterator implementation
class SparseTimeIndexIteratorImpl : public TimeIndexIterator {
public:
    SparseTimeIndexIteratorImpl(std::vector<TimeFrameIndex> const * time_indices, size_t current_index, size_t end_index, bool is_end)
        : _time_indices(time_indices),
          _current_index(current_index),
          _end_index(end_index),
          _is_end(is_end) {}

    reference operator*() const override {
        if (_is_end || _current_index >= _end_index) {
            throw std::out_of_range("SparseTimeIndexIterator: attempt to dereference end iterator");
        }
        return (*_time_indices)[_current_index];
    }

    TimeIndexIterator & operator++() override {
        if (_is_end || _current_index >= _end_index) {
            _is_end = true;
            return *this;
        }

        ++_current_index;

        if (_current_index >= _end_index) {
            _is_end = true;
        }

        return *this;
    }

    bool operator==(TimeIndexIterator const & other) const override {
        auto const * other_sparse = dynamic_cast<SparseTimeIndexIteratorImpl const *>(&other);
        if (!other_sparse) return false;

        return _time_indices == other_sparse->_time_indices &&
               _current_index == other_sparse->_current_index &&
               _is_end == other_sparse->_is_end;
    }

    bool operator!=(TimeIndexIterator const & other) const override {
        return !(*this == other);
    }

    std::unique_ptr<TimeIndexIterator> clone() const override {
        return std::make_unique<SparseTimeIndexIteratorImpl>(_time_indices, _current_index, _end_index, _is_end);
    }

private:
    std::vector<TimeFrameIndex> const * _time_indices;
    size_t _current_index;
    size_t _end_index;
    bool _is_end;
};
}// namespace

// ========== DenseTimeIndexStorage Implementation ==========

DenseTimeIndexStorage::DenseTimeIndexStorage(TimeFrameIndex start_index, size_t count)
    : _start_index(start_index),
      _count(count) {}

TimeFrameIndex DenseTimeIndexStorage::getTimeFrameIndexAt(size_t array_position) const {
    if (array_position >= _count) {
        throw std::out_of_range("Array position " + std::to_string(array_position) +
                                " is out of bounds (size: " + std::to_string(_count) + ")");
    }
    return TimeFrameIndex(_start_index.getValue() + static_cast<int64_t>(array_position));
}

size_t DenseTimeIndexStorage::size() const {
    return _count;
}

std::optional<size_t> DenseTimeIndexStorage::findArrayPositionForTimeIndex(TimeFrameIndex time_index) const {
    TimeFrameIndex end_index(_start_index.getValue() + static_cast<int64_t>(_count) - 1);

    if (time_index >= _start_index && time_index <= end_index) {
        size_t position = static_cast<size_t>(time_index.getValue() - _start_index.getValue());
        return position;
    }
    return std::nullopt;
}

std::optional<size_t> DenseTimeIndexStorage::findArrayPositionGreaterOrEqual(TimeFrameIndex target_time) const {
    TimeFrameIndex end_index(_start_index.getValue() + static_cast<int64_t>(_count) - 1);

    if (target_time <= end_index) {
        // If target is before our range, return the first position
        if (target_time <= _start_index) {
            return 0;
        }
        // Otherwise calculate the position
        size_t position = static_cast<size_t>(target_time.getValue() - _start_index.getValue());
        return position;
    }
    return std::nullopt;
}

std::optional<size_t> DenseTimeIndexStorage::findArrayPositionLessOrEqual(TimeFrameIndex target_time) const {
    if (target_time >= _start_index) {
        TimeFrameIndex end_index(_start_index.getValue() + static_cast<int64_t>(_count) - 1);
        
        // If target is after our range, return the last position
        if (target_time >= end_index) {
            return _count - 1;
        }
        // Otherwise calculate the position
        size_t position = static_cast<size_t>(target_time.getValue() - _start_index.getValue());
        return position;
    }
    return std::nullopt;
}

std::vector<TimeFrameIndex> DenseTimeIndexStorage::getAllTimeIndices() const {
    std::vector<TimeFrameIndex> result;
    result.reserve(_count);
    for (size_t i = 0; i < _count; ++i) {
        result.push_back(TimeFrameIndex(_start_index.getValue() + static_cast<int64_t>(i)));
    }
    return result;
}

std::shared_ptr<TimeIndexStorage> DenseTimeIndexStorage::clone() const {
    return std::make_shared<DenseTimeIndexStorage>(_start_index, _count);
}

std::unique_ptr<TimeIndexIterator> DenseTimeIndexStorage::createIterator(size_t start_position, size_t end_position, bool is_end) const {
    return std::make_unique<DenseTimeIndexIteratorImpl>(_start_index, start_position, end_position, is_end);
}

// ========== SparseTimeIndexStorage Implementation ==========

SparseTimeIndexStorage::SparseTimeIndexStorage(std::vector<TimeFrameIndex> time_indices)
    : _time_indices(std::move(time_indices)) {
    // Ensure indices are sorted for binary search operations
    std::sort(_time_indices.begin(), _time_indices.end());
}

TimeFrameIndex SparseTimeIndexStorage::getTimeFrameIndexAt(size_t array_position) const {
    if (array_position >= _time_indices.size()) {
        throw std::out_of_range("Array position " + std::to_string(array_position) +
                                " is out of bounds (size: " + std::to_string(_time_indices.size()) + ")");
    }
    return _time_indices[array_position];
}

size_t SparseTimeIndexStorage::size() const {
    return _time_indices.size();
}

std::optional<size_t> SparseTimeIndexStorage::findArrayPositionForTimeIndex(TimeFrameIndex time_index) const {
    auto it = std::find(_time_indices.begin(), _time_indices.end(), time_index);
    if (it != _time_indices.end()) {
        return static_cast<size_t>(std::distance(_time_indices.begin(), it));
    }
    return std::nullopt;
}

std::optional<size_t> SparseTimeIndexStorage::findArrayPositionGreaterOrEqual(TimeFrameIndex target_time) const {
    auto it = std::lower_bound(_time_indices.begin(), _time_indices.end(), target_time);
    if (it != _time_indices.end()) {
        return static_cast<size_t>(std::distance(_time_indices.begin(), it));
    }
    return std::nullopt;
}

std::optional<size_t> SparseTimeIndexStorage::findArrayPositionLessOrEqual(TimeFrameIndex target_time) const {
    auto it = std::upper_bound(_time_indices.begin(), _time_indices.end(), target_time);
    if (it != _time_indices.begin()) {
        --it;
        return static_cast<size_t>(std::distance(_time_indices.begin(), it));
    }
    return std::nullopt;
}

std::vector<TimeFrameIndex> SparseTimeIndexStorage::getAllTimeIndices() const {
    return _time_indices;
}

std::shared_ptr<TimeIndexStorage> SparseTimeIndexStorage::clone() const {
    return std::make_shared<SparseTimeIndexStorage>(_time_indices);
}

std::unique_ptr<TimeIndexIterator> SparseTimeIndexStorage::createIterator(size_t start_position, size_t end_position, bool is_end) const {
    return std::make_unique<SparseTimeIndexIteratorImpl>(&_time_indices, start_position, end_position, is_end);
}

// ========== Factory Functions ==========

namespace TimeIndexStorageFactory {

std::shared_ptr<TimeIndexStorage> createFromTimeIndices(std::vector<TimeFrameIndex> time_indices) {
    if (time_indices.empty()) {
        return std::make_shared<DenseTimeIndexStorage>(TimeFrameIndex(0), 0);
    }

    // Check if indices are consecutive
    bool is_consecutive = true;
    TimeFrameIndex expected = time_indices[0];
    for (size_t i = 0; i < time_indices.size(); ++i) {
        if (time_indices[i] != TimeFrameIndex(expected.getValue() + static_cast<int64_t>(i))) {
            is_consecutive = false;
            break;
        }
    }

    if (is_consecutive) {
        return std::make_shared<DenseTimeIndexStorage>(time_indices[0], time_indices.size());
    } else {
        return std::make_shared<SparseTimeIndexStorage>(std::move(time_indices));
    }
}

std::shared_ptr<TimeIndexStorage> createDenseFromZero(size_t count) {
    return std::make_shared<DenseTimeIndexStorage>(TimeFrameIndex(0), count);
}

std::shared_ptr<TimeIndexStorage> createDense(TimeFrameIndex start_index, size_t count) {
    return std::make_shared<DenseTimeIndexStorage>(start_index, count);
}

}// namespace TimeIndexStorageFactory
