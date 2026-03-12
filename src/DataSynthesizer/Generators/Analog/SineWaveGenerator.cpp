/**
 * @file SineWaveGenerator.cpp
 * @brief Analog time series generator that produces a sine wave.
 *
 * Registers a "SineWave" generator in the DataSynthesizer registry.
 * Produces: y[t] = amplitude * sin(2π * (num_cycles / cycle_length) * t + phase) + dc_offset
 * where t is the sample index.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

struct SineWaveParams {
    int num_samples = 1000;
    float amplitude = 1.0f;
    float num_cycles = 1.0f;
    std::optional<int> cycle_length;
    std::optional<float> phase;
    std::optional<float> dc_offset;
};

DataTypeVariant generateSineWave(SineWaveParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("SineWave: num_samples must be > 0");
    }
    if (params.num_cycles <= 0.0f) {
        throw std::invalid_argument("SineWave: num_cycles must be > 0");
    }
    int const cyc_len = params.cycle_length.value_or(params.num_samples);
    if (cyc_len <= 0) {
        throw std::invalid_argument("SineWave: cycle_length must be > 0");
    }

    float const ph = params.phase.value_or(0.0f);
    float const dc = params.dc_offset.value_or(0.0f);
    float const frequency = params.num_cycles / static_cast<float>(cyc_len);
    float const two_pi_freq = 2.0f * std::numbers::pi_v<float> * frequency;

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        auto const t = static_cast<float>(i);
        data[i] = params.amplitude * std::sin(two_pi_freq * t + ph) + dc;
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const sine_wave_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<SineWaveParams>(
                "SineWave",
                generateSineWave,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a sine wave analog time series. "
                                       "Produces y[t] = amplitude * sin(2π * frequency * t + phase) + dc_offset.",
                        .category = "Periodic",
                        .output_type = "AnalogTimeSeries"});

}// namespace
