#ifndef WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP
#define WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>

class AnalogTimeSeries;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for analog Hilbert phase/amplitude extraction
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
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> discontinuity_threshold;

    // Maximum samples per chunk (0 = no limit, process entire signal)
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> max_chunk_size;

    // Fraction of overlap between chunks (0.0 to 0.5)
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> overlap_fraction;

    // Apply Hann window to reduce edge artifacts
    std::optional<bool> use_windowing;

    // Whether to apply bandpass filtering before Hilbert transform
    std::optional<bool> apply_bandpass_filter;

    // Bandpass filter low cutoff (Hz)
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> filter_low_freq;

    // Bandpass filter high cutoff (Hz)
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> filter_high_freq;

    // Butterworth filter order (1-8)
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> filter_order;

    // Sampling rate in Hz (0 = auto-detect from data)
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> sampling_rate;

    // Helper methods to get values with defaults
    std::string getOutputType() const {
        return output_type.value_or("phase");
    }

    size_t getDiscontinuityThreshold() const {
        return discontinuity_threshold.has_value() 
            ? static_cast<size_t>(discontinuity_threshold.value().value()) 
            : 1000;
    }

    size_t getMaxChunkSize() const {
        return max_chunk_size.has_value() 
            ? static_cast<size_t>(max_chunk_size.value().value()) 
            : 100000;
    }

    double getOverlapFraction() const {
        return overlap_fraction.has_value() 
            ? static_cast<double>(overlap_fraction.value().value()) 
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
            ? static_cast<double>(filter_low_freq.value().value()) 
            : 5.0;
    }

    double getFilterHighFreq() const {
        return filter_high_freq.has_value() 
            ? static_cast<double>(filter_high_freq.value().value()) 
            : 15.0;
    }

    int getFilterOrder() const {
        return filter_order.has_value() 
            ? filter_order.value().value() 
            : 4;
    }

    double getSamplingRate() const {
        return sampling_rate.has_value() 
            ? static_cast<double>(sampling_rate.value().value()) 
            : 1000.0;
    }

    // Validate output type string
    bool isValidOutputType() const {
        auto type = getOutputType();
        return type == "phase" || type == "amplitude";
    }
};

/**
 * @brief Calculate instantaneous phase or amplitude using Hilbert transform
 * 
 * This is a container-level transform because it needs to run the FFT on 
 * the entire time series (or large chunks of it). It has temporal dependencies
 * that span the entire signal, unlike element-level transforms.
 * 
 * Container signature: AnalogTimeSeries → AnalogTimeSeries
 * 
 * Algorithm:
 * 1. Detect discontinuities in the time series (large gaps)
 * 2. Split signal into continuous chunks
 * 3. For each chunk:
 *    a. Optionally apply bandpass filtering
 *    b. Optionally split into overlapping sub-chunks for long signals
 *    c. Apply FFT-based Hilbert transform to get analytic signal
 *    d. Extract phase or amplitude from analytic signal
 *    e. Merge overlapping results
 * 4. Combine all chunks into output
 * 5. Report progress and check for cancellation
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

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP
