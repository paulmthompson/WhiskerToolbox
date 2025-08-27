#include "catch2/catch_test_macros.hpp"
#include "catch2/benchmark/catch_benchmark.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"

#include <vector>
#include <memory>

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

TEST_CASE("Benchmark Analog Event Threshold", "[!benchmark]") {
    auto ats_1k = create_test_data(1000);
    auto ats_10k = create_test_data(10000);
    auto ats_100k = create_test_data(100000);

    ThresholdParams params;
    params.thresholdValue = 5.0;
    params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
    params.lockoutTime = 0.0;

    BENCHMARK("Event Threshold 1k") {
        return event_threshold(ats_1k.get(), params);
    };

    BENCHMARK("Event Threshold 10k") {
        return event_threshold(ats_10k.get(), params);
    };

    BENCHMARK("Event Threshold 100k") {
        return event_threshold(ats_100k.get(), params);
    };
}
