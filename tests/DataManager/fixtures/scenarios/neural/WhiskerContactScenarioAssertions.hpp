#ifndef WHISKER_CONTACT_SCENARIO_ASSERTIONS_HPP
#define WHISKER_CONTACT_SCENARIO_ASSERTIONS_HPP

/**
 * @file WhiskerContactScenarioAssertions.hpp
 * @brief Statistical assertion helpers for whisker-contact scenario tests.
 */

#include "fixtures/scenarios/neural/WhiskerContactScenario.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace whisker_contact_assertions {

inline double computeMean(std::span<float const> values) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float v: values) {
        sum += static_cast<double>(v);
    }
    return sum / static_cast<double>(values.size());
}

inline double computeLag1Autocorrelation(std::span<float const> values) {
    if (values.size() < 2) {
        return 0.0;
    }
    double const mean = computeMean(values);
    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t t = 1; t < values.size(); ++t) {
        double const x0 = static_cast<double>(values[t - 1]) - mean;
        double const x1 = static_cast<double>(values[t]) - mean;
        numerator += x0 * x1;
        denominator += x0 * x0;
    }
    if (denominator == 0.0) {
        return 0.0;
    }
    return numerator / denominator;
}

inline std::vector<double> computeRanks(std::span<double const> data) {
    std::vector<std::size_t> indices(data.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
        return data[a] < data[b];
    });
    std::vector<double> ranks(data.size());
    for (std::size_t rank_value = 0; rank_value < indices.size(); ++rank_value) {
        ranks[indices[rank_value]] = static_cast<double>(rank_value);
    }
    return ranks;
}

inline double computePearsonCorrelation(
        std::span<double const> x,
        std::span<double const> y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }
    double const x_mean = computeMean(std::vector<float>(x.begin(), x.end()));
    double const y_mean = computeMean(std::vector<float>(y.begin(), y.end()));
    double numerator = 0.0;
    double x_var = 0.0;
    double y_var = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        double const dx = x[i] - x_mean;
        double const dy = y[i] - y_mean;
        numerator += dx * dy;
        x_var += dx * dx;
        y_var += dy * dy;
    }
    if (x_var == 0.0 || y_var == 0.0) {
        return 0.0;
    }
    return numerator / std::sqrt(x_var * y_var);
}

inline double computeSpearmanCorrelation(
        std::span<double const> x,
        std::span<double const> y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }
    auto const x_ranks = computeRanks(x);
    auto const y_ranks = computeRanks(y);
    return computePearsonCorrelation(x_ranks, y_ranks);
}

inline bool isTimeFrameInContact(
        DigitalIntervalSeries const & contact,
        int64_t time_frame_index) {
    for (auto interval: contact.view()) {
        auto const value = interval.value();
        if (time_frame_index >= value.start && time_frame_index <= value.end) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Verify all scenario keys are registered on the correct timeframes.
 */
inline bool assertScenarioStructure(
        DataManager & dm,
        WhiskerContactScenario const & scenario) {
    auto const & cfg = scenario.config;

    if (!dm.getData<DigitalEventSeries>(cfg.spikes_key) || !dm.getData<DigitalIntervalSeries>(cfg.contact_key) || !dm.getData<AnalogTimeSeries>(cfg.curvature_key) || !dm.getData<AnalogTimeSeries>(cfg.angle_key)) {
        return false;
    }

    return dm.getTimeKey(cfg.spikes_key) == TimeKey(cfg.master_time_key) && dm.getTimeKey(cfg.contact_key) == TimeKey(cfg.time_time_key) && dm.getTimeKey(cfg.curvature_key) == TimeKey(cfg.time_time_key) && dm.getTimeKey(cfg.angle_key) == TimeKey(cfg.time_time_key);
}

/**
 * @brief Verify mean contact interval duration is near the expected value.
 */
inline bool assertContactDuration(
        DigitalIntervalSeries const & contact,
        float expected,
        float tolerance) {
    if (contact.size() == 0) {
        return false;
    }
    double sum = 0.0;
    for (auto interval: contact.view()) {
        auto const value = interval.value();
        sum += static_cast<double>(value.end - value.start + 1);
    }
    double const mean = sum / static_cast<double>(contact.size());
    return std::abs(mean - static_cast<double>(expected)) <= static_cast<double>(tolerance);
}

/**
 * @brief Verify spike rate is elevated during contact intervals.
 */
inline bool assertSpikeRateElevatedDuringContact(
        WhiskerContactScenario const & scenario,
        double min_ratio) {
    auto const & cfg = scenario.config;
    auto const samples_per_frame = cfg.samplesPerTimeFrame();

    std::size_t contact_master_samples = 0;
    std::size_t non_contact_master_samples = 0;
    std::size_t spikes_in_contact = 0;
    std::size_t spikes_outside_contact = 0;

    for (auto event: scenario.spikes->view()) {
        auto const master_idx = event.time().getValue();
        auto const time_frame = master_idx / samples_per_frame;
        if (isTimeFrameInContact(*scenario.contact, time_frame)) {
            ++spikes_in_contact;
        } else {
            ++spikes_outside_contact;
        }
    }

    for (std::size_t m = 0; m < cfg.masterSampleCount(); ++m) {
        auto const time_frame = static_cast<int64_t>(m / static_cast<std::size_t>(samples_per_frame));
        if (isTimeFrameInContact(*scenario.contact, time_frame)) {
            ++contact_master_samples;
        } else {
            ++non_contact_master_samples;
        }
    }

    if (contact_master_samples == 0 || non_contact_master_samples == 0) {
        return false;
    }

    double const rate_in_contact = static_cast<double>(spikes_in_contact) / static_cast<double>(contact_master_samples);
    double const rate_outside = static_cast<double>(spikes_outside_contact) / static_cast<double>(non_contact_master_samples);

    if (rate_outside <= 0.0) {
        return rate_in_contact > 0.0;
    }
    return rate_in_contact / rate_outside >= min_ratio;
}

/**
 * @brief Verify higher curvature at contact onset correlates with more onset-window spikes.
 */
inline bool assertCurvatureOnsetModulation(
        WhiskerContactScenario const & scenario,
        double min_spearman) {
    auto const & cfg = scenario.config;
    auto const samples_per_frame = cfg.samplesPerTimeFrame();
    auto const curvature = scenario.curvature->getAnalogTimeSeries();

    std::vector<double> onset_curvatures;
    std::vector<double> onset_spike_counts;

    for (auto interval: scenario.contact->view()) {
        auto const value = interval.value();
        auto const onset_frame = value.start;
        if (static_cast<std::size_t>(onset_frame) >= curvature.size()) {
            continue;
        }

        auto const master_onset = static_cast<int64_t>(onset_frame) * samples_per_frame;
        auto const master_end = master_onset + cfg.onset_window_master_samples;

        std::size_t spike_count = 0;
        for (auto event: scenario.spikes->view()) {
            auto const idx = event.time().getValue();
            if (idx >= master_onset && idx < master_end) {
                ++spike_count;
            }
        }

        onset_curvatures.push_back(static_cast<double>(curvature[static_cast<std::size_t>(onset_frame)]));
        onset_spike_counts.push_back(static_cast<double>(spike_count));
    }

    if (onset_curvatures.size() < 3) {
        return false;
    }

    double const correlation = computeSpearmanCorrelation(onset_curvatures, onset_spike_counts);
    return correlation >= min_spearman;
}

/**
 * @brief Verify lag-1 autocorrelation is near the expected AR(1) coefficient.
 */
inline bool assertAR1Autocorrelation(
        AnalogTimeSeries const & signal,
        float expected_phi,
        float tolerance) {
    double const observed = computeLag1Autocorrelation(signal.getAnalogTimeSeries());
    return std::abs(observed - static_cast<double>(expected_phi)) <= static_cast<double>(tolerance);
}

}// namespace whisker_contact_assertions

#endif// WHISKER_CONTACT_SCENARIO_ASSERTIONS_HPP
