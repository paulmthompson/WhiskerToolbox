#include "Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>// std::nan, std::sqrt
#include <iostream>
#include <numeric>// std::iota
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

float calculate_std_dev_approximate(AnalogTimeSeries const & series,
                                    float sample_percentage,
                                    size_t min_sample_threshold) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }

    size_t const data_size = data.size();
    size_t const target_sample_size = static_cast<size_t>(data_size * sample_percentage / 100.0f);

    // Fall back to exact calculation if sample would be too small
    if (target_sample_size < min_sample_threshold) {
        return calculate_std_dev(series);
    }

    // Use systematic sampling for better cache performance
    size_t const step_size = data_size / target_sample_size;
    if (step_size == 0) {
        return calculate_std_dev(series);
    }

    // Calculate mean of sampled data
    float sum = 0.0f;
    size_t sample_count = 0;
    for (size_t i = 0; i < data_size; i += step_size) {
        sum += data[i];
        ++sample_count;
    }
    float const mean = sum / static_cast<float>(sample_count);

    // Calculate variance of sampled data
    float variance_sum = 0.0f;
    for (size_t i = 0; i < data_size; i += step_size) {
        float const diff = data[i] - mean;
        variance_sum += diff * diff;
    }

    return std::sqrt(variance_sum / static_cast<float>(sample_count));
}

float calculate_std_dev_adaptive(AnalogTimeSeries const & series,
                                 size_t initial_sample_size,
                                 size_t max_sample_size,
                                 float convergence_tolerance) {
    auto const & data = series.getAnalogTimeSeries();
    if (data.empty()) {
        return 0.0f;
    }

    size_t const data_size = data.size();
    if (data_size <= max_sample_size) {
        return calculate_std_dev(series);
    }

    size_t current_sample_size = std::min(initial_sample_size, data_size);
    float previous_std_dev = 0.0f;
    bool first_iteration = true;

    while (current_sample_size <= max_sample_size) {
        // Use systematic sampling
        size_t const step_size = data_size / current_sample_size;
        if (step_size == 0) break;

        // Calculate mean of current sample
        float sum = 0.0f;
        size_t actual_sample_count = 0;
        for (size_t i = 0; i < data_size; i += step_size) {
            sum += data[i];
            ++actual_sample_count;
        }
        float const mean = sum / static_cast<float>(actual_sample_count);

        // Calculate standard deviation of current sample
        float variance_sum = 0.0f;
        for (size_t i = 0; i < data_size; i += step_size) {
            float const diff = data[i] - mean;
            variance_sum += diff * diff;
        }
        float const current_std_dev = std::sqrt(variance_sum / static_cast<float>(actual_sample_count));

        // Check for convergence (skip first iteration)
        if (!first_iteration) {
            float const relative_change = std::abs(current_std_dev - previous_std_dev) /
                                          std::max(current_std_dev, previous_std_dev);
            if (relative_change < convergence_tolerance) {
                return current_std_dev;
            }
        }

        previous_std_dev = current_std_dev;
        first_iteration = false;

        // Increase sample size for next iteration (double it)
        current_sample_size = std::min(current_sample_size * 2, max_sample_size);
    }

    return previous_std_dev;
}
