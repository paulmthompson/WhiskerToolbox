#include "Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>// std::nan, std::sqrt
#include <iostream>
#include <numeric>// std::iota
#include <vector>
#include <stdexcept>
#include <span>

AnalogTimeSeries::AnalogTimeSeries() :
 _data(),
 _time_storage(DenseTimeRange(0, 0)),
 _timeframe_v2() {}

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map) :
 _data(),
 _time_storage(DenseTimeRange(0, 0)),
 _timeframe_v2() {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector) :
 _data(),
 _time_storage(DenseTimeRange(0, 0)),
 _timeframe_v2() {
    setData(std::move(analog_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector) :
_data(),
_time_storage(DenseTimeRange(0, 0)),
_timeframe_v2() {
    setData(std::move(analog_vector), std::move(time_vector));
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector) {
    _data = std::move(analog_vector);
    // Use dense time storage for consecutive indices starting from 0
    _time_storage = DenseTimeRange(0, _data.size());
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
    if (analog_vector.size() != time_vector.size()) {
        std::cerr << "Error: size of analog vector and time vector are not the same!" << std::endl;
        return;
    }
    
    _data = std::move(analog_vector);
    
    // Check if we can use dense storage (consecutive indices)
    bool is_dense = true;
    if (!time_vector.empty()) {
        size_t expected_value = time_vector[0];
        for (size_t i = 0; i < time_vector.size(); ++i) {
            if (time_vector[i] != expected_value + i) {
                is_dense = false;
                break;
            }
        }
    }
    
    if (is_dense && !time_vector.empty()) {
        _time_storage = DenseTimeRange(time_vector[0], time_vector.size());
    } else {
        _time_storage = SparseTimeIndices(std::move(time_vector));
    }
}

void AnalogTimeSeries::setData(std::map<int, float> analog_map) {
    _data.clear();
    _data = std::vector<float>();
    auto time_storage = std::vector<size_t>();
    for (auto & [key, value]: analog_map) {
        time_storage.push_back(key);
        _data.push_back(value);
    }
    _time_storage = SparseTimeIndices(std::move(time_storage));
}

void AnalogTimeSeries::overwriteAtTimes(std::vector<float> & analog_data, std::vector<size_t> & time) {
    if (analog_data.size() != time.size()) {
        std::cerr << "Analog data and time vectors must be the same size" << std::endl;
        return;
    }

    auto indices = std::visit([](auto const & time_storage) -> std::vector<size_t> {
        if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
            return std::vector<size_t>(time_storage.count);
        } else {
            return time_storage.indices;
        }
    }, _time_storage);
    for (size_t i = 0; i < time.size(); ++i) {
        auto it = std::find(indices.begin(), indices.end(), time[i]);
        if (it != indices.end()) {
            _data[std::distance(indices.begin(), it)] = analog_data[i];
        } else {
            std::cerr << "Time " << time[i] << " not found in time series" << std::endl;
        }
    }
}

std::span<const float> AnalogTimeSeries::getDataSpanInCoordinateRange(TimeCoordinate start_coord, TimeCoordinate end_coord) const {
    if (!_timeframe_v2.has_value()) {
        throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
    }
    return std::visit([&](auto const & timeframe_ptr) -> std::span<const float> {
        using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
        using FrameCoordType = typename FrameType::coordinate_type;
        if (!std::holds_alternative<FrameCoordType>(start_coord) ||
            !std::holds_alternative<FrameCoordType>(end_coord)) {
            throw std::runtime_error("Coordinate type mismatch with TimeFrameV2. Expected: " + getCoordinateType());
        }
        auto start_typed = std::get<FrameCoordType>(start_coord);
        auto end_typed = std::get<FrameCoordType>(end_coord);
        TimeFrameIndex start_idx = timeframe_ptr->getIndexAtTime(start_typed);
        TimeFrameIndex end_idx = timeframe_ptr->getIndexAtTime(end_typed);
        if (start_idx > end_idx) std::swap(start_idx, end_idx);
        // Clamp indices to valid range
        start_idx = std::max<TimeFrameIndex>(TimeFrameIndex(0), start_idx);
        end_idx = std::min<TimeFrameIndex>(TimeFrameIndex(_data.size() - 1), end_idx);
        if (start_idx > end_idx || start_idx >= TimeFrameIndex(static_cast<int64_t>(_data.size()))) {
            return std::span<const float>(); // empty span
        }

        //Find indexes in _timeStorage that correspond to start_idx and end_idx
        auto start_idx_in_storage = std::visit([start_idx](auto const & time_storage) -> size_t {
            if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
                return time_storage.start_index + start_idx.getValue();
            } else {
                return time_storage.indices[start_idx.getValue()];
            }
        }, _time_storage);
        auto end_idx_in_storage = std::visit([end_idx](auto const & time_storage) -> size_t {
            if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
                return time_storage.start_index + end_idx.getValue();
            } else {
                return time_storage.indices[end_idx.getValue()];
            }
        }, _time_storage);

        return std::span<const float>(&_data[start_idx_in_storage], 
                                      static_cast<size_t>(end_idx_in_storage - start_idx_in_storage + 1));
    }, _timeframe_v2.value());
}