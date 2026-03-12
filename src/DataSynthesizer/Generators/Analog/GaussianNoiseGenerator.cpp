/**
 * @file GaussianNoiseGenerator.cpp
 * @brief Analog time series generator that produces Gaussian (normal) noise.
 *
 * Registers a "GaussianNoise" generator in the DataSynthesizer registry.
 * Produces i.i.d. samples drawn from N(mean, stddev²).
 * The generator is deterministic: the same seed always produces the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct GaussianNoiseParams {
    int num_samples = 1000;
    float stddev = 1.0f;
    std::optional<float> mean;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateGaussianNoise(GaussianNoiseParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("GaussianNoise: num_samples must be > 0");
    }
    if (params.stddev < 0.0f) {
        throw std::invalid_argument("GaussianNoise: stddev must be >= 0");
    }

    float const mu = params.mean.value_or(0.0f);
    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::normal_distribution<float> dist(mu, params.stddev);

    auto const n = static_cast<size_t>(params.num_samples);
    std::vector<float> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = dist(rng);
    }

    return std::make_shared<AnalogTimeSeries>(std::move(data), n);
}

auto const gaussian_noise_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<GaussianNoiseParams>(
                "GaussianNoise",
                generateGaussianNoise,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates i.i.d. Gaussian (normal) noise samples. "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Noise",
                        .output_type = "AnalogTimeSeries"});

}// namespace
