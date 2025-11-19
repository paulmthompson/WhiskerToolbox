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
    : _data_storage(VectorAnalogDataStorage(std::vector<float>())),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {
    _cacheOptimizationPointers();
}

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map)
    : _data_storage(VectorAnalogDataStorage(std::vector<float>())),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector)
    : _data_storage(VectorAnalogDataStorage(std::vector<float>())),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(0)) {
    setData(std::move(analog_vector), std::move(time_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples)
    : _data_storage(VectorAnalogDataStorage(std::vector<float>())),
      _time_storage(TimeIndexStorageFactory::createDenseFromZero(num_samples)) {
    if (analog_vector.size() != num_samples) {
        std::cerr << "Error: size of analog vector and number of samples are not the same!" << std::endl;
        return;
    }
    setData(std::move(analog_vector));
}

// Private constructor for factory methods
AnalogTimeSeries::AnalogTimeSeries(DataStorageWrapper storage, std::vector<TimeFrameIndex> time_vector)
    : _data_storage(std::move(storage)),
      _time_storage(TimeIndexStorageFactory::createFromTimeIndices(std::move(time_vector))) {
    _cacheOptimizationPointers();
}

// ========== Factory Methods ==========

std::shared_ptr<AnalogTimeSeries> AnalogTimeSeries::createMemoryMapped(
    MmapStorageConfig config,
    std::vector<TimeFrameIndex> time_vector) {
    
    // Create memory-mapped storage
    auto mmap_storage = MemoryMappedAnalogDataStorage(std::move(config));
    
    // Validate that time vector matches data size
    size_t num_samples = mmap_storage.size();
    if (time_vector.size() != num_samples) {
        throw std::runtime_error(
            "Time vector size (" + std::to_string(time_vector.size()) + 
            ") does not match data size (" + std::to_string(num_samples) + ")");
    }
    
    // Wrap in type-erased storage
    DataStorageWrapper storage_wrapper(std::move(mmap_storage));
    
    // Use make_shared with private constructor access via friendship or helper
    // Since we can't use make_shared with private constructors directly, use new
    return std::shared_ptr<AnalogTimeSeries>(
        new AnalogTimeSeries(std::move(storage_wrapper), std::move(time_vector)));
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector) {
    size_t size = analog_vector.size();
    _data_storage = DataStorageWrapper(VectorAnalogDataStorage(std::move(analog_vector)));
    _time_storage = TimeIndexStorageFactory::createDenseFromZero(size);
    _cacheOptimizationPointers();
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) {
    if (analog_vector.size() != time_vector.size()) {
        std::cerr << "Error: size of analog vector and time vector are not the same!" << std::endl;
        return;
    }

    _data_storage = DataStorageWrapper(VectorAnalogDataStorage(std::move(analog_vector)));
    _time_storage = TimeIndexStorageFactory::createFromTimeIndices(std::move(time_vector));
    _cacheOptimizationPointers();
}

void AnalogTimeSeries::setData(std::map<int, float> analog_map) {
    std::vector<float> data_vec;
    std::vector<TimeFrameIndex> time_vec;
    data_vec.reserve(analog_map.size());
    time_vec.reserve(analog_map.size());
    
    for (auto & [key, value]: analog_map) {
        time_vec.push_back(TimeFrameIndex(key));
        data_vec.push_back(value);
    }
    
    _data_storage = DataStorageWrapper(VectorAnalogDataStorage(std::move(data_vec)));
    _time_storage = std::make_shared<SparseTimeIndexStorage>(std::move(time_vec));
    _cacheOptimizationPointers();
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

    // Use storage's getSpanRange for efficient access
    return _data_storage.getSpanRange(start_idx, end_idx + 1);
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
      _is_end(is_end),
      _contiguous_data_ptr(series->_contiguous_data_ptr) {
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

    // Fast path: direct pointer access for contiguous data (no virtual call)
    float value;
    if (_contiguous_data_ptr != nullptr) {
        value = _contiguous_data_ptr[_current_index.getValue()];
    } else {
        // Slow path: go through storage wrapper (virtual dispatch)
        value = _series->_getDataAtDataArrayIndex(_current_index);
    }

    _current_point = TimeValuePoint(
            _series->_getTimeFrameIndexAtDataArrayIndex(_current_index),
            value);
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
    return {this, DataArrayIndex(0), DataArrayIndex(_data_storage.size())};
}