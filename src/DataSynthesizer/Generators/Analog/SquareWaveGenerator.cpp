/**
 * @file SquareWaveGenerator.cpp
 * @brief Analog time series generator that produces a square wave.
 *
 * Registers a "SquareWave" generator in the DataSynthesizer registry.
 * Produces: y[t] = +amplitude if fractional position within cycle < duty_cycle,
 *                  -amplitude otherwise, plus dc_offset.
 * Fractional position = fmod(frequency * t + phase/(2π), 1.0).
 * frequency is in cycles per sample; phase is in radians.
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

struct SquareWaveParams {
    int num_samples;
    float amplitude;
    float frequency;
    std::optional<float> phase;
    std::optional<float> dc_offset;
    std::optional<float> duty_cycle;
};

DataTypeVariant generateSquareWave(SquareWaveParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("SquareWave: num_samples must be > 0");
    }
    if (params.frequency <= 0.0f) {
        throw std::invalid_argument("SquareWave: frequency must be > 0");
    }
    float const dc_ratio = params.duty_cycle.value_or(0.5f);
    if (dc_ratio < 0.0f || dc_ratio > 1.0f) {
        throw std::invalid_argument("SquareWave: duty_cycle must be in [0, 1]");
    }

    float const ph = params.phase.value_or(0.0f);
    float const dc = params.dc_offset.value_or(0.0f);
    // Convert radian phase to cycle fraction for consistent phase semantics
    float const phase_frac = ph / (2.0f * std::numbers::pi_v<float>);

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        auto const t = static_cast<float>(i);
        float pos = std::fmod(params.frequency * t + phase_frac, 1.0f);
        if (pos < 0.0f) {
            pos += 1.0f;
        }
        data[i] = (pos < dc_ratio ? params.amplitude : -params.amplitude) + dc;
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const square_wave_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<SquareWaveParams>(
                "SquareWave",
                generateSquareWave,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a square wave analog time series with configurable duty cycle. "
                                       "duty_cycle in [0,1] controls the fraction of each period at +amplitude.",
                        .category = "Periodic",
                        .output_type = "AnalogTimeSeries"});

}// namespace
