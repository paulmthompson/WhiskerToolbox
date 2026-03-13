/**
 * @file TriangleWaveGenerator.cpp
 * @brief Analog time series generator that produces a triangle wave.
 *
 * Registers a "TriangleWave" generator in the DataSynthesizer registry.
 * Produces: y[t] = amplitude * (2/π) * arcsin(sin(2π * (num_cycles/cycle_length) * t + phase)) + dc_offset
 * phase is in radians.
 * This formulation maintains the same phase semantics as SineWave:
 * at t=0 with phase=0, the output is 0 and rising.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

struct TriangleWaveParams {
    int num_samples = 1000;
    float amplitude = 1.0f;
    float num_cycles = 1.0f;
    std::optional<int> cycle_length;
    std::optional<float> phase;
    std::optional<float> dc_offset;
};

DataTypeVariant generateTriangleWave(TriangleWaveParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("TriangleWave: num_samples must be > 0");
    }
    if (params.num_cycles <= 0.0f) {
        throw std::invalid_argument("TriangleWave: num_cycles must be > 0");
    }
    int const cyc_len = params.cycle_length.value_or(params.num_samples);
    if (cyc_len <= 0) {
        throw std::invalid_argument("TriangleWave: cycle_length must be > 0");
    }

    float const ph = params.phase.value_or(0.0f);
    float const dc = params.dc_offset.value_or(0.0f);
    float const frequency = params.num_cycles / static_cast<float>(cyc_len);
    float const two_pi_freq = 2.0f * std::numbers::pi_v<float> * frequency;
    // Scale factor: (2/π) converts arcsin range [-π/2, π/2] to [-1, 1]
    float const inv_half_pi = 2.0f / std::numbers::pi_v<float>;

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        auto const t = static_cast<float>(i);
        data[i] = params.amplitude * inv_half_pi * std::asin(std::sin(two_pi_freq * t + ph)) + dc;
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const triangle_wave_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<TriangleWaveParams>(
                "TriangleWave",
                generateTriangleWave,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a triangle wave analog time series. "
                                       "num_cycles complete cycles over cycle_length samples (defaults to num_samples). "
                                       "Uses arcsin(sin(x)) formulation for consistent phase with SineWave.",
                        .category = "Periodic",
                        .output_type = "AnalogTimeSeries"});

}// namespace
