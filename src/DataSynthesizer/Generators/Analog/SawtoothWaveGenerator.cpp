/**
 * @file SawtoothWaveGenerator.cpp
 * @brief Analog time series generator that produces a sawtooth (rising ramp) wave.
 *
 * Registers a "SawtoothWave" generator in the DataSynthesizer registry.
 * Produces: y[t] = amplitude * (2 * frac - 1) + dc_offset
 * where frac is the fractional position within the cycle, offset so that
 * at t=0, phase=0 the output is 0 and rising (consistent with SineWave).
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

struct SawtoothWaveParams {
    int num_samples = 1000;
    float amplitude = 1.0f;
    float num_cycles = 1.0f;
    std::optional<int> cycle_length;
    std::optional<float> phase;
    std::optional<float> dc_offset;
};

DataTypeVariant generateSawtoothWave(SawtoothWaveParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("SawtoothWave: num_samples must be > 0");
    }
    if (params.num_cycles <= 0.0f) {
        throw std::invalid_argument("SawtoothWave: num_cycles must be > 0");
    }
    int const cyc_len = params.cycle_length.value_or(params.num_samples);
    if (cyc_len <= 0) {
        throw std::invalid_argument("SawtoothWave: cycle_length must be > 0");
    }

    float const ph = params.phase.value_or(0.0f);
    float const dc = params.dc_offset.value_or(0.0f);
    float const frequency = params.num_cycles / static_cast<float>(cyc_len);
    // Convert radian phase to cycle fraction for consistent phase semantics
    float const phase_frac = ph / (2.0f * std::numbers::pi_v<float>);

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        auto const t = static_cast<float>(i);
        // Shift by 0.5 so that at t=0, phase=0 the output is 0 (rising),
        // consistent with SineWave phase conventions.
        float pos = std::fmod(frequency * t + phase_frac + 0.5f, 1.0f);
        if (pos < 0.0f) {
            pos += 1.0f;
        }
        data[i] = params.amplitude * (2.0f * pos - 1.0f) + dc;
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const sawtooth_wave_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<SawtoothWaveParams>(
                "SawtoothWave",
                generateSawtoothWave,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a sawtooth (rising ramp) wave analog time series. "
                                       "num_cycles complete cycles over cycle_length samples (defaults to num_samples). "
                                       "Rising ramp from -amplitude to +amplitude with sharp reset.",
                        .category = "Periodic",
                        .output_type = "AnalogTimeSeries"});

}// namespace
