#ifndef FILTER_HPP
#define FILTER_HPP

#include "Iir.h"
#include "TimeFrame.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class AnalogTimeSeries;

// Maximum filter order supported at compile time
int const max_filter_order = 8;

/**
 * @brief Types of filters supported
 */
enum class FilterType {
    Butterworth,// Maximally flat passband
    ChebyshevI, // Equiripple in passband, monotonic in stopband
    ChebyshevII,// Monotonic in passband, equiripple in stopband
    RBJ         // Robert Bristow-Johnson biquad filters
};

/**
 * @brief Filter response types
 */
enum class FilterResponse {
    LowPass, // Low-pass filter
    HighPass,// High-pass filter
    BandPass,// Band-pass filter
    BandStop,// Band-stop/notch filter
    LowShelf,// Low-shelf filter (Butterworth/Chebyshev only)
    HighShelf// High-shelf filter (Butterworth/Chebyshev only)
};

/**
 * @brief Interpolation methods for handling irregular sampling
 */
enum class InterpolationMethod {
    None,        // No interpolation, process as-is
    Linear,      // Linear interpolation between samples
    ZeroOrderHold// Hold previous value (step interpolation)
};

/**
 * @brief Comprehensive filter options structure
 */
struct FilterOptions {
    // Basic filter parameters
    FilterType type = FilterType::Butterworth;
    FilterResponse response = FilterResponse::LowPass;
    int order = 4;// Filter order (1 to max_filter_order)

    // Frequency parameters
    double sampling_rate_hz = 1000.0;  // Sampling rate in Hz
    double cutoff_frequency_hz = 100.0;// Primary cutoff frequency
    double high_cutoff_hz = 200.0;     // Secondary cutoff for bandpass/bandstop

    // Filter-specific parameters
    double passband_ripple_db = 1.0; // For Chebyshev I (dB)
    double stopband_ripple_db = 20.0;// For Chebyshev II (dB)
    double q_factor = 1.0;           // For RBJ filters
    double shelf_gain_db = 6.0;      // For shelf filters (dB)

    // Processing options
    bool zero_phase = false;// Use filtfilt (forward-backward filtering)

    // Irregular sampling handling
    InterpolationMethod interpolation = InterpolationMethod::None;
    size_t max_gap_samples = 1000;// Max gap before starting new filter segment

    // Validation
    bool isValid() const;
    std::string getValidationError() const;
};

/**
 * @brief Results of filtering operation
 */
struct FilterResult {
    std::shared_ptr<AnalogTimeSeries> filtered_data;
    bool success = false;
    std::string error_message;
    size_t samples_processed = 0;
    size_t segments_processed = 0;// For irregular data with gaps
};

/**
 * @brief Filter AnalogTimeSeries data within a specific time range
 * 
 * This function applies digital filtering to an AnalogTimeSeries within the specified
 * time range. It handles both regular and irregular sampling, with options for
 * interpolation and zero-phase filtering.
 * 
 * @param analog_time_series Input AnalogTimeSeries to filter
 * @param start_time Start of time range to filter (inclusive)
 * @param end_time End of time range to filter (inclusive)
 * @param options Filter configuration options
 * @return FilterResult containing the filtered data and processing information
 */
FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time,
        FilterOptions const & options);

/**
 * @brief Filter entire AnalogTimeSeries
 * 
 * Convenience function that filters the entire time series.
 * 
 * @param analog_time_series Input AnalogTimeSeries to filter
 * @param options Filter configuration options
 * @return FilterResult containing the filtered data and processing information
 */
FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        FilterOptions const & options);

/**
 * @brief Get recommended sampling rate for AnalogTimeSeries
 * 
 * Analyzes the time spacing in an AnalogTimeSeries to estimate the sampling rate.
 * Useful for automatically determining appropriate filter parameters.
 * 
 * @param analog_time_series Input AnalogTimeSeries to analyze
 * @param start_time Start of time range to analyze (optional)
 * @param end_time End of time range to analyze (optional)
 * @return Estimated sampling rate in Hz, or 0.0 if estimation fails
 */
double estimateSamplingRate(
        AnalogTimeSeries const * analog_time_series,
        std::optional<TimeFrameIndex> start_time = std::nullopt,
        std::optional<TimeFrameIndex> end_time = std::nullopt);

/**
 * @brief Create default filter options for common use cases
 */
namespace FilterDefaults {
FilterOptions lowpass(double cutoff_hz, double sampling_rate_hz, int order = 4);
FilterOptions highpass(double cutoff_hz, double sampling_rate_hz, int order = 4);
FilterOptions bandpass(double low_hz, double high_hz, double sampling_rate_hz, int order = 4);
FilterOptions notch(double center_hz, double sampling_rate_hz, double q_factor = 10.0);
}// namespace FilterDefaults

#endif// FILTER_HPP