/**
 * @file BurstEventsGenerator.cpp
 * @brief DigitalEventSeries generator that produces clustered burst event patterns.
 *
 * Registers a "BurstEvents" generator in the DataSynthesizer registry.
 * Generates bursts of events: bursts arrive at a given rate, and within each burst,
 * events occur at a higher within-burst rate for a specified burst duration.
 * The generator is deterministic: the same seed always produces the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct BurstEventsParams {
    int num_samples = 10000;
    float burst_rate = 0.005f;
    float within_burst_rate = 0.5f;
    int burst_duration = 20;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateBurstEvents(BurstEventsParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("BurstEvents: num_samples must be > 0");
    }
    if (params.burst_rate <= 0.0f) {
        throw std::invalid_argument("BurstEvents: burst_rate must be > 0");
    }
    if (params.within_burst_rate <= 0.0f) {
        throw std::invalid_argument("BurstEvents: within_burst_rate must be > 0");
    }
    if (params.burst_duration <= 0) {
        throw std::invalid_argument("BurstEvents: burst_duration must be > 0");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::exponential_distribution<double> burst_arrival_dist(
            static_cast<double>(params.burst_rate));
    std::exponential_distribution<double> within_burst_dist(
            static_cast<double>(params.within_burst_rate));

    auto const n = static_cast<double>(params.num_samples);
    std::vector<TimeFrameIndex> events;

    // Generate burst onset times via Poisson process
    double burst_start = burst_arrival_dist(rng);
    while (burst_start < n) {
        double const burst_end = std::min(
                burst_start + static_cast<double>(params.burst_duration), n);

        // Generate within-burst events via another Poisson process
        double event_time = burst_start + within_burst_dist(rng);
        while (event_time < burst_end) {
            events.emplace_back(static_cast<int64_t>(event_time));
            event_time += within_burst_dist(rng);
        }

        burst_start += burst_arrival_dist(rng);
    }

    // Sort and deduplicate (multiple bursts could share boundary events)
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());

    return std::make_shared<DigitalEventSeries>(std::move(events));
}

auto const burst_events_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<BurstEventsParams>(
                "BurstEvents",
                generateBurstEvents,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates clustered burst event patterns. Bursts arrive "
                                       "via a Poisson process at burst_rate, and within each burst, "
                                       "events occur at within_burst_rate for burst_duration samples. "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Events",
                        .output_type = "DigitalEventSeries"});

}// namespace
