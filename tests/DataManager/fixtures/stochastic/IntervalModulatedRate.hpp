#ifndef INTERVAL_MODULATED_RATE_HPP
#define INTERVAL_MODULATED_RATE_HPP

/**
 * @file IntervalModulatedRate.hpp
 * @brief Build master-clock spike rate signals from contact intervals and curvature.
 */

#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace interval_modulated_rate_detail {

inline bool isInInterval(Interval const & interval, int64_t frame_index) {
    return frame_index >= interval.start && frame_index <= interval.end;
}

inline std::optional<Interval> findContainingInterval(
        std::span<Interval const> intervals,
        int64_t frame_index) {
    for (auto const & interval: intervals) {
        if (isInInterval(interval, frame_index)) {
            return interval;
        }
    }
    return std::nullopt;
}

}// namespace interval_modulated_rate_detail

/**
 * @brief Build per-master-sample spike rate with contact and curvature-onset modulation.
 *
 * For master sample m, camera frame t = m / samples_per_time_frame:
 *   λ(m) = baseline + contact_boost·1{in_contact}
 *        + curvature_scale·max(0, κ_onset)·1{onset_window}
 *
 * κ_onset is curvature at the start frame of the active contact interval.
 * onset_window covers the first onset_window_master_samples of each contact interval
 * after upsampling to master resolution.
 *
 * @pre master_sample_count > 0
 * @pre curvature.size() == time_frame_count
 * @pre samples_per_time_frame > 0
 */
inline std::vector<float> buildSpikeRate(
        std::size_t master_sample_count,
        std::size_t time_frame_count,
        int samples_per_time_frame,
        std::span<float const> curvature,
        std::span<Interval const> contact_intervals,
        float baseline_rate,
        float contact_rate_boost,
        float curvature_onset_rate_scale,
        int onset_window_master_samples) {
    std::vector<float> rate(master_sample_count, baseline_rate);
    if (master_sample_count == 0 || curvature.empty()) {
        return rate;
    }

    for (std::size_t m = 0; m < master_sample_count; ++m) {
        auto const time_frame = static_cast<int64_t>(m / static_cast<std::size_t>(samples_per_time_frame));
        if (time_frame < 0 || static_cast<std::size_t>(time_frame) >= time_frame_count) {
            continue;
        }

        auto const containing = interval_modulated_rate_detail::findContainingInterval(
                contact_intervals, time_frame);
        if (!containing.has_value()) {
            continue;
        }

        rate[m] += contact_rate_boost;

        auto const onset_frame = containing->start;
        auto const master_onset = static_cast<std::size_t>(onset_frame) * static_cast<std::size_t>(samples_per_time_frame);
        auto const in_onset_window = m >= master_onset && m < master_onset + static_cast<std::size_t>(onset_window_master_samples);

        if (in_onset_window && static_cast<std::size_t>(onset_frame) < curvature.size()) {
            float const kappa_onset = std::max(0.0f, curvature[static_cast<std::size_t>(onset_frame)]);
            rate[m] += curvature_onset_rate_scale * kappa_onset;
        }
    }

    return rate;
}

#endif// INTERVAL_MODULATED_RATE_HPP
