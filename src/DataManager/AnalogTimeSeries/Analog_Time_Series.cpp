#include "Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>// std::nan, std::sqrt
#include <iostream>
#include <numeric>// std::iota
#include <span>
#include <stdexcept>
#include <vector>

// ========== Constructors ==========

AnalogTimeSeries::AnalogTimeSeries()
    : _data(),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {}

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map)
    : _data(),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector)
    : _data(),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {
    setData(std::move(analog_vector), std::move(time_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples)
    : _data(),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(num_samples)) {
    if (analog_vector.size() != num_samples) {
        std::cerr << "Error: size of analog vector and number of samples are not the same!" << std::endl;
        return;
    }
    setData(std::move(analog_vector));
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector) {
    _data = std::move(analog_vector);
    // Use dense time storage for consecutive indices starting from 0
    _time_storage = TimeIndexStorageFactory::createDenseFromZero(_data.size());
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) {
    if (analog_vector.size() != time_vector.size()) {
        std::cerr << "Error: size of analog vector and time vector are not the same!" << std::endl;
        return;
    }

    _data = std::move(analog_vector);
    _time_storage = TimeIndexStorageFactory::createFromTimeIndices(std::move(time_vector));
}

void AnalogTimeSeries::setData(std::map<int, float> analog_map) {
    _data.clear();
    _data = std::vector<float>();
    auto time_storage = std::vector<TimeFrameIndex>();
    for (auto & [key, value]: analog_map) {
        time_storage.push_back(TimeFrameIndex(key));
        _data.push_back(value);
    }
    _time_storage = std::make_shared<SparseTimeIndexStorage>(std::move(time_storage));
}

// ========== Overwriting Data ==========

void AnalogTimeSeries::overwriteAtTimeIndexes(std::vector<float> & analog_data, std::vector<TimeFrameIndex> & time_indices) {
    if (analog_data.size() != time_indices.size()) {
        std::cerr << "Analog data and time indices vectors must be the same size" << std::endl;
        return;
    }

    for (size_t i = 0; i < time_indices.size(); ++i) {
        // Find the DataArrayIndex that corresponds to this TimeFrameIndex
        std::optional<DataArrayIndex> data_index = findDataArrayIndexForTimeFrameIndex(time_indices[i]);

        // Only overwrite if we found a corresponding DataArrayIndex
        if (data_index.has_value()) {
            _data[data_index.value().getValue()] = analog_data[i];
        } else {
            std::cerr << "TimeFrameIndex " << time_indices[i].getValue() << " not found in time series" << std::endl;
        }
    }
}

void AnalogTimeSeries::overwriteAtDataArrayIndexes(std::vector<float> & analog_data, std::vector<DataArrayIndex> & data_indices) {
    if (analog_data.size() != data_indices.size()) {
        std::cerr << "Analog data and data indices vectors must be the same size" << std::endl;
        return;
    }

    for (size_t i = 0; i < data_indices.size(); ++i) {
        if (data_indices[i].getValue() < _data.size()) {
            _data[data_indices[i].getValue()] = analog_data[i];
        } else {
            std::cerr << "DataArrayIndex " << data_indices[i].getValue() << " is out of bounds (data size: " << _data.size() << ")" << std::endl;
        }
    }
}

// ========== Getting Data ==========

std::span<float const> AnalogTimeSeries::getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Find the start and end indices using our boundary-finding methods
    auto start_index_opt = findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = findDataArrayIndexLessOrEqual(end_time);

    // Check if both boundaries were found
    if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
        // Return empty span if either boundary is not found
        return std::span<float const>();
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end (should always be true if our logic is correct)
    if (start_idx > end_idx) {
        // Return empty span for invalid range
        return std::span<float const>();
    }

    // Calculate the size of the range (inclusive of both endpoints)
    size_t range_size = end_idx - start_idx + 1;

    // Return span from start_idx with range_size elements
    return std::span<float const>(_data.data() + start_idx, range_size);
}


[[nodiscard]] std::span<float const> AnalogTimeSeries::getDataInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                                    TimeFrameIndex end_time,
                                                                                    TimeFrame const * source_timeFrame) const {
    if (source_timeFrame == _time_frame.get()) {
        return getDataInTimeFrameIndexRange(start_time, end_time);
    }

    // If either timeframe is null, fall back to original behavior
    if (!source_timeFrame || !_time_frame) {
        return getDataInTimeFrameIndexRange(start_time, end_time);
    }

    // Convert the time index from source timeframe to target timeframe
    // 1. Get the time value from the source timeframe
    auto start_time_value = source_timeFrame->getTimeAtIndex(start_time);
    auto end_time_value = source_timeFrame->getTimeAtIndex(end_time);

    // 2. Convert that time value to an index in the analog timeframe
    auto target_start_index = _time_frame->getIndexAtTime(static_cast<float>(start_time_value), false);
    auto target_end_index = _time_frame->getIndexAtTime(static_cast<float>(end_time_value));

    // 3. Use the converted indices to get the data in the target timeframe
    return getDataInTimeFrameIndexRange(target_start_index, target_end_index);
}


// ========== TimeFrame Support ==========

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const {
    auto position = _time_storage->findArrayPositionForTimeIndex(time_index);
    if (position.has_value()) {
        return DataArrayIndex(position.value());
    }
    return std::nullopt;
}

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const {
    auto position = _time_storage->findArrayPositionGreaterOrEqual(target_time);
    if (position.has_value()) {
        return DataArrayIndex(position.value());
    }
    return std::nullopt;
}

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const {
    auto position = _time_storage->findArrayPositionLessOrEqual(target_time);
    if (position.has_value()) {
        return DataArrayIndex(position.value());
    }
    return std::nullopt;
}

// ========== Time-Value Range Access Implementation ==========

AnalogTimeSeries::TimeValueRangeIterator::TimeValueRangeIterator(AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index, bool is_end)
    : _series(series),
      _current_index(is_end ? end_index : start_index),
      _end_index(end_index),
      _is_end(is_end) {
    if (!_is_end && _current_index.getValue() < _end_index.getValue()) {
        _updateCurrentPoint();
    }
}

AnalogTimeSeries::TimeValueRangeIterator::reference AnalogTimeSeries::TimeValueRangeIterator::operator*() const {
    if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
        throw std::out_of_range("TimeValueRangeIterator: attempt to dereference end iterator");
    }
    _updateCurrentPoint();
    return _current_point;
}

AnalogTimeSeries::TimeValueRangeIterator::pointer AnalogTimeSeries::TimeValueRangeIterator::operator->() const {
    return &(operator*());
}

AnalogTimeSeries::TimeValueRangeIterator & AnalogTimeSeries::TimeValueRangeIterator::operator++() {
    if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
        _is_end = true;
        return *this;
    }

    _current_index = DataArrayIndex(_current_index.getValue() + 1);

    if (_current_index.getValue() >= _end_index.getValue()) {
        _is_end = true;
    }

    return *this;
}

AnalogTimeSeries::TimeValueRangeIterator AnalogTimeSeries::TimeValueRangeIterator::operator++(int) {
    auto temp = *this;
    ++(*this);
    return temp;
}

bool AnalogTimeSeries::TimeValueRangeIterator::operator==(TimeValueRangeIterator const & other) const {
    return _series == other._series &&
           _current_index.getValue() == other._current_index.getValue() &&
           _is_end == other._is_end;
}

bool AnalogTimeSeries::TimeValueRangeIterator::operator!=(TimeValueRangeIterator const & other) const {
    return !(*this == other);
}

void AnalogTimeSeries::TimeValueRangeIterator::_updateCurrentPoint() const {
    if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
        return;
    }

    _current_point = TimeValuePoint(
            _series->getTimeFrameIndexAtDataArrayIndex(_current_index),
            _series->getDataAtDataArrayIndex(_current_index));
}

AnalogTimeSeries::TimeValueRangeView::TimeValueRangeView(AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index)
    : _series(series),
      _start_index(start_index),
      _end_index(end_index) {}

AnalogTimeSeries::TimeValueRangeIterator AnalogTimeSeries::TimeValueRangeView::begin() const {
    return {_series, _start_index, _end_index, false};
}

AnalogTimeSeries::TimeValueRangeIterator AnalogTimeSeries::TimeValueRangeView::end() const {
    return {_series, _start_index, _end_index, true};
}

size_t AnalogTimeSeries::TimeValueRangeView::size() const {
    if (_start_index.getValue() >= _end_index.getValue()) {
        return 0;
    }
    return _end_index.getValue() - _start_index.getValue();
}

bool AnalogTimeSeries::TimeValueRangeView::empty() const {
    return size() == 0;
}

// Dense and Sparse time index iterators for TimeIndexRange
namespace {
// Dense time index iterator implementation
class DenseTimeIndexIterator : public AnalogTimeSeries::TimeIndexIterator {
public:
    DenseTimeIndexIterator(TimeFrameIndex start_time, DataArrayIndex current_offset, DataArrayIndex end_offset, bool is_end)
        : _start_time(start_time),
          _current_offset(current_offset),
          _end_offset(end_offset),
          _current_value(TimeFrameIndex(0)),
          _is_end(is_end) {
        if (!_is_end) {
            _current_value = TimeFrameIndex(_start_time.getValue() + static_cast<int64_t>(_current_offset.getValue()));
        }
    }

    reference operator*() const override {
        if (_is_end || _current_offset.getValue() >= _end_offset.getValue()) {
            throw std::out_of_range("DenseTimeIndexIterator: attempt to dereference end iterator");
        }
        return _current_value;
    }

    TimeIndexIterator & operator++() override {
        if (_is_end || _current_offset.getValue() >= _end_offset.getValue()) {
            _is_end = true;
            return *this;
        }

        _current_offset = DataArrayIndex(_current_offset.getValue() + 1);

        if (_current_offset.getValue() >= _end_offset.getValue()) {
            _is_end = true;
        } else {
            _current_value = TimeFrameIndex(_start_time.getValue() + static_cast<int64_t>(_current_offset.getValue()));
        }

        return *this;
    }

    bool operator==(TimeIndexIterator const & other) const override {
        auto const * other_dense = dynamic_cast<DenseTimeIndexIterator const *>(&other);
        if (!other_dense) return false;

        return _start_time.getValue() == other_dense->_start_time.getValue() &&
               _current_offset.getValue() == other_dense->_current_offset.getValue() &&
               _is_end == other_dense->_is_end;
    }

    bool operator!=(TimeIndexIterator const & other) const override {
        return !(*this == other);
    }

    std::unique_ptr<TimeIndexIterator> clone() const override {
        return std::make_unique<DenseTimeIndexIterator>(_start_time, _current_offset, _end_offset, _is_end);
    }

private:
    TimeFrameIndex _start_time;
    DataArrayIndex _current_offset;
    DataArrayIndex _end_offset;
    mutable TimeFrameIndex _current_value;
    bool _is_end;
};

// Sparse time index iterator implementation
class SparseTimeIndexIterator : public AnalogTimeSeries::TimeIndexIterator {
public:
    SparseTimeIndexIterator(std::vector<TimeFrameIndex> const * time_indices, DataArrayIndex current_index, DataArrayIndex end_index, bool is_end)
        : _time_indices(time_indices),
          _current_index(current_index),
          _end_index(end_index),
          _is_end(is_end) {}

    reference operator*() const override {
        if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
            throw std::out_of_range("SparseTimeIndexIterator: attempt to dereference end iterator");
        }
        return (*_time_indices)[_current_index.getValue()];
    }

    TimeIndexIterator & operator++() override {
        if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
            _is_end = true;
            return *this;
        }

        _current_index = DataArrayIndex(_current_index.getValue() + 1);

        if (_current_index.getValue() >= _end_index.getValue()) {
            _is_end = true;
        }

        return *this;
    }

    bool operator==(TimeIndexIterator const & other) const override {
        auto const * other_sparse = dynamic_cast<SparseTimeIndexIterator const *>(&other);
        if (!other_sparse) return false;

        return _time_indices == other_sparse->_time_indices &&
               _current_index.getValue() == other_sparse->_current_index.getValue() &&
               _is_end == other_sparse->_is_end;
    }

    bool operator!=(TimeIndexIterator const & other) const override {
        return !(*this == other);
    }

    [[nodiscard]] std::unique_ptr<TimeIndexIterator> clone() const override {
        return std::make_unique<SparseTimeIndexIterator>(_time_indices, _current_index, _end_index, _is_end);
    }

private:
    std::vector<TimeFrameIndex> const * _time_indices;
    DataArrayIndex _current_index;
    DataArrayIndex _end_index;
    bool _is_end;
};
}// namespace

AnalogTimeSeries::TimeIndexRange::TimeIndexRange(AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index)
    : _series(series),
      _start_index(start_index),
      _end_index(end_index) {}

std::unique_ptr<AnalogTimeSeries::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::begin() const {
    auto storage = _series->getTimeStorage();
    
    // Try to downcast to DenseTimeIndexStorage
    if (auto dense = std::dynamic_pointer_cast<DenseTimeIndexStorage const>(storage)) {
        return std::make_unique<DenseTimeIndexIterator>(
                dense->getStartIndex(),
                _start_index,
                _end_index,
                false);
    }
    
    // Try to downcast to SparseTimeIndexStorage
    if (auto sparse = std::dynamic_pointer_cast<SparseTimeIndexStorage const>(storage)) {
        return std::make_unique<SparseTimeIndexIterator>(
                &sparse->getTimeIndices(),
                _start_index,
                _end_index,
                false);
    }
    
    // Fallback: this shouldn't happen with current implementations
    throw std::runtime_error("Unknown TimeIndexStorage type");
}

std::unique_ptr<AnalogTimeSeries::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::end() const {
    auto storage = _series->getTimeStorage();
    
    // Try to downcast to DenseTimeIndexStorage
    if (auto dense = std::dynamic_pointer_cast<DenseTimeIndexStorage const>(storage)) {
        return std::make_unique<DenseTimeIndexIterator>(
                dense->getStartIndex(),
                _start_index,
                _end_index,
                true);
    }
    
    // Try to downcast to SparseTimeIndexStorage
    if (auto sparse = std::dynamic_pointer_cast<SparseTimeIndexStorage const>(storage)) {
        return std::make_unique<SparseTimeIndexIterator>(
                &sparse->getTimeIndices(),
                _start_index,
                _end_index,
                true);
    }
    
    // Fallback: this shouldn't happen with current implementations
    throw std::runtime_error("Unknown TimeIndexStorage type");
}

size_t AnalogTimeSeries::TimeIndexRange::size() const {
    if (_start_index.getValue() >= _end_index.getValue()) {
        return 0;
    }
    return _end_index.getValue() - _start_index.getValue();
}

bool AnalogTimeSeries::TimeIndexRange::empty() const {
    return size() == 0;
}

AnalogTimeSeries::TimeValueSpanPair::TimeValueSpanPair(std::span<float const> data_span, AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index)
    : values(data_span),
      time_indices(series, start_index, end_index) {}

AnalogTimeSeries::TimeValueRangeView AnalogTimeSeries::getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Use existing boundary-finding logic
    auto start_index_opt = findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = findDataArrayIndexLessOrEqual(end_time);

    // Handle cases where boundaries are not found
    if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
        // Return empty range
        return {this, DataArrayIndex(0), DataArrayIndex(0)};
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end
    if (start_idx > end_idx) {
        // Return empty range for invalid range
        return {this, DataArrayIndex(0), DataArrayIndex(0)};
    }

    // Create range view (end_idx + 1 because end is exclusive for the range)
    return {this, DataArrayIndex(start_idx), DataArrayIndex(end_idx + 1)};
}

AnalogTimeSeries::TimeValueSpanPair AnalogTimeSeries::getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Use existing getDataInTimeFrameIndexRange for the span
    auto data_span = getDataInTimeFrameIndexRange(start_time, end_time);

    // Find the corresponding array indices for the time iterator
    auto start_index_opt = findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = findDataArrayIndexLessOrEqual(end_time);

    // Handle cases where boundaries are not found
    if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
        // Return empty span pair
        return {std::span<float const>(), this, DataArrayIndex(0), DataArrayIndex(0)};
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end
    if (start_idx > end_idx) {
        // Return empty span pair for invalid range
        return {std::span<float const>(), this, DataArrayIndex(0), DataArrayIndex(0)};
    }

    // Create span pair (end_idx + 1 because end is exclusive for the range)
    return {data_span, this, DataArrayIndex(start_idx), DataArrayIndex(end_idx + 1)};
}

AnalogTimeSeries::TimeValueSpanPair AnalogTimeSeries::getTimeValueSpanInTimeFrameIndexRange(
        TimeFrameIndex start_time,
        TimeFrameIndex end_time,
        TimeFrame const * source_timeFrame) const {
    
    if (source_timeFrame == _time_frame.get()) {
        return getTimeValueSpanInTimeFrameIndexRange(start_time, end_time);
    }

    // If either timeframe is null, fall back to original behavior
    if (!source_timeFrame || !_time_frame) {
        return getTimeValueSpanInTimeFrameIndexRange(start_time, end_time);
    }

    // Convert the time index from source timeframe to target timeframe
    // 1. Get the time value from the source timeframe
    auto start_time_value = source_timeFrame->getTimeAtIndex(start_time);
    auto end_time_value = source_timeFrame->getTimeAtIndex(end_time);

    // 2. Convert that time value to an index in the analog timeframe
    auto target_start_index = _time_frame->getIndexAtTime(static_cast<float>(start_time_value), false);
    auto target_end_index = _time_frame->getIndexAtTime(static_cast<float>(end_time_value));

    // 3. Use the converted indices to get the data in the target timeframe
    return getTimeValueSpanInTimeFrameIndexRange(target_start_index, target_end_index);
}