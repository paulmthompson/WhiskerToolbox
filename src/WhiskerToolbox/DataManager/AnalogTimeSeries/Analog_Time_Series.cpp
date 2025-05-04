#include "Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>// std::nan, std::sqrt
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

AnalogTimeSeries::AnalogTimeSeries(std::map<int, float> analog_map) {
    setData(std::move(analog_map));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector) {
    setData(std::move(analog_vector));
}

AnalogTimeSeries::AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
    setData(std::move(analog_vector), std::move(time_vector));
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector) {
    _data.clear();
    _data = std::move(analog_vector);
    _time = std::vector<size_t>(_data.size());
    std::iota(_time.begin(), _time.end(), 0);
}

void AnalogTimeSeries::setData(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
    _data.clear();
    _time.clear();
    if (analog_vector.size() != time_vector.size()) {
        std::cerr << "Error: size of analog vector and time vector are not the same!" << std::endl;
    }
    _data = std::move(analog_vector);
    _time = std::move(time_vector);
}

void AnalogTimeSeries::setData(std::map<int, float> analog_map) {
    _data.clear();
    _time.clear();
    _data = std::vector<float>();
    _time = std::vector<size_t>();
    for (auto & [key, value]: analog_map) {
        _time.push_back(key);
        _data.push_back(value);
    }
}

void AnalogTimeSeries::overwriteAtTimes(std::vector<float> & analog_data, std::vector<size_t> & time) {
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

float calculate_mean(AnalogTimeSeries const & series) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }
    return std::accumulate(data.begin(), data.end(), 0.0f) / static_cast<float>(data.size());
}

float calculate_mean(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty() || start >= end || start < 0 || end > static_cast<int64_t>(data.size())) {
        return 0.0f;
    }
    return std::accumulate(data.begin() + start, data.begin() + end, 0.0f) /
           static_cast<float>(end - start);
}

float calculate_std_dev(AnalogTimeSeries const & series) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }

    float const mean = calculate_mean(series);
    float const sum = std::accumulate(data.begin(), data.end(), 0.0f,
                                      [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
    return std::sqrt(sum / static_cast<float>(data.size()));
}

float calculate_std_dev(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty() || start >= end || start < 0 || end > static_cast<int64_t>(data.size())) {
        return 0.0f;
    }

    float const mean = calculate_mean(series, start, end);
    float const sum = std::accumulate(data.begin() + start, data.begin() + end, 0.0f,
                                      [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
    return std::sqrt(sum / static_cast<float>((end - start)));
}

float calculate_min(AnalogTimeSeries const & series) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }
    return *std::min_element(data.begin(), data.end());
}

float calculate_min(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty() || start >= end || start < 0 || end > static_cast<int64_t>(data.size())) {
        return 0.0f;
    }
    return *std::min_element(data.begin() + start, data.begin() + end);
}

float calculate_max(AnalogTimeSeries const & series) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }
    return *std::max_element(data.begin(), data.end());
}

float calculate_max(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty() || start >= end || start < 0 || end > static_cast<int64_t>(data.size())) {
        return 0.0f;
    }
    return *std::max_element(data.begin() + start, data.begin() + end);
}

void save_analog(
        std::vector<float> const & analog_series,
        std::vector<size_t> const & time_series,
        std::string const & block_output) {
    std::fstream fout;
    fout.open(block_output, std::fstream::out);

    for (size_t i = 0; i < analog_series.size(); ++i) {
        fout << time_series[i] << "," << analog_series[i] << "\n";
    }

    fout.close();
}
