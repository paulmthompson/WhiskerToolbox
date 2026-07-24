#ifndef WHISKER_CONTACT_SCENARIO_HPP
#define WHISKER_CONTACT_SCENARIO_HPP

/**
 * @file WhiskerContactScenario.hpp
 * @brief Generates and populates the whisker-contact multi-timeframe test scenario.
 */

#include "fixtures/builders/TimeFrameBuilder.hpp"
#include "fixtures/scenarios/neural/WhiskerContactScenarioConfig.hpp"
#include "fixtures/stochastic/AR1Process.hpp"
#include "fixtures/stochastic/IntervalModulatedRate.hpp"
#include "fixtures/stochastic/PoissonThinning.hpp"
#include "fixtures/stochastic/RandomIntervals.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <vector>

/**
 * @brief Generated whisker-contact scenario data objects.
 */
struct WhiskerContactScenario {
    WhiskerContactScenarioConfig config;

    std::shared_ptr<TimeFrame> master_time;
    std::shared_ptr<TimeFrame> time_time;

    std::shared_ptr<DigitalEventSeries> spikes;
    std::shared_ptr<DigitalIntervalSeries> contact;
    std::shared_ptr<AnalogTimeSeries> curvature;
    std::shared_ptr<AnalogTimeSeries> angle;

    /**
     * @brief Generate all scenario data deterministically from config.
     */
    static WhiskerContactScenario generate(WhiskerContactScenarioConfig const & cfg) {
        WhiskerContactScenario scenario;
        scenario.config = cfg;

        auto const master_count = cfg.masterSampleCount();
        auto const time_count = cfg.timeFrameCount();
        auto const samples_per_frame = cfg.samplesPerTimeFrame();

        scenario.master_time = TimeFrameBuilder().withCountFrom(master_count, 0).build();
        scenario.time_time = TimeFrameBuilder()
                                     .withDerivedClock(time_count, samples_per_frame, 0)
                                     .build();

        auto curvature_values = generateAR1(time_count, cfg.ar1_phi, cfg.ar1_sigma, cfg.seed + 1);
        auto angle_values = generateAR1(time_count, cfg.ar1_phi, cfg.ar1_sigma, cfg.seed + 2);

        auto interval_data = generateRandomIntervals(
                static_cast<int>(time_count),
                cfg.mean_contact_duration_frames,
                cfg.mean_contact_gap_frames,
                cfg.seed + 3);

        auto rate = buildSpikeRate(
                master_count,
                time_count,
                samples_per_frame,
                curvature_values,
                interval_data,
                cfg.baseline_spike_rate,
                cfg.contact_rate_boost,
                cfg.curvature_onset_rate_scale,
                cfg.onset_window_master_samples);

        scenario.curvature = std::make_shared<AnalogTimeSeries>(
                std::move(curvature_values), time_count);
        scenario.curvature->setTimeFrame(scenario.time_time);

        scenario.angle = std::make_shared<AnalogTimeSeries>(
                std::move(angle_values), time_count);
        scenario.angle->setTimeFrame(scenario.time_time);

        scenario.contact = std::make_shared<DigitalIntervalSeries>(std::move(interval_data));
        scenario.contact->setTimeFrame(scenario.time_time);

        auto spike_indices = thinInhomogeneousPoisson(rate, cfg.seed + 4);
        scenario.spikes = std::make_shared<DigitalEventSeries>(std::move(spike_indices));
        scenario.spikes->setTimeFrame(scenario.master_time);

        return scenario;
    }

    /**
     * @brief Register timeframes and data in a DataManager.
     */
    void populate(DataManager & dm) const {
        dm.setTime(TimeKey(config.master_time_key), master_time, true);
        dm.setTime(TimeKey(config.time_time_key), time_time, true);

        dm.setData<DigitalEventSeries>(config.spikes_key, spikes, TimeKey(config.master_time_key));
        dm.setData<DigitalIntervalSeries>(config.contact_key, contact, TimeKey(config.time_time_key));
        dm.setData<AnalogTimeSeries>(config.curvature_key, curvature, TimeKey(config.time_time_key));
        dm.setData<AnalogTimeSeries>(config.angle_key, angle, TimeKey(config.time_time_key));
    }
};

#endif// WHISKER_CONTACT_SCENARIO_HPP
