#ifndef WHISKER_CONTACT_SCENARIO_CONFIG_HPP
#define WHISKER_CONTACT_SCENARIO_CONFIG_HPP

/**
 * @file WhiskerContactScenarioConfig.hpp
 * @brief Configuration for the whisker-contact multi-timeframe test scenario.
 */

#include <cstdint>
#include <string>

/**
 * @brief Parameters controlling whisker-contact scenario generation.
 */
struct WhiskerContactScenarioConfig {
    double duration_sec = 10.0;
    int master_hz = 30000;
    int time_hz = 500;

    std::string master_time_key = "Master";
    std::string time_time_key = "Time";

    std::string spikes_key = "Spikes";
    std::string contact_key = "Contact";
    std::string curvature_key = "Curvature";
    std::string angle_key = "Angle";

    float mean_contact_duration_frames = 10.0f;
    float mean_contact_gap_frames = 200.0f;

    float ar1_phi = 0.9f;
    float ar1_sigma = 0.436f;

    float baseline_spike_rate = 1.0f / 30000.0f;
    float contact_rate_boost = 50.0f / 30000.0f;
    float curvature_onset_rate_scale = 20.0f / 30000.0f;

    int onset_window_master_samples = 60;

    uint64_t seed = 42;

    /**
     * @brief Parent samples between consecutive time-frame indices.
     */
    [[nodiscard]] int samplesPerTimeFrame() const { return master_hz / time_hz; }

    [[nodiscard]] std::size_t masterSampleCount() const {
        return static_cast<std::size_t>(duration_sec * static_cast<double>(master_hz));
    }

    [[nodiscard]] std::size_t timeFrameCount() const {
        return static_cast<std::size_t>(duration_sec * static_cast<double>(time_hz));
    }
};

#endif// WHISKER_CONTACT_SCENARIO_CONFIG_HPP
