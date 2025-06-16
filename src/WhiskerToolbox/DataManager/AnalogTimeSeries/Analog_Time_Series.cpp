#include "Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>// std::nan, std::sqrt
#include <iostream>
#include <numeric>// std::iota
#include <vector>
#include <stdexcept>
#include <span>

// ========== Constructors ==========

AnalogTimeSeries::AnalogTimeSeries() :
 _data(),
 _time_storage(DenseTimeRange(TimeFrameIndex(0), 0)),
 _timeframe_v2() {}

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map) :
 _data(),
 _time_storage(DenseTimeRange(TimeFrameIndex(0), 0)),
 _timeframe_v2() {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) :
_data(),
_time_storage(DenseTimeRange(TimeFrameIndex(0), 0)),
_timeframe_v2() {
    setData(std::move(analog_vector), std::move(time_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples) :
_data(),
_time_storage(DenseTimeRange(TimeFrameIndex(0), num_samples)),
_timeframe_v2() {
    if (analog_vector.size() != num_samples) {
        std::cerr << "Error: size of analog vector and number of samples are not the same!" << std::endl;
        return;
    }
    setData(std::move(analog_vector));
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector) {
    _data = std::move(analog_vector);
    // Use dense time storage for consecutive indices starting from 0
    _time_storage = DenseTimeRange(TimeFrameIndex(0), _data.size());
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) {
    if (analog_vector.size() != time_vector.size()) {
        std::cerr << "Error: size of analog vector and time vector are not the same!" << std::endl;
        return;
    }
    
    _data = std::move(analog_vector);
    
    // Check if we can use dense storage (consecutive indices)
    bool is_dense = true;
    if (!time_vector.empty()) {
        TimeFrameIndex expected_value = time_vector[0];
        for (size_t i = 0; i < time_vector.size(); ++i) {
            if (time_vector[i] != expected_value + TimeFrameIndex(static_cast<int64_t>(i))) {
                is_dense = false;
                break;
            }
        }
    }
    
    if (is_dense && !time_vector.empty()) {
        _time_storage = DenseTimeRange(TimeFrameIndex(time_vector[0]), time_vector.size());
    } else {
        _time_storage = SparseTimeIndices(std::move(time_vector));
    }
}

void AnalogTimeSeries::setData(std::map<int, float> analog_map) {
    _data.clear();
    _data = std::vector<float>();
    auto time_storage = std::vector<TimeFrameIndex>();
    for (auto & [key, value]: analog_map) {
        time_storage.push_back(TimeFrameIndex(key));
        _data.push_back(value);
    }
    _time_storage = SparseTimeIndices(std::move(time_storage));
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

// ========== TimeFrame Support ==========

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const {
    return std::visit([time_index](auto const & time_storage) -> std::optional<DataArrayIndex> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            // For dense storage, check if the TimeFrameIndex falls within our range
            TimeFrameIndex start = time_storage.start_time_frame_index;
            TimeFrameIndex end = TimeFrameIndex(start.getValue() + static_cast<int64_t>(time_storage.count) - 1);
            
            if (time_index >= start && time_index <= end) {
                // Calculate the DataArrayIndex
                size_t offset = static_cast<size_t>(time_index.getValue() - start.getValue());
                return DataArrayIndex(offset);
            }
            return std::nullopt;
        } else {
            // For sparse storage, search for the TimeFrameIndex
            auto it = std::find(time_storage.time_frame_indices.begin(), time_storage.time_frame_indices.end(), time_index);
            if (it != time_storage.time_frame_indices.end()) {
                size_t index = static_cast<size_t>(std::distance(time_storage.time_frame_indices.begin(), it));
                return DataArrayIndex(index);
            }
            return std::nullopt;
        }
    }, _time_storage);
}

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const {
    return std::visit([target_time](auto const & time_storage) -> std::optional<DataArrayIndex> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            // For dense storage, calculate the position if target_time falls within our range
            TimeFrameIndex start = time_storage.start_time_frame_index;
            TimeFrameIndex end = TimeFrameIndex(start.getValue() + static_cast<int64_t>(time_storage.count) - 1);
            
            if (target_time <= end) {
                // Find the first index >= target_time
                TimeFrameIndex effective_start = (target_time >= start) ? target_time : start;
                size_t offset = static_cast<size_t>(effective_start.getValue() - start.getValue());
                return DataArrayIndex(offset);
            }
            return std::nullopt;
        } else {
            // For sparse storage, use lower_bound to find first element >= target_time
            auto it = std::lower_bound(time_storage.time_frame_indices.begin(), time_storage.time_frame_indices.end(), target_time);
            if (it != time_storage.time_frame_indices.end()) {
                size_t index = static_cast<size_t>(std::distance(time_storage.time_frame_indices.begin(), it));
                return DataArrayIndex(index);
            }
            return std::nullopt;
        }
    }, _time_storage);
}

std::optional<DataArrayIndex> AnalogTimeSeries::findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const {
    return std::visit([target_time](auto const & time_storage) -> std::optional<DataArrayIndex> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            // For dense storage, calculate the position if target_time falls within our range
            TimeFrameIndex start = time_storage.start_time_frame_index;
            TimeFrameIndex end = TimeFrameIndex(start.getValue() + static_cast<int64_t>(time_storage.count) - 1);
            
            if (target_time >= start) {
                // Find the last index <= target_time
                TimeFrameIndex effective_end = (target_time <= end) ? target_time : end;
                size_t offset = static_cast<size_t>(effective_end.getValue() - start.getValue());
                return DataArrayIndex(offset);
            }
            return std::nullopt;
        } else {
            // For sparse storage, use upper_bound to find first element > target_time, then step back
            auto it = std::upper_bound(time_storage.time_frame_indices.begin(), time_storage.time_frame_indices.end(), target_time);
            if (it != time_storage.time_frame_indices.begin()) {
                --it; // Step back to get the last element <= target_time
                size_t index = static_cast<size_t>(std::distance(time_storage.time_frame_indices.begin(), it));
                return DataArrayIndex(index);
            }
            return std::nullopt;
        }
    }, _time_storage);
}

std::span<const float> AnalogTimeSeries::getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Find the start and end indices using our boundary-finding methods
    auto start_index_opt = findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = findDataArrayIndexLessOrEqual(end_time);

    // Check if both boundaries were found
    if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
        // Return empty span if either boundary is not found
        return std::span<const float>();
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end (should always be true if our logic is correct)
    if (start_idx > end_idx) {
        // Return empty span for invalid range
        return std::span<const float>();
    }

    // Calculate the size of the range (inclusive of both endpoints)
    size_t range_size = end_idx - start_idx + 1;

    // Return span from start_idx with range_size elements
    return std::span<const float>(_data.data() + start_idx, range_size);
}

std::span<const float> AnalogTimeSeries::getDataSpanInCoordinateRange(TimeCoordinate start_coord, TimeCoordinate end_coord) const {
    // Note: This implementation is simplified and may not return a contiguous span
    // for all cases. For non-contiguous data, consider using getDataInCoordinateRange() instead.
    throw std::runtime_error("getDataSpanInCoordinateRange not fully implemented due to potential non-contiguous data. Use getDataInCoordinateRange() instead.");
}

// ========== Time-Value Range Access Implementation ==========

AnalogTimeSeries::TimeValueRangeIterator::TimeValueRangeIterator(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index, bool is_end)
    : _series(series), _current_index(is_end ? end_index : start_index), _end_index(end_index), _is_end(is_end) {
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

AnalogTimeSeries::TimeValueRangeIterator& AnalogTimeSeries::TimeValueRangeIterator::operator++() {
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

bool AnalogTimeSeries::TimeValueRangeIterator::operator==(TimeValueRangeIterator const& other) const {
    return _series == other._series && 
           _current_index.getValue() == other._current_index.getValue() && 
           _is_end == other._is_end;
}

bool AnalogTimeSeries::TimeValueRangeIterator::operator!=(TimeValueRangeIterator const& other) const {
    return !(*this == other);
}

void AnalogTimeSeries::TimeValueRangeIterator::_updateCurrentPoint() const {
    if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
        return;
    }
    
    _current_point = TimeValuePoint(
        _series->getTimeFrameIndexAtDataArrayIndex(_current_index),
        _series->getDataAtDataArrayIndex(_current_index)
    );
}

AnalogTimeSeries::TimeValueRangeView::TimeValueRangeView(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index)
    : _series(series), _start_index(start_index), _end_index(end_index) {}

AnalogTimeSeries::TimeValueRangeIterator AnalogTimeSeries::TimeValueRangeView::begin() const {
    return TimeValueRangeIterator(_series, _start_index, _end_index, false);
}

AnalogTimeSeries::TimeValueRangeIterator AnalogTimeSeries::TimeValueRangeView::end() const {
    return TimeValueRangeIterator(_series, _start_index, _end_index, true);
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
            : _start_time(start_time), _current_offset(current_offset), _end_offset(end_offset), 
              _current_value(TimeFrameIndex(0)), _is_end(is_end) {
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

        TimeIndexIterator& operator++() override {
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

        bool operator==(TimeIndexIterator const& other) const override {
            auto const* other_dense = dynamic_cast<DenseTimeIndexIterator const*>(&other);
            if (!other_dense) return false;
            
            return _start_time.getValue() == other_dense->_start_time.getValue() &&
                   _current_offset.getValue() == other_dense->_current_offset.getValue() &&
                   _is_end == other_dense->_is_end;
        }

        bool operator!=(TimeIndexIterator const& other) const override {
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
        SparseTimeIndexIterator(std::vector<TimeFrameIndex> const* time_indices, DataArrayIndex current_index, DataArrayIndex end_index, bool is_end)
            : _time_indices(time_indices), _current_index(current_index), _end_index(end_index), _is_end(is_end) {}

        reference operator*() const override {
            if (_is_end || _current_index.getValue() >= _end_index.getValue()) {
                throw std::out_of_range("SparseTimeIndexIterator: attempt to dereference end iterator");
            }
            return (*_time_indices)[_current_index.getValue()];
        }

        TimeIndexIterator& operator++() override {
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

        bool operator==(TimeIndexIterator const& other) const override {
            auto const* other_sparse = dynamic_cast<SparseTimeIndexIterator const*>(&other);
            if (!other_sparse) return false;
            
            return _time_indices == other_sparse->_time_indices &&
                   _current_index.getValue() == other_sparse->_current_index.getValue() &&
                   _is_end == other_sparse->_is_end;
        }

        bool operator!=(TimeIndexIterator const& other) const override {
            return !(*this == other);
        }

        std::unique_ptr<TimeIndexIterator> clone() const override {
            return std::make_unique<SparseTimeIndexIterator>(_time_indices, _current_index, _end_index, _is_end);
        }

    private:
        std::vector<TimeFrameIndex> const* _time_indices;
        DataArrayIndex _current_index;
        DataArrayIndex _end_index;
        bool _is_end;
    };
}

AnalogTimeSeries::TimeIndexRange::TimeIndexRange(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index)
    : _series(series), _start_index(start_index), _end_index(end_index) {}

std::unique_ptr<AnalogTimeSeries::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::begin() const {
    return std::visit([this](auto const & time_storage) -> std::unique_ptr<TimeIndexIterator> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            // Dense storage - create iterator based on start time and offset
            return std::make_unique<DenseTimeIndexIterator>(
                time_storage.start_time_frame_index, 
                _start_index, 
                _end_index, 
                false
            );
        } else {
            // Sparse storage - create iterator with direct index access
            return std::make_unique<SparseTimeIndexIterator>(
                &time_storage.time_frame_indices,
                _start_index,
                _end_index,
                false
            );
        }
    }, _series->getTimeStorage());
}

std::unique_ptr<AnalogTimeSeries::TimeIndexIterator> AnalogTimeSeries::TimeIndexRange::end() const {
    return std::visit([this](auto const & time_storage) -> std::unique_ptr<TimeIndexIterator> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            // Dense storage - create end iterator
            return std::make_unique<DenseTimeIndexIterator>(
                time_storage.start_time_frame_index, 
                _start_index, 
                _end_index, 
                true
            );
        } else {
            // Sparse storage - create end iterator
            return std::make_unique<SparseTimeIndexIterator>(
                &time_storage.time_frame_indices,
                _start_index,
                _end_index,
                true
            );
        }
    }, _series->getTimeStorage());
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

AnalogTimeSeries::TimeValueSpanPair::TimeValueSpanPair(std::span<const float> data_span, AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index)
    : values(data_span), time_indices(series, start_index, end_index) {}

AnalogTimeSeries::TimeValueRangeView AnalogTimeSeries::getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
    // Use existing boundary-finding logic
    auto start_index_opt = findDataArrayIndexGreaterOrEqual(start_time);
    auto end_index_opt = findDataArrayIndexLessOrEqual(end_time);

    // Handle cases where boundaries are not found
    if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
        // Return empty range
        return TimeValueRangeView(this, DataArrayIndex(0), DataArrayIndex(0));
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end
    if (start_idx > end_idx) {
        // Return empty range for invalid range
        return TimeValueRangeView(this, DataArrayIndex(0), DataArrayIndex(0));
    }

    // Create range view (end_idx + 1 because end is exclusive for the range)
    return TimeValueRangeView(this, DataArrayIndex(start_idx), DataArrayIndex(end_idx + 1));
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
        return TimeValueSpanPair(std::span<const float>(), this, DataArrayIndex(0), DataArrayIndex(0));
    }

    size_t start_idx = start_index_opt.value().getValue();
    size_t end_idx = end_index_opt.value().getValue();

    // Validate that start <= end
    if (start_idx > end_idx) {
        // Return empty span pair for invalid range
        return TimeValueSpanPair(std::span<const float>(), this, DataArrayIndex(0), DataArrayIndex(0));
    }

    // Create span pair (end_idx + 1 because end is exclusive for the range)
    return TimeValueSpanPair(data_span, this, DataArrayIndex(start_idx), DataArrayIndex(end_idx + 1));
}