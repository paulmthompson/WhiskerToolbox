#ifndef POISSON_THINNING_HPP
#define POISSON_THINNING_HPP

/**
 * @file PoissonThinning.hpp
 * @brief Lewis-Shedler thinning for inhomogeneous Poisson event generation in tests.
 */

#include "TimeFrame/TimeFrameIndex.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

/**
 * @brief Generate event indices via Lewis-Shedler thinning.
 *
 * @pre rate.size() > 0
 * @pre All rate values must be non-negative.
 * @post Returned indices are sorted and unique.
 */
inline std::vector<TimeFrameIndex> thinInhomogeneousPoisson(
        std::span<float const> rate,
        uint64_t seed) {
    std::vector<TimeFrameIndex> events;
    if (rate.empty()) {
        return events;
    }

    double lambda_max = 0.0;
    for (float val: rate) {
        assert(val >= 0.0f && "thinInhomogeneousPoisson: rate must be non-negative");
        lambda_max = std::max(lambda_max, static_cast<double>(val));
    }

    if (lambda_max <= 0.0) {
        return events;
    }

    auto const num_samples = static_cast<int64_t>(rate.size());
    std::mt19937_64 rng(seed);
    std::exponential_distribution<double> exp_dist(lambda_max);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    double current_time = exp_dist(rng);
    while (current_time < static_cast<double>(num_samples)) {
        auto const idx = static_cast<int64_t>(current_time);
        assert(idx >= 0 && idx < num_samples);

        double const local_rate = static_cast<double>(rate[static_cast<std::size_t>(idx)]);
        if (uniform(rng) < local_rate / lambda_max) {
            events.emplace_back(idx);
        }

        current_time += exp_dist(rng);
    }

    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());
    return events;
}

#endif// POISSON_THINNING_HPP
