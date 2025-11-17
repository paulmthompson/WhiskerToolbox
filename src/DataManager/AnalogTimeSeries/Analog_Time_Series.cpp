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

// ========== Getting Data ==========

std::span<float const> AnalogTimeSeries::getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Find the start and end indices using our boundary-finding methods
    auto start_index_opt = _findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = _findDataArrayIndexLessOrEqual(end_time);

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

std::optional<DataArrayIndex> AnalogTimeSeries::_findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const {
    auto position = _time_storage->findArrayPositionForTimeIndex(time_index);
    if (position.has_value()) {
        return DataArrayIndex(position.value());
    }
    return std::nullopt;
}

std::optional<DataArrayIndex> AnalogTimeSeries::_findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const {
    auto position = _time_storage->findArrayPositionGreaterOrEqual(target_time);
    if (position.has_value()) {
        return DataArrayIndex(position.value());
    }
    return std::nullopt;
}

std::optional<DataArrayIndex> AnalogTimeSeries::_findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const {
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
            _series->_getTimeFrameIndexAtDataArrayIndex(_current_index),
            _series->_getDataAtDataArrayIndex(_current_index));
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

// ========== TimeIndexRange Implementation ==========

AnalogTimeSeries::TimeIndexRange::TimeIndexRange(AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index)
    : _series(series),
      _start_index(start_index),
      _end_index(end_index) {}

std::unique_ptr<::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::begin() const {
    return _series->getTimeStorage()->createIterator(_start_index.getValue(), _end_index.getValue(), false);
}

std::unique_ptr<::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::end() const {
    return _series->getTimeStorage()->createIterator(_start_index.getValue(), _end_index.getValue(), true);
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
    auto start_index_opt = _findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = _findDataArrayIndexLessOrEqual(end_time);

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
    auto start_index_opt = _findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = _findDataArrayIndexLessOrEqual(end_time);

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

AnalogTimeSeries::TimeValueRangeView AnalogTimeSeries::getAllSamples() const {
    // Return a range view over all samples (from index 0 to size)
    return {this, DataArrayIndex(0), DataArrayIndex(_data.size())};
}