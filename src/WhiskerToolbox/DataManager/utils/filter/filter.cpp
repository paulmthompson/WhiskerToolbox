#include "filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "new_filter_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>

// ========== FilterOptions Validation ==========

bool FilterOptions::isValid() const {
    return getValidationError().empty();
}

std::string FilterOptions::getValidationError() const {
    if (order < 1 || order > max_filter_order) {
        return "Filter order must be between 1 and " + std::to_string(max_filter_order);
    }

    if (sampling_rate_hz <= 0.0) {
        return "Sampling rate must be positive";
    }

    if (cutoff_frequency_hz <= 0.0) {
        return "Cutoff frequency must be positive";
    }

    double nyquist = sampling_rate_hz / 2.0;
    if (cutoff_frequency_hz >= nyquist) {
        return "Cutoff frequency must be less than Nyquist frequency (" +
               std::to_string(nyquist) + " Hz)";
    }

    // Additional validation for band filters
    if (response == FilterResponse::BandPass || response == FilterResponse::BandStop) {
        // For RBJ filters, we use cutoff_frequency_hz as center frequency and q_factor
        if (type == FilterType::RBJ) {
            if (cutoff_frequency_hz >= nyquist) {
                return "Center frequency must be less than Nyquist frequency";
            }
            if (q_factor <= 0.0) {
                return "Q factor must be positive for RBJ band filters";
            }
        } else {
            // For IIR filters, we need both cutoff frequencies
            if (high_cutoff_hz <= cutoff_frequency_hz) {
                return "High cutoff frequency must be greater than low cutoff frequency";
            }
            if (high_cutoff_hz >= nyquist) {
                return "High cutoff frequency must be less than Nyquist frequency";
            }
        }
    }

    // Validation for filter-specific parameters
    if (type == FilterType::ChebyshevI && passband_ripple_db <= 0.0) {
        return "Chebyshev I passband ripple must be positive";
    }

    if (type == FilterType::ChebyshevII && stopband_ripple_db <= 0.0) {
        return "Chebyshev II stopband ripple must be positive";
    }

    if (type == FilterType::RBJ && q_factor <= 0.0) {
        return "RBJ Q factor must be positive";
    }

    return "";// Valid
}

// ========== Main Filtering Functions ==========

FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time,
        FilterOptions const & options) {
    return filterAnalogTimeSeriesNew(analog_time_series, start_time, end_time, options);
}

FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        FilterOptions const & options) {
    return filterAnalogTimeSeriesNew(analog_time_series, options);
}

// ========== Filter Defaults ==========

namespace FilterDefaults {

FilterOptions lowpass(double cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::LowPass;
    options.cutoff_frequency_hz = cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions highpass(double cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::HighPass;
    options.cutoff_frequency_hz = cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions bandpass(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::BandPass;
    options.cutoff_frequency_hz = low_cutoff_hz;
    options.high_cutoff_hz = high_cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions notch(double center_freq_hz, double sampling_rate_hz, double q_factor) {
    FilterOptions options;
    options.type = FilterType::RBJ;
    options.response = FilterResponse::BandStop;
    options.cutoff_frequency_hz = center_freq_hz;
    options.high_cutoff_hz = center_freq_hz;  // Set equal to center frequency to force Q-factor path
    options.sampling_rate_hz = sampling_rate_hz;
    options.q_factor = q_factor;
    options.order = 2;  // RBJ filters are always 2nd order
    return options;
}

}// namespace FilterDefaults