#ifndef WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP
#define WHISKERTOOLBOX_V2_ANALOG_HILBERT_PHASE_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cstddef>
#include <memory>

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
    enum class OutputType {
        phase,
        amplitude
    };

    OutputType output_type = OutputType::phase;

    /// Gap size (in samples) above which to split processing into chunks
    rfl::Validator<size_t, rfl::Minimum<1>> discontinuity_threshold = 1000;

    /// Maximum samples per chunk (0 = no limit, process entire signal)
    size_t max_chunk_size = 100000;

    /// Fraction of overlap between chunks (0.0 to 0.5)
    rfl::Validator<double, rfl::Minimum<0.0>> overlap_fraction = 0.25;

    /// Apply Hann window to reduce edge artifacts
    bool use_windowing = true;

    /// Whether to apply bandpass filtering before Hilbert transform
    bool apply_bandpass_filter = false;

    /// Bandpass filter low cutoff (Hz)
    rfl::Validator<double, rfl::Minimum<0.0>> filter_low_freq = 5.0;

    /// Bandpass filter high cutoff (Hz)
    rfl::Validator<double, rfl::Minimum<0.0>> filter_high_freq = 15.0;

    /// Butterworth filter order (1-8)
    rfl::Validator<int, rfl::Minimum<1>> filter_order = 4;

    /// Sampling rate in Hz (0 = auto-detect from data)
    rfl::Validator<double, rfl::Minimum<0.0>> sampling_rate = 1000.0;
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
