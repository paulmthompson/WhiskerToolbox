/**
 * @file RegularEventsGenerator.cpp
 * @brief DigitalEventSeries generator that produces evenly spaced events with optional jitter.
 *
 * Registers a "RegularEvents" generator in the DataSynthesizer registry.
 * Events are placed at regular intervals with optional Gaussian jitter.
 * The generator is deterministic: the same seed always produces the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct RegularEventsParams {
    int num_samples = 1000;
    int interval = 20;
    std::optional<float> jitter_stddev;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateRegularEvents(RegularEventsParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("RegularEvents: num_samples must be > 0");
    }
    if (params.interval <= 0) {
        throw std::invalid_argument("RegularEvents: interval must be > 0");
    }

    float const jitter = params.jitter_stddev.value_or(0.0f);
    if (jitter < 0.0f) {
        throw std::invalid_argument("RegularEvents: jitter_stddev must be >= 0");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::normal_distribution<float> jitter_dist(0.0f, jitter);

    auto const n = static_cast<int64_t>(params.num_samples);
    std::vector<TimeFrameIndex> events;

    for (int64_t t = 0; t < n; t += params.interval) {
        int64_t jittered_t = t;
        if (jitter > 0.0f) {
            jittered_t = t + static_cast<int64_t>(std::round(jitter_dist(rng)));
        }
        // Clamp to [0, num_samples)
        jittered_t = std::clamp(jittered_t, int64_t{0}, n - 1);
        events.emplace_back(jittered_t);
    }

    // Sort and deduplicate (jitter can cause out-of-order or duplicate times)
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());

    return std::make_shared<DigitalEventSeries>(std::move(events));
}

auto const regular_events_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<RegularEventsParams>(
                "RegularEvents",
                generateRegularEvents,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates evenly spaced events with optional Gaussian jitter. "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Events",
                        .output_type = "DigitalEventSeries"});

}// namespace
