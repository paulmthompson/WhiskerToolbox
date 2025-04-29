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

float AnalogTimeSeries::getMeanValue() const {
    return std::accumulate(_data.begin(), _data.end(), 0.0f) / static_cast<float>(_data.size());
}

float AnalogTimeSeries::getMeanValue(int64_t start, int64_t end) const {
    return std::accumulate(_data.begin() + start, _data.begin() + end, 0.0f) / static_cast<float>(end - start);
}

float AnalogTimeSeries::getStdDevValue() const {
    float const mean = getMeanValue();
    float const sum = std::accumulate(_data.begin(), _data.end(), 0.0f,
                                      [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
    return std::sqrt(sum / static_cast<float>(_data.size()));
}

float AnalogTimeSeries::getStdDevValue(int64_t start, int64_t end) const {
    float const mean = getMeanValue(start, end);
    float const sum = std::accumulate(_data.begin() + start, _data.begin() + end, 0.0f,
                                      [mean](float acc, float val) { return acc + (val - mean) * (val - mean); });
    return std::sqrt(sum / static_cast<float>((end - start)));
}

float AnalogTimeSeries::getMinValue() const {
    return *std::min_element(_data.begin(), _data.end());
}

float AnalogTimeSeries::getMinValue(int64_t start, int64_t end) const {
    return *std::min_element(_data.begin() + start, _data.begin() + end);
}

float AnalogTimeSeries::getMaxValue() const {
    return *std::max_element(_data.begin(), _data.end());
}

float AnalogTimeSeries::getMaxValue(int64_t start, int64_t end) const {
    return *std::max_element(_data.begin() + start, _data.begin() + end);
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
