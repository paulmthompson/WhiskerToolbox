#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <algorithm>
#include <cmath>// std::nan
#include <concepts>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

/**
 * @brief The AnalogTimeSeries class
 *
 * Analog time series is used for storing continuous data
 * The data may be sampled at irregular intervals as long as the time vector is provided
 *
 */
class AnalogTimeSeries : public ObserverData {
public:
    AnalogTimeSeries() = default;
    explicit AnalogTimeSeries(std::vector<float> analog_vector);
    AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector);
    explicit AnalogTimeSeries(std::map<int, float> analog_map);

    void setData(std::vector<float> analog_vector) {
        _data.clear();
        _data = std::move(analog_vector);
        _time = std::vector<size_t>(_data.size());
        std::iota(_time.begin(), _time.end(), 0);
    };
    void setData(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
        _data.clear();
        _time.clear();
        _data = std::move(analog_vector);
        _time = std::move(time_vector);
    };
    void setData(std::map<int, float> analog_map) {
        _data.clear();
        _time.clear();
        _data = std::vector<float>();
        _time = std::vector<size_t>();
        for (auto & [key, value]: analog_map) {
            _time.push_back(key);
            _data.push_back(value);
        }
    };

    template<typename T>
    void overwriteAtTimes(std::vector<float> & analog_data, std::vector<T> & time) {
        if (analog_data.size() != time.size()) {
            std::cerr << "Analog data and time vectors must be the same size" << std::endl;
            return;
        }
        for (size_t i = 0; i < time.size(); ++i) {
            auto it = std::find(_time.begin(), _time.end(), time[i]);
            if (it != _time.end()) {
                _data[std::distance(_time.begin(), it)] = analog_data[i];
            } else {
                std::cerr << "Time " << time[i] << " not found in time series" << std::endl;
            }
        }
    }

    std::vector<float> & getAnalogTimeSeries() { return _data; };
    std::vector<size_t> & getTimeSeries() { return _time; };

    [[nodiscard]] float getMeanValue() const {
        return std::accumulate(_data.begin(), _data.end(), 0.0f) / static_cast<float>(_data.size());
    }

    [[nodiscard]] float getMeanValue(int64_t start, int64_t end) const {
        return std::accumulate(_data.begin() + start, _data.begin() + end, 0.0f) / static_cast<float>(end - start);
    }

    [[nodiscard]] float getStdDevValue() const {
        float const mean = getMeanValue();
        float const sum = std::accumulate(_data.begin(), _data.end(), 0.0f,
                                          [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
        return std::sqrt(sum / static_cast<float>(_data.size()));
    }

    [[nodiscard]] float getStdDevValue(int64_t start, int64_t end) const {
        float const mean = getMeanValue(start, end);
        float const sum = std::accumulate(_data.begin() + start, _data.begin() + end, 0.0f,
                                          [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
        return std::sqrt(sum / static_cast<float>((end - start)));
    }

    [[nodiscard]] float getMinValue() const {
        return *std::min_element(_data.begin(), _data.end());
    }

    [[nodiscard]] float getMinValue(int64_t start, int64_t end) const {
        return *std::min_element(_data.begin() + start, _data.begin() + end);
    }

    [[nodiscard]] float getMaxValue() const {
        return *std::max_element(_data.begin(), _data.end());
    }

    [[nodiscard]] float getMaxValue(int64_t start, int64_t end) const {
        return *std::max_element(_data.begin() + start, _data.begin() + end);
    }

    template<typename TransformFunc = std::identity>
    auto getDataInRange(float start_time, float stop_time,
                        TransformFunc && time_transform = {}) const {
        struct DataPoint {
            size_t time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, _data.size()) |
               std::views::filter([this, start_time, stop_time, time_transform](size_t i) {
                   auto transformed_time = time_transform(_time[i]);
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{_time[i], _data[i]};
               });
    }

    template<typename TransformFunc = std::identity>
    std::pair<std::vector<size_t>, std::vector<float>> getDataVectorsInRange(
            float start_time, float stop_time,
            TransformFunc && time_transform = {}) const {
        std::vector<size_t> times;
        std::vector<float> values;

        auto range = getDataInRange(start_time, stop_time, time_transform);
        for (auto const & point: range) {
            times.push_back(point.time_idx);
            values.push_back(point.value);
        }

        return {times, values};
    }

protected:
private:
    std::vector<float> _data;
    std::vector<size_t> _time;
};

/**
 * @brief load_analog_series_from_csv
 *
 *
 *
 * @param filename - the name of the file to load
 * @return a vector of floats representing the analog time series
 */
std::vector<float> load_analog_series_from_csv(std::string const & filename);

void save_analog(
        std::vector<float> const & analog_series,
        std::vector<size_t> const & time_series,
        std::string block_output);

#endif// ANALOG_TIME_SERIES_HPP
