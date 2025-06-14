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