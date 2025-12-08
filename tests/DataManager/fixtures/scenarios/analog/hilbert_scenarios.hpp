#ifndef HILBERT_SCENARIOS_HPP
#define HILBERT_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include <memory>
#include <cmath>
#include <numbers>
#include <limits>

/**
 * @brief Hilbert transform test scenarios for AnalogTimeSeries
 * 
 * This namespace contains pre-configured test data for Hilbert transform
 * algorithms (phase and amplitude extraction). These scenarios are extracted 
 * from existing test fixtures to enable reuse across v1 and v2 transform tests.
 */
namespace hilbert_scenarios {

// ============================================================================
// Basic Waveform Scenarios
// ============================================================================

/**
 * @brief Simple sine wave - 1 Hz, 100 Hz sampling, 200 samples
 * 
 * Use for: Basic phase relationship testing
 * Expected: Phase values in range [-π, π], monotonically increasing (with wraps)
 */
inline std::shared_ptr<AnalogTimeSeries> sine_1hz_200() {
    return AnalogTimeSeriesBuilder()
        .withSineWave(0, 199, 0.01f, 1.0f)  // 1 Hz at 100 Hz sampling = 0.01 cycles/sample
        .build();
}

/**
 * @brief Cosine wave - 2 Hz, 50 Hz sampling, 100 samples
 * 
 * Use for: Testing phase shift from sine (should be π/2 shifted)
 */
inline std::shared_ptr<AnalogTimeSeries> cosine_2hz_100() {
    return AnalogTimeSeriesBuilder()
        .withCosineWave(0, 99, 0.04f, 1.0f)  // 2 Hz at 50 Hz sampling = 0.04 cycles/sample
        .build();
}

/**
 * @brief Complex signal with multiple frequencies - 2Hz and 5Hz components
 * 
 * Data: sin(2πf1*t) + 0.5*sin(2πf2*t) where f1=2Hz, f2=5Hz
 * Use for: Testing phase continuity with complex signals
 */
inline std::shared_ptr<AnalogTimeSeries> multi_freq_2_5() {
    return AnalogTimeSeriesBuilder()
        .withFunction(0, 299, [](int t) {
            float time = static_cast<float>(t) / 100.0f;  // 100 Hz sampling
            return std::sin(2.0f * std::numbers::pi_v<float> * 2.0f * time) +
                   0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 5.0f * time);
        })
        .build();
}

// ============================================================================
// Discontinuous Signal Scenarios
// ============================================================================

/**
 * @brief Discontinuous time series with large gap
 * 
 * Data: {1.0, 0.0, -1.0, 0.0, 1.0, 0.0, -1.0, 0.0}
 * Times: {0, 1, 2, 3, 2000, 2001, 2002, 2003}
 * 
 * Use for: Testing chunked processing with discontinuity_threshold=100
 * Gap of 1997 samples should trigger chunk split
 */
inline std::shared_ptr<AnalogTimeSeries> discontinuous_large_gap() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f})
        .atTimes({0, 1, 2, 3, 2000, 2001, 2002, 2003})
        .build();
}

/**
 * @brief Multiple discontinuities
 * 
 * Data: {1.0, 0.0, -1.0, 1.0, 0.0, -1.0}
 * Times: {0, 1, 2, 1000, 1001, 2000}
 * 
 * Use for: Testing with discontinuity_threshold=100 (creates 3 chunks)
 */
inline std::shared_ptr<AnalogTimeSeries> multiple_discontinuities() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f})
        .atTimes({0, 1, 2, 1000, 1001, 2000})
        .build();
}

/**
 * @brief Signal for progress callback testing
 * 
 * Data: {1.0, 0.0, -1.0, 0.0, 1.0}
 * Times: {0, 25, 50, 75, 100}
 */
inline std::shared_ptr<AnalogTimeSeries> progress_callback_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f, 1.0f})
        .atTimes({0, 25, 50, 75, 100})
        .build();
}

/**
 * @brief Default parameters test signal
 * 
 * Data: {1.0, 2.0, 1.0, 0.0, -1.0}
 * Times: {0, 10, 20, 30, 40}
 */
inline std::shared_ptr<AnalogTimeSeries> default_params_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 1.0f, 0.0f, -1.0f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
}

// ============================================================================
// Amplitude Extraction Scenarios
// ============================================================================

/**
 * @brief Sine wave with known amplitude (2.5) for amplitude extraction testing
 * 
 * Use for: Verifying amplitude extraction returns ~2.5
 */
inline std::shared_ptr<AnalogTimeSeries> amplitude_sine_2_5() {
    return AnalogTimeSeriesBuilder()
        .withSineWave(0, 199, 0.01f, 2.5f)  // 1 Hz at 100 Hz sampling, amplitude 2.5
        .build();
}

/**
 * @brief Amplitude modulated signal
 * 
 * envelope = 1.0 + 0.5*sin(2π*1Hz*t)
 * carrier = sin(2π*10Hz*t)
 * output = envelope * carrier
 * 
 * Use for: Testing amplitude extraction with varying envelope
 */
inline std::shared_ptr<AnalogTimeSeries> amplitude_modulated() {
    return AnalogTimeSeriesBuilder()
        .withFunction(0, 199, [](int t) {
            float time = static_cast<float>(t) / 100.0f;  // 100 Hz sampling
            float envelope = 1.0f + 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 1.0f * time);
            float carrier = std::sin(2.0f * std::numbers::pi_v<float> * 10.0f * time);
            return envelope * carrier;
        })
        .build();
}

/**
 * @brief Amplitude extraction with discontinuities
 * 
 * Data: {1.0, 0.5, -1.0, 0.5, 2.0, 1.0, -2.0, 1.0}
 * Times: {0, 1, 2, 3, 2000, 2001, 2002, 2003}
 */
inline std::shared_ptr<AnalogTimeSeries> amplitude_discontinuous() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.5f, -1.0f, 0.5f, 2.0f, 1.0f, -2.0f, 1.0f})
        .atTimes({0, 1, 2, 3, 2000, 2001, 2002, 2003})
        .build();
}

// ============================================================================
// Long Signal Scenarios (for windowed processing)
// ============================================================================

/**
 * @brief Long sine wave for windowed processing - 150000 samples
 * 
 * 5 Hz sine wave at 1000 Hz sampling, amplitude 2.0
 * Use for: Testing chunked/windowed processing with maxChunkSize
 */
inline std::shared_ptr<AnalogTimeSeries> long_sine_5hz() {
    return AnalogTimeSeriesBuilder()
        .withSineWave(0, 149999, 0.005f, 2.0f)  // 5 Hz at 1000 Hz sampling
        .build();
}

// ============================================================================
// Edge Case Scenarios
// ============================================================================

/**
 * @brief Empty signal
 */
inline std::shared_ptr<AnalogTimeSeries> empty_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({})
        .atTimes({})
        .build();
}

/**
 * @brief Single sample signal
 */
inline std::shared_ptr<AnalogTimeSeries> single_sample() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f})
        .atTimes({0})
        .build();
}

/**
 * @brief Signal with NaN values
 * 
 * Data: {1.0, NaN, -1.0, 0.0}
 * Times: {0, 25, 50, 75}
 */
inline std::shared_ptr<AnalogTimeSeries> signal_with_nan() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, std::numeric_limits<float>::quiet_NaN(), -1.0f, 0.0f})
        .atTimes({0, 25, 50, 75})
        .build();
}

/**
 * @brief Signal for testing invalid frequency parameters
 * 
 * Data: {1.0, 0.0, -1.0, 0.0}
 * Times: {0, 25, 50, 75}
 */
inline std::shared_ptr<AnalogTimeSeries> invalid_freq_params() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f})
        .atTimes({0, 25, 50, 75})
        .build();
}

/**
 * @brief Irregular timestamp spacing
 * 
 * Data: {1.0, 0.0, -1.0, 0.0, 1.0}
 * Times: {0, 1, 10, 11, 100}
 */
inline std::shared_ptr<AnalogTimeSeries> irregular_spacing() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f, 1.0f})
        .atTimes({0, 1, 10, 11, 100})
        .build();
}

/**
 * @brief Small gaps for discontinuity threshold testing
 * 
 * Data: {1.0, 0.0, -1.0, 0.0}
 * Times: {0, 5, 10, 15}
 */
inline std::shared_ptr<AnalogTimeSeries> small_gaps() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f})
        .atTimes({0, 5, 10, 15})
        .build();
}

/**
 * @brief Large gaps for discontinuity threshold testing
 * 
 * Data: {1.0, 0.0, -1.0, 0.0}
 * Times: {0, 100, 200, 300}
 */
inline std::shared_ptr<AnalogTimeSeries> large_gaps() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 0.0f, -1.0f, 0.0f})
        .atTimes({0, 100, 200, 300})
        .build();
}

/**
 * @brief Irregularly sampled signal for interpolation testing
 * 
 * Two segments with gaps:
 * - First segment: points at 0,1,3,4,6,7,9,10 (skipping every 3rd)
 * - Gap of ~100 samples
 * - Second segment: points at 110,111,113,114,116,117,119,120
 */
inline std::shared_ptr<AnalogTimeSeries> irregularly_sampled() {
    std::vector<float> data;
    std::vector<int> times;
    
    constexpr double sampling_rate = 1000.0;
    constexpr double frequency = 10.0;
    
    // First segment: 0-10, skipping every 3rd
    for (int i = 0; i <= 10; i++) {
        if (i % 3 == 2) continue;
        double t = i / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * t)));
        times.push_back(i);
    }
    
    // Second segment: 110-120, skipping every 3rd
    for (int i = 110; i <= 120; i++) {
        if (i % 3 == 2) continue;
        double t = i / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * t)));
        times.push_back(i);
    }
    
    return AnalogTimeSeriesBuilder()
        .withValues(data)
        .atTimes(times)
        .build();
}

/**
 * @brief Pipeline test signal - 10 Hz sine wave, 100 Hz sampling, 200 samples
 * 
 * Use for: JSON pipeline integration tests
 */
inline std::shared_ptr<AnalogTimeSeries> pipeline_test_signal() {
    return AnalogTimeSeriesBuilder()
        .withSineWave(0, 199, 0.1f, 1.0f)  // 10 Hz at 100 Hz sampling
        .build();
}

} // namespace hilbert_scenarios

#endif // HILBERT_SCENARIOS_HPP
