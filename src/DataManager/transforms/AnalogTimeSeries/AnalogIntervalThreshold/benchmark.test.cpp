#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/transforms/AnalogTimeSeries/AnalogIntervalThreshold/analog_interval_threshold.hpp"

#include <memory>
#include <vector>

// Define a function that creates a sample AnalogTimeSeries
static std::shared_ptr<AnalogTimeSeries> create_test_data(size_t size) {
    std::vector<float> values;
    values.reserve(size);
    std::vector<TimeFrameIndex> times;
    times.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        values.push_back(static_cast<float>(i % 10));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i * 10)));
    }
    return std::make_shared<AnalogTimeSeries>(values, times);
}

TEST_CASE("Benchmark Analog Interval Threshold", "[!benchmark]") {
    auto ats_1k = create_test_data(1000);
    auto ats_10k = create_test_data(10000);
    auto ats_100k = create_test_data(100000);

    IntervalThresholdParams params;
    params.thresholdValue = 5.0;
    params.direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
    params.lockoutTime = 0.0;
    params.minDuration = 0.0;
    params.missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;

    BENCHMARK("Interval Threshold 1k") {
        return interval_threshold(ats_1k.get(), params);
    };

    BENCHMARK("Interval Threshold 10k") {
        return interval_threshold(ats_10k.get(), params);
    };

    BENCHMARK("Interval Threshold 100k") {
        return interval_threshold(ats_100k.get(), params);
    };
}
