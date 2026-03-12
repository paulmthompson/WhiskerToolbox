/**
 * @file TriangleWaveGenerator.cpp
 * @brief Analog time series generator that produces a triangle wave.
 *
 * Registers a "TriangleWave" generator in the DataSynthesizer registry.
 * Produces: y[t] = amplitude * (2/π) * arcsin(sin(2π * frequency * t + phase)) + dc_offset
 * frequency is in cycles per sample; phase is in radians.
 * This formulation maintains the same phase semantics as SineWave:
 * at t=0 with phase=0, the output is 0 and rising.
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

struct TriangleWaveParams {
    int num_samples;
    float amplitude;
    float frequency;
    std::optional<float> phase;
    std::optional<float> dc_offset;
};

DataTypeVariant generateTriangleWave(TriangleWaveParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("TriangleWave: num_samples must be > 0");
    }
    if (params.frequency <= 0.0f) {
        throw std::invalid_argument("TriangleWave: frequency must be > 0");
    }

    float const ph = params.phase.value_or(0.0f);
    float const dc = params.dc_offset.value_or(0.0f);
    float const two_pi_freq = 2.0f * std::numbers::pi_v<float> * params.frequency;
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
                                       "Uses arcsin(sin(x)) formulation for consistent phase with SineWave.",
                        .category = "Periodic",
                        .output_type = "AnalogTimeSeries"});

}// namespace
