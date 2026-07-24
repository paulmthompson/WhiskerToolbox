#ifndef AR1_PROCESS_HPP
#define AR1_PROCESS_HPP

/**
 * @file AR1Process.hpp
 * @brief Generate zero-mean AR(1) Gaussian processes for test fixtures.
 */

#include <cstdint>
#include <random>
#include <vector>

/**
 * @brief Generate a zero-mean AR(1) process.
 *
 * x[0] = ε[0], x[t] = φ·x[t-1] + ε[t], ε[t] ~ N(0, σ²).
 *
 * @pre num_samples > 0
 * @pre sigma > 0
 * @post result.size() == num_samples
 */
inline std::vector<float> generateAR1(
        std::size_t num_samples,
        float phi,
        float sigma,
        uint64_t seed) {
    std::vector<float> values(num_samples);
    if (num_samples == 0) {
        return values;
    }

    std::mt19937_64 rng(seed);
    std::normal_distribution<float> noise(0.0f, sigma);

    values[0] = noise(rng);
    for (std::size_t t = 1; t < num_samples; ++t) {
        values[t] = phi * values[t - 1] + noise(rng);
    }
    return values;
}

#endif// AR1_PROCESS_HPP
