/**
 * @file UniformNoiseGenerator.cpp
 * @brief Analog time series generator that produces uniformly distributed noise.
 *
 * Registers a "UniformNoise" generator in the DataSynthesizer registry.
 * Produces i.i.d. samples drawn from U(min_value, max_value).
 * The generator is deterministic: the same seed always produces the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct UniformNoiseParams {
    int num_samples = 1000;
    float min_value = -1.0f;
    float max_value = 1.0f;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateUniformNoise(UniformNoiseParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("UniformNoise: num_samples must be > 0");
    }
    if (params.min_value >= params.max_value) {
        throw std::invalid_argument("UniformNoise: min_value must be < max_value");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::uniform_real_distribution<float> dist(params.min_value, params.max_value);

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = dist(rng);
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const uniform_noise_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<UniformNoiseParams>(
                "UniformNoise",
                generateUniformNoise,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates i.i.d. uniform noise samples in [min_value, max_value). "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Noise",
                        .output_type = "AnalogTimeSeries"});

}// namespace
