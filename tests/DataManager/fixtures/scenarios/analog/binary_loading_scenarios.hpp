#ifndef ANALOG_BINARY_LOADING_SCENARIOS_HPP
#define ANALOG_BINARY_LOADING_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace analog_scenarios {

/**
 * @brief Helper to write AnalogTimeSeries data to a binary file as int16
 * 
 * Writes the float values as int16_t (truncated/scaled) to match
 * the binary format expected by BinaryAnalogLoaderOptions.
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param header_size Number of header bytes to write (filled with zeros)
 * @return true if write succeeded
 */
inline bool writeBinaryInt16(AnalogTimeSeries const* signal, 
                             std::string const& filepath,
                             size_t header_size = 0) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    // Write data as int16_t
    for (auto const& sample : signal->getAllSamples()) {
        auto value = static_cast<int16_t>(sample.value);
        file.write(reinterpret_cast<char const*>(&value), sizeof(int16_t));
    }
    
    return file.good();
}

/**
 * @brief Helper to write multiple channels of AnalogTimeSeries to interleaved binary
 * 
 * For multi-channel data, values are interleaved: ch0[0], ch1[0], ch0[1], ch1[1], ...
 * 
 * @param signals Vector of AnalogTimeSeries (all must have same size)
 * @param filepath Output file path
 * @param header_size Number of header bytes to write
 * @return true if write succeeded
 */
inline bool writeBinaryInt16MultiChannel(
        std::vector<std::shared_ptr<AnalogTimeSeries>> const& signals,
        std::string const& filepath,
        size_t header_size = 0) {
    
    if (signals.empty()) {
        return false;
    }
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    // Verify all signals have same size
    size_t num_samples = signals[0]->getNumSamples();
    for (auto const& sig : signals) {
        if (sig->getNumSamples() != num_samples) {
            return false;
        }
    }
    
    // Write interleaved data
    for (size_t i = 0; i < num_samples; ++i) {
        for (auto const& sig : signals) {
            auto samples = sig->getAllSamples();
            auto value = static_cast<int16_t>(samples[i].value);
            file.write(reinterpret_cast<char const*>(&value), sizeof(int16_t));
        }
    }
    
    return file.good();
}

/**
 * @brief Helper to write AnalogTimeSeries data as float32 binary
 * 
 * @param signal The AnalogTimeSeries to write
 * @param filepath Output file path
 * @param header_size Number of header bytes to write
 * @return true if write succeeded
 */
inline bool writeBinaryFloat32(AnalogTimeSeries const* signal,
                               std::string const& filepath,
                               size_t header_size = 0) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    // Write data as float
    for (auto const& sample : signal->getAllSamples()) {
        float value = sample.value;
        file.write(reinterpret_cast<char const*>(&value), sizeof(float));
    }
    
    return file.good();
}

//=============================================================================
// Pre-configured test signals for binary loading tests
//=============================================================================

/**
 * @brief Simple ramp signal for basic loading tests
 * 
 * Values: 0, 1, 2, ..., 99 (100 samples)
 * Good for verifying order is preserved
 */
inline std::shared_ptr<AnalogTimeSeries> simple_ramp_100() {
    return AnalogTimeSeriesBuilder()
        .withRamp(0, 99, 0.0f, 99.0f)
        .build();
}

/**
 * @brief Sine wave signal for testing floating point precision
 * 
 * 1000 samples of a 10Hz sine wave with amplitude 1000
 * Large amplitude to survive int16 round-trip
 */
inline std::shared_ptr<AnalogTimeSeries> sine_wave_1000_samples() {
    return AnalogTimeSeriesBuilder()
        .withSineWave(0, 999, 0.01f, 1000.0f)
        .build();
}

/**
 * @brief Square wave for testing distinct value transitions
 * 
 * Alternates between 0 and 100 every 10 samples
 */
inline std::shared_ptr<AnalogTimeSeries> square_wave_500_samples() {
    return AnalogTimeSeriesBuilder()
        .withSquareWave(0, 499, 10, 100.0f, 0.0f)
        .build();
}

/**
 * @brief Constant value signal for baseline tests
 */
inline std::shared_ptr<AnalogTimeSeries> constant_value_100() {
    return AnalogTimeSeriesBuilder()
        .withConstant(42.0f, 0, 99)
        .build();
}

/**
 * @brief Multi-channel test signals (2 channels)
 * 
 * Channel 0: Ramp 0-99
 * Channel 1: Ramp 99-0 (inverted)
 */
inline std::vector<std::shared_ptr<AnalogTimeSeries>> two_channel_ramps() {
    return {
        AnalogTimeSeriesBuilder()
            .withRamp(0, 99, 0.0f, 99.0f)
            .build(),
        AnalogTimeSeriesBuilder()
            .withRamp(0, 99, 99.0f, 0.0f)
            .build()
    };
}

/**
 * @brief Multi-channel test signals (4 channels)
 * 
 * Channel 0: Constant 10
 * Channel 1: Constant 20
 * Channel 2: Constant 30
 * Channel 3: Constant 40
 */
inline std::vector<std::shared_ptr<AnalogTimeSeries>> four_channel_constants() {
    return {
        AnalogTimeSeriesBuilder().withConstant(10.0f, 0, 49).build(),
        AnalogTimeSeriesBuilder().withConstant(20.0f, 0, 49).build(),
        AnalogTimeSeriesBuilder().withConstant(30.0f, 0, 49).build(),
        AnalogTimeSeriesBuilder().withConstant(40.0f, 0, 49).build()
    };
}

} // namespace analog_scenarios

#endif // ANALOG_BINARY_LOADING_SCENARIOS_HPP
