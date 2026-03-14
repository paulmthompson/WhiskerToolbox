/**
 * @file PoissonEventsGenerator.cpp
 * @brief DigitalEventSeries generator that produces events via a homogeneous Poisson process.
 *
 * Registers a "PoissonEvents" generator in the DataSynthesizer registry.
 * Events are generated across [0, num_samples) with a constant rate lambda (events per sample).
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

struct PoissonEventsParams {
    int num_samples = 1000;
    float lambda = 0.05f;
    std::optional<uint64_t> seed;
};

DataTypeVariant generatePoissonEvents(PoissonEventsParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("PoissonEvents: num_samples must be > 0");
    }
    if (params.lambda <= 0.0f) {
        throw std::invalid_argument("PoissonEvents: lambda must be > 0");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::exponential_distribution<double> dist(static_cast<double>(params.lambda));

    auto const n = static_cast<int64_t>(params.num_samples);
    std::vector<TimeFrameIndex> events;

    double current_time = dist(rng);
    while (current_time < static_cast<double>(n)) {
        events.emplace_back(static_cast<int64_t>(current_time));
        current_time += dist(rng);
    }

    // Deduplicate (floor can map distinct continuous times to the same integer)
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());

    return std::make_shared<DigitalEventSeries>(std::move(events));
}

auto const poisson_events_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<PoissonEventsParams>(
                "PoissonEvents",
                generatePoissonEvents,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates events via a homogeneous Poisson process with "
                                       "constant rate lambda (events per sample). "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Events",
                        .output_type = "DigitalEventSeries"});

}// namespace
