/**
 * @file RegularIntervalsGenerator.cpp
 * @brief DigitalIntervalSeries generator that produces periodic on/off intervals.
 *
 * Registers a "RegularIntervals" generator in the DataSynthesizer registry.
 * Produces a series of non-overlapping intervals with constant on_duration and
 * off_duration, starting at start_offset. Purely deterministic (no RNG).
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

struct RegularIntervalsParams {
    int num_samples = 1000;
    int on_duration = 50;
    int off_duration = 50;
    std::optional<int> start_offset;
};

DataTypeVariant generateRegularIntervals(RegularIntervalsParams const & params) {
    if (params.num_samples <= 0) {
        throw std::invalid_argument("RegularIntervals: num_samples must be > 0");
    }
    if (params.on_duration <= 0) {
        throw std::invalid_argument("RegularIntervals: on_duration must be > 0");
    }
    if (params.off_duration <= 0) {
        throw std::invalid_argument("RegularIntervals: off_duration must be > 0");
    }

    int64_t const offset = static_cast<int64_t>(params.start_offset.value_or(0));
    auto const n = static_cast<int64_t>(params.num_samples);
    auto const on = static_cast<int64_t>(params.on_duration);
    auto const off = static_cast<int64_t>(params.off_duration);
    int64_t const period = on + off;

    std::vector<Interval> intervals;
    int64_t t = offset;
    while (t < n) {
        int64_t const start = t;
        int64_t const end = std::min(t + on - 1, n - 1);
        intervals.push_back(Interval{start, end});
        t += period;
    }

    return std::make_shared<DigitalIntervalSeries>(std::move(intervals));
}

auto const regular_intervals_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<RegularIntervalsParams>(
                "RegularIntervals",
                generateRegularIntervals,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates periodic on/off intervals with constant "
                                       "on_duration and off_duration. Intervals are placed "
                                       "starting at start_offset (default 0).",
                        .category = "Intervals",
                        .output_type = "DigitalIntervalSeries"});

}// namespace
