/**
 * @file InhomogeneousPoissonEventsGenerator.cpp
 * @brief DigitalEventSeries generator for an inhomogeneous Poisson process.
 *
 * Registers an "InhomogeneousPoissonEvents" generator that produces events
 * with a time-varying rate lambda(t) read from an AnalogTimeSeries in the
 * DataManager. Uses the Lewis-Shedler thinning algorithm for correctness.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct InhomogeneousPoissonEventsParams {
    std::string rate_signal_key;
    std::optional<float> rate_scale;
    std::optional<uint64_t> seed;
};

/**
 * @brief Generate events via an inhomogeneous Poisson process (Lewis-Shedler thinning).
 *
 * @pre ctx.data_manager must not be null.
 * @pre rate_signal_key must refer to an existing AnalogTimeSeries in the DataManager.
 * @pre All rate values (after scaling) must be non-negative.
 *
 * Algorithm (Lewis-Shedler thinning):
 * 1. Read the rate signal lambda(t) = rate_scale * analog_values[t]
 * 2. Find lambda_max = max(lambda(t))
 * 3. Generate candidate events from a homogeneous Poisson process with rate lambda_max
 * 4. Accept each candidate at time t with probability lambda(t) / lambda_max
 */
DataTypeVariant generateInhomogeneousPoissonEvents(
        InhomogeneousPoissonEventsParams const & params,
        WhiskerToolbox::DataSynthesizer::GeneratorContext const & ctx) {

    if (!ctx.data_manager) {
        throw std::invalid_argument(
                "InhomogeneousPoissonEvents: requires DataManager context");
    }
    if (params.rate_signal_key.empty()) {
        throw std::invalid_argument(
                "InhomogeneousPoissonEvents: rate_signal_key must not be empty");
    }
    auto const rate_scale = params.rate_scale.value_or(1.0f);
    if (rate_scale <= 0.0f) {
        throw std::invalid_argument(
                "InhomogeneousPoissonEvents: rate_scale must be > 0");
    }

    auto rate_series = const_cast<DataManager *>(ctx.data_manager)
                               ->getData<AnalogTimeSeries>(params.rate_signal_key);
    if (!rate_series) {
        throw std::invalid_argument(
                "InhomogeneousPoissonEvents: rate_signal_key '" +
                params.rate_signal_key + "' not found in DataManager");
    }

    auto const rate_data = rate_series->getAnalogTimeSeries();
    auto const num_samples = static_cast<int64_t>(rate_data.size());

    if (num_samples == 0) {
        return std::make_shared<DigitalEventSeries>(std::vector<TimeFrameIndex>{});
    }

    // Compute scaled rates and find lambda_max
    auto const scale = static_cast<double>(rate_scale);
    double lambda_max = 0.0;
    for (auto val: rate_data) {
        double const scaled = static_cast<double>(val) * scale;
        if (scaled < 0.0) {
            throw std::invalid_argument(
                    "InhomogeneousPoissonEvents: rate signal contains negative "
                    "values (after scaling). All rates must be >= 0.");
        }
        lambda_max = std::max(lambda_max, scaled);
    }

    if (lambda_max <= 0.0) {
        // Rate is zero everywhere — no events possible
        return std::make_shared<DigitalEventSeries>(std::vector<TimeFrameIndex>{});
    }

    // Lewis-Shedler thinning algorithm
    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::exponential_distribution<double> exp_dist(lambda_max);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    std::vector<TimeFrameIndex> events;

    double current_time = exp_dist(rng);
    while (current_time < static_cast<double>(num_samples)) {
        auto const idx = static_cast<int64_t>(current_time);
        assert(idx >= 0 && idx < num_samples);

        double const local_rate =
                static_cast<double>(rate_data[static_cast<std::size_t>(idx)]) * scale;

        // Accept with probability local_rate / lambda_max
        if (uniform(rng) < local_rate / lambda_max) {
            events.emplace_back(idx);
        }

        current_time += exp_dist(rng);
    }

    // Deduplicate (floor can map distinct continuous times to the same integer)
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());

    return std::make_shared<DigitalEventSeries>(std::move(events));
}

auto const inhomogeneous_poisson_events_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<InhomogeneousPoissonEventsParams>(
                "InhomogeneousPoissonEvents",
                generateInhomogeneousPoissonEvents,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description =
                                "Generates events via an inhomogeneous Poisson process "
                                "with time-varying rate read from an AnalogTimeSeries in "
                                "the DataManager. Uses the Lewis-Shedler thinning algorithm. "
                                "rate_signal_key specifies the DataManager key for the rate "
                                "signal, and rate_scale multiplies the signal values. "
                                "Deterministic: same seed always produces the same output.",
                        .category = "Events",
                        .output_type = "DigitalEventSeries"});

}// namespace
