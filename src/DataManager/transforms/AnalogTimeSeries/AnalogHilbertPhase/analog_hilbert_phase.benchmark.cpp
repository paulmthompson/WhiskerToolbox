#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

// Define a function that creates a sample AnalogTimeSeries
static std::shared_ptr<AnalogTimeSeries> create_test_data(size_t size) {
    std::vector<float> values;
    values.reserve(size);
    std::vector<TimeFrameIndex> times;
    times.reserve(size);
    constexpr float frequency = 1.0f;
    constexpr size_t sample_rate = 100;
    for (size_t i = 0; i < size; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        values.push_back(std::sin(2.0f * std::numbers::pi_v<float> * frequency * t));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(values, times);
}

TEST_CASE("Benchmark Analog Hilbert Phase", "[!benchmark]") {
    auto ats_1k = create_test_data(1000);
    auto ats_10k = create_test_data(10000);
    auto ats_100k = create_test_data(100000);

    HilbertPhaseParams params;
    params.lowFrequency = 0.5;
    params.highFrequency = 2.0;
    params.discontinuityThreshold = 100;

    BENCHMARK("Hilbert Phase 1k") {
        return hilbert_phase(ats_1k.get(), params);
    };

    BENCHMARK("Hilbert Phase 10k") {
        return hilbert_phase(ats_10k.get(), params);
    };

    BENCHMARK("Hilbert Phase 100k") {
        return hilbert_phase(ats_100k.get(), params);
    };
}
