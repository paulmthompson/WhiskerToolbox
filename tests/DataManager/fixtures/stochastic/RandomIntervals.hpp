#ifndef RANDOM_INTERVALS_HPP
#define RANDOM_INTERVALS_HPP

/**
 * @file RandomIntervals.hpp
 * @brief Exponentially distributed random intervals for test fixtures.
 */

#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

/**
 * @brief Generate randomly spaced intervals with exponential durations and gaps.
 *
 * @pre num_samples > 0
 * @pre mean_duration > 0
 * @pre mean_gap > 0
 */
inline std::vector<TimeFrameInterval> generateRandomIntervals(
        int num_samples,
        float mean_duration,
        float mean_gap,
        uint64_t seed) {
    std::vector<TimeFrameInterval> intervals;
    if (num_samples <= 0) {
        return intervals;
    }

    std::mt19937_64 rng(seed);
    std::exponential_distribution<double> duration_dist(
            1.0 / static_cast<double>(mean_duration));
    std::exponential_distribution<double> gap_dist(
            1.0 / static_cast<double>(mean_gap));

    auto const n = static_cast<double>(num_samples);
    double t = gap_dist(rng);
    while (t < n) {
        auto const duration = std::max(1.0, std::round(duration_dist(rng)));
        auto const start = static_cast<int64_t>(t);
        int64_t const end = std::min(
                static_cast<int64_t>(t + duration - 1.0),
                static_cast<int64_t>(num_samples) - 1);

        if (start < static_cast<int64_t>(num_samples)) {
            intervals.push_back(TimeFrameInterval{TimeFrameIndex(start), TimeFrameIndex(end)});
        }

        t += duration + gap_dist(rng);
    }

    return intervals;
}

#endif// RANDOM_INTERVALS_HPP
