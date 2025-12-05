#ifndef WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP
#define WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

class AnalogTimeSeries;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for Hilbert phase/amplitude extraction
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * 
 * Example JSON:
 * ```json
 * {
 *   "output_type": "phase",
 *   "discontinuity_threshold": 1000,
 *   "max_chunk_size": 100000,
 *   "overlap_fraction": 0.25,
 *   "use_windowing": true,
 *   "apply_bandpass_filter": false,
 *   "filter_low_freq": 5.0,
 *   "filter_high_freq": 15.0,
 *   "filter_order": 4,
 *   "sampling_rate": 1000.0
 * }
 * ```
 */
struct AnalogHilbertPhaseParams {
    // Output type: "phase" or "amplitude"
    std::optional<std::string> output_type;

    // Gap size (in samples) above which to split processing into chunks
    std::optional<rfl::Validator<size_t, rfl::Minimum<1>>> discontinuity_threshold;

    // Maximum samples per chunk (0 = no limit, process entire signal)
    std::optional<size_t> max_chunk_size;

    // Fraction of overlap between chunks (0.0 to 0.5)
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> overlap_fraction;

    // Apply Hann window to reduce edge artifacts
    std::optional<bool> use_windowing;

    // Whether to apply bandpass filtering before Hilbert transform
    std::optional<bool> apply_bandpass_filter;

    // Bandpass filter low cutoff (Hz)
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> filter_low_freq;

    // Bandpass filter high cutoff (Hz)
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> filter_high_freq;

    // Butterworth filter order (1-8)
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> filter_order;

    // Sampling rate in Hz (0 = auto-detect from data)
    std::optional<rfl::Validator<double, rfl::Minimum<0.0>>> sampling_rate;

    // Helper methods to get values with defaults
    std::string getOutputType() const {
        return output_type.value_or("phase");
    }

    bool isPhaseOutput() const {
        auto type = getOutputType();
        return type == "phase" || type == "Phase";
    }

    bool isAmplitudeOutput() const {
        auto type = getOutputType();
        return type == "amplitude" || type == "Amplitude";
    }

    size_t getDiscontinuityThreshold() const {
        return discontinuity_threshold.has_value() 
            ? discontinuity_threshold.value().value() 
            : 1000;
    }

    size_t getMaxChunkSize() const {
        return max_chunk_size.value_or(100000);
    }

    double getOverlapFraction() const {
        return overlap_fraction.has_value() 
            ? overlap_fraction.value().value() 
            : 0.25;
    }

    bool getUseWindowing() const {
        return use_windowing.value_or(true);
    }

    bool getApplyBandpassFilter() const {
        return apply_bandpass_filter.value_or(false);
    }

    double getFilterLowFreq() const {
        return filter_low_freq.has_value() 
            ? filter_low_freq.value().value() 
            : 5.0;
    }

    double getFilterHighFreq() const {
        return filter_high_freq.has_value() 
            ? filter_high_freq.value().value() 
            : 15.0;
    }

    int getFilterOrder() const {
        return filter_order.has_value() 
            ? filter_order.value().value() 
            : 4;
    }

    double getSamplingRate() const {
        return sampling_rate.has_value() 
            ? sampling_rate.value().value() 
            : 1000.0;
    }
};

/**
 * @brief Calculate instantaneous phase or amplitude using Hilbert transform
 * 
 * This is a container-level transform because it operates on the entire time series
 * and handles discontinuities, chunked processing, and FFT-based computation that
 * requires global context.
 * 
 * Container signature: AnalogTimeSeries → AnalogTimeSeries
 * 
 * Algorithm:
 * 1. Detect discontinuities and split into chunks
 * 2. For each chunk:
 *    a. Optionally apply bandpass filter
 *    b. Compute FFT
 *    c. Create analytic signal by zeroing negative frequencies
 *    d. Compute inverse FFT
 *    e. Extract phase (atan2) or amplitude (magnitude)
 * 3. Stitch chunks back together
 * 4. Report progress and check for cancellation
 * 
 * @param input Input analog time series
 * @param params Hilbert transform parameters
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to analog time series containing phase (radians) or amplitude
 */
std::shared_ptr<AnalogTimeSeries> analogHilbertPhase(
        AnalogTimeSeries const & input,
        AnalogHilbertPhaseParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP
