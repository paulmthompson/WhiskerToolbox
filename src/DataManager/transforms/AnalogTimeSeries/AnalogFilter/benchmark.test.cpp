#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "analog_filter.hpp"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

TEST_CASE("Benchmark: Analog Filter", "[benchmark][analog_filter]") {
    // Create a large test data: sine wave at 100 Hz sampled at 10 kHz
    size_t const num_samples = 100000;
    double const sampling_rate = 10000.0;
    double const signal_freq = 100.0;
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * signal_freq * t)));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    auto series = std::make_shared<AnalogTimeSeries>(data, times);

    AnalogFilterParams params;
    params.filter_type = AnalogFilterParams::FilterType::Lowpass;
    params.cutoff_frequency = 50.0;
    params.order = 4;
    params.sampling_rate = sampling_rate;

    BENCHMARK("4th Order Lowpass Filter") {
        return filter_analog(series.get(), params);
    };
}
