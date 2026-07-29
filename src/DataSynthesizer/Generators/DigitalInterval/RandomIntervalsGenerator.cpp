/**
 * @file RandomIntervalsGenerator.cpp
 * @brief DigitalIntervalSeries generator that produces randomly spaced intervals.
 *
 * Registers a "RandomIntervals" generator in the DataSynthesizer registry.
 * Interval durations and gaps are drawn from exponential distributions with
 * specified means. The generator is deterministic: the same seed always produces
 * the same output.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct RandomIntervalsParams {
    int num_samples = 10000;
    float mean_duration = 50.0f;
    float mean_gap = 100.0f;
    std::optional<uint64_t> seed;
};

DataTypeVariant generateRandomIntervals(RandomIntervalsParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("RandomIntervals: num_samples must be > 0");
    }
    if (params.mean_duration <= 0.0f) {
        throw std::invalid_argument("RandomIntervals: mean_duration must be > 0");
    }
    if (params.mean_gap <= 0.0f) {
        throw std::invalid_argument("RandomIntervals: mean_gap must be > 0");
    }

    std::mt19937_64 rng(params.seed.value_or(42ULL));
    std::exponential_distribution<double> duration_dist(
            1.0 / static_cast<double>(params.mean_duration));
    std::exponential_distribution<double> gap_dist(
            1.0 / static_cast<double>(params.mean_gap));

    auto const n = static_cast<double>(params.num_samples);
    std::vector<TimeFrameInterval> intervals;

    // Start with a gap before the first interval
    double t = gap_dist(rng);
    while (t < n) {
        auto const duration = std::max(1.0, std::round(duration_dist(rng)));
        auto const start = static_cast<int64_t>(t);
        int64_t const end = std::min(
                static_cast<int64_t>(t + duration - 1.0),
                static_cast<int64_t>(n) - 1);

        if (start < static_cast<int64_t>(n)) {
            intervals.push_back(TimeFrameInterval{TimeFrameIndex{static_cast<int64_t>(start)},
                                                  TimeFrameIndex{static_cast<int64_t>(end)}});
        }

        t += duration + gap_dist(rng);
    }

    auto series = std::make_shared<DigitalIntervalSeries>(std::move(intervals));

    std::vector<int> frame_times(static_cast<std::size_t>(params.num_samples));
    for (int i = 0; i < params.num_samples; ++i) {
        frame_times[static_cast<std::size_t>(i)] = i;
    }
    series->setTimeFrame(std::make_shared<TimeFrame>(frame_times));

    return series;
}

auto const random_intervals_reg =
        Neuralyzer::DataSynthesizer::RegisterGenerator<RandomIntervalsParams>(
                "RandomIntervals",
                generateRandomIntervals,
                Neuralyzer::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates randomly spaced intervals with exponentially "
                                       "distributed durations and gaps. "
                                       "Deterministic: same seed always produces the same output.",
                        .category = "Intervals",
                        .output_type = "DigitalIntervalSeries"});

}// namespace
