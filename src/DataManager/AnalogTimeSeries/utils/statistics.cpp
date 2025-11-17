#include "statistics.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// ========== Mean ==========

float calculate_mean_impl(std::vector<float> const & data, size_t start, size_t end) {
    if (data.empty() || start >= end || start >= data.size() || end > data.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_mean_impl(data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(end));
}

float calculate_mean(AnalogTimeSeries const & series) {
    auto span = series.getAnalogTimeSeries();
    // If span is empty (non-contiguous storage like memory-mapped with stride),
    // fall back to iterator-based calculation
    if (span.empty()) {
        size_t count = 0;
        double sum = 0.0;
        for (auto const& [time, value] : series.getAllSamples()) {
            sum += static_cast<double>(value);
            ++count;
        }
        return count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_mean(span);
}

float calculate_mean(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto span = series.getAnalogTimeSeries();
    if (span.empty() || start < 0 || end < 0 || start >= end) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    size_t ustart = static_cast<size_t>(start);
    size_t uend = static_cast<size_t>(end);
    if (ustart >= span.size() || uend > span.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_mean(span.subspan(ustart, uend - ustart));
}

float calculate_mean_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time) {
    auto data_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
    return calculate_mean(data_span);
}

// ========== Standard Deviation ==========

float calculate_std_dev_impl(std::vector<float> const & data, size_t start, size_t end) {
    if (data.empty() || start >= end || start >= data.size() || end > data.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_std_dev_impl(data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(end));
}

float calculate_std_dev(AnalogTimeSeries const & series) {
    auto span = series.getAnalogTimeSeries();
    // If span is empty (non-contiguous storage like memory-mapped with stride),
    // fall back to iterator-based calculation
    if (span.empty()) {
        // First pass: calculate mean
        size_t count = 0;
        double sum = 0.0;
        for (auto const& [time, value] : series.getAllSamples()) {
            sum += static_cast<double>(value);
            ++count;
        }
        if (count == 0) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        double mean = sum / static_cast<double>(count);
        
        // Second pass: calculate variance
        double variance_sum = 0.0;
        for (auto const& [time, value] : series.getAllSamples()) {
            double diff = static_cast<double>(value) - mean;
            variance_sum += diff * diff;
        }
        return static_cast<float>(std::sqrt(variance_sum / static_cast<double>(count)));
    }
    return calculate_std_dev(span);
}

float calculate_std_dev(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto span = series.getAnalogTimeSeries();
    if (span.empty() || start < 0 || end < 0 || start >= end) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    size_t ustart = static_cast<size_t>(start);
    size_t uend = static_cast<size_t>(end);
    if (ustart >= span.size() || uend > span.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_std_dev(span.subspan(ustart, uend - ustart));
}

float calculate_std_dev_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time) {
    auto data_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
    return calculate_std_dev(data_span);
}

float calculate_std_dev_approximate(AnalogTimeSeries const & series,
                                    float sample_percentage,
                                    size_t min_sample_threshold) {
    auto span = series.getAnalogTimeSeries();
    
    // If span is empty (non-contiguous storage), fall back to exact calculation via iterators
    if (span.empty()) {
        return calculate_std_dev(series);
    }

    size_t const data_size = span.size();
    auto const target_sample_size = static_cast<size_t>(static_cast<float>(data_size) * sample_percentage / 100.0f);

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
        sum += span[i];
        ++sample_count;
    }
    float const mean = sum / static_cast<float>(sample_count);

    // Calculate variance of sampled data
    float variance_sum = 0.0f;
    for (size_t i = 0; i < data_size; i += step_size) {
        float const diff = span[i] - mean;
        variance_sum += diff * diff;
    }

    return std::sqrt(variance_sum / static_cast<float>(sample_count));
}

float calculate_std_dev_adaptive(AnalogTimeSeries const & series,
                                 size_t initial_sample_size,
                                 size_t max_sample_size,
                                 float convergence_tolerance) {
    auto span = series.getAnalogTimeSeries();
    
    // If span is empty (non-contiguous storage), fall back to exact calculation via iterators
    if (span.empty()) {
        return calculate_std_dev(series);
    }

    size_t const data_size = span.size();
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
            sum += span[i];
            ++actual_sample_count;
        }
        float const mean = sum / static_cast<float>(actual_sample_count);

        // Calculate standard deviation of current sample
        float variance_sum = 0.0f;
        for (size_t i = 0; i < data_size; i += step_size) {
            float const diff = span[i] - mean;
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

float calculate_std_dev_approximate_in_time_range(AnalogTimeSeries const & series,
                                                  TimeFrameIndex start_time,
                                                  TimeFrameIndex end_time,
                                                  float sample_percentage,
                                                  size_t min_sample_threshold) {
    auto data_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
    if (data_span.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    size_t const data_size = data_span.size();
    size_t const target_sample_size = static_cast<size_t>(static_cast<float>(data_size) * sample_percentage / 100.0f);

    // Fall back to exact calculation if sample would be too small
    if (target_sample_size < min_sample_threshold) {
        return calculate_std_dev(data_span);
    }

    // Use systematic sampling for better cache performance
    size_t const step_size = data_size / target_sample_size;
    if (step_size == 0) {
        return calculate_std_dev(data_span);
    }

    // Calculate mean of sampled data
    float sum = 0.0f;
    size_t sample_count = 0;
    for (size_t i = 0; i < data_size; i += step_size) {
        sum += data_span[i];
        ++sample_count;
    }
    float const mean = sum / static_cast<float>(sample_count);

    // Calculate variance of sampled data
    float variance_sum = 0.0f;
    for (size_t i = 0; i < data_size; i += step_size) {
        float const diff = data_span[i] - mean;
        variance_sum += diff * diff;
    }

    return std::sqrt(variance_sum / static_cast<float>(sample_count));
}

// ========== Minimum ==========

float calculate_min_impl(std::vector<float> const & data, size_t start, size_t end) {
    if (data.empty() || start >= end || start >= data.size() || end > data.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_min_impl(data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(end));
}

float calculate_min(AnalogTimeSeries const & series) {
    auto span = series.getAnalogTimeSeries();
    // If span is empty (non-contiguous storage like memory-mapped with stride),
    // fall back to iterator-based calculation
    if (span.empty()) {
        float min_val = std::numeric_limits<float>::max();
        bool found_any = false;
        for (auto const& [time, value] : series.getAllSamples()) {
            min_val = std::min(min_val, value);
            found_any = true;
        }
        return found_any ? min_val : std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_min(span);
}

float calculate_min(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto span = series.getAnalogTimeSeries();
    if (span.empty() || start < 0 || end < 0 || start >= end) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    size_t ustart = static_cast<size_t>(start);
    size_t uend = static_cast<size_t>(end);
    if (ustart >= span.size() || uend > span.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_min(span.subspan(ustart, uend - ustart));
}

float calculate_min_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time) {
    auto data_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
    return calculate_min(data_span);
}

// ========== Maximum ==========

float calculate_max_impl(std::vector<float> const & data, size_t start, size_t end) {
    if (data.empty() || start >= end || start >= data.size() || end > data.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_max_impl(data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(end));
}

float calculate_max(AnalogTimeSeries const & series) {
    auto span = series.getAnalogTimeSeries();
    // If span is empty (non-contiguous storage like memory-mapped with stride),
    // fall back to iterator-based calculation
    if (span.empty()) {
        float max_val = std::numeric_limits<float>::lowest();
        bool found_any = false;
        for (auto const& [time, value] : series.getAllSamples()) {
            max_val = std::max(max_val, value);
            found_any = true;
        }
        return found_any ? max_val : std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_max(span);
}

float calculate_max(AnalogTimeSeries const & series, int64_t start, int64_t end) {
    auto span = series.getAnalogTimeSeries();
    if (span.empty() || start < 0 || end < 0 || start >= end) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    size_t ustart = static_cast<size_t>(start);
    size_t uend = static_cast<size_t>(end);
    if (ustart >= span.size() || uend > span.size()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return calculate_max(span.subspan(ustart, uend - ustart));
}

float calculate_max_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time) {
    auto data_span = series.getDataInTimeFrameIndexRange(start_time, end_time);
    return calculate_max(data_span);
}
