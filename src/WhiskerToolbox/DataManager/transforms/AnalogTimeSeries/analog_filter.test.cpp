#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <iostream>

TEST_CASE("Filter Analog Time Series", "[transforms][analog_filter]") {
    // Create test data: sine wave at 10 Hz sampled at 1000 Hz
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    const size_t num_samples = 2000; // Increased for better steady-state
    const double sampling_rate = 1000.0;
    const double signal_freq = 10.0;

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * signal_freq * t)));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    auto series = std::make_shared<AnalogTimeSeries>(data, times);

    SECTION("Low-pass filter") {
        AnalogFilterParams params;
        // More aggressive lowpass: 4th order, lower cutoff
        params.filter_options = FilterDefaults::lowpass(3.0, sampling_rate, 4);

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that high frequency content is attenuated
        float max_amplitude = 0.0f;
        // Skip initial transient
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude, 
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 0.15f); // More stringent requirement
    }

    SECTION("High-pass filter") {
        AnalogFilterParams params;
        // More aggressive highpass: 4th order, higher cutoff
        params.filter_options = FilterDefaults::highpass(20.0, sampling_rate, 4);

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that low frequency content is attenuated
        float max_amplitude = 0.0f;
        // Skip initial transient
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude, 
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 0.15f); // More stringent requirement
    }

    SECTION("Band-pass filter around signal frequency") {
        AnalogFilterParams params;
        // Narrower bandpass, higher order
        params.filter_options = FilterDefaults::bandpass(9.0, 11.0, sampling_rate, 4);

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Signal should be preserved (allowing for some attenuation)
        float max_amplitude = 0.0f;
        // Skip longer transient for bandpass
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude, 
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude > 0.7f); // Signal should be mostly preserved
    }

    SECTION("Notch filter at signal frequency") {
        // Create a notch filter at 10 Hz with very narrow bandwidth
        auto filter_options = FilterDefaults::notch(signal_freq, sampling_rate, 100.0); // Much higher Q for narrower notch
        filter_options.zero_phase = true; // Use zero-phase filtering for better attenuation

        // Create input time series
        AnalogTimeSeries series(data, times);

        // Apply filter
        auto result = filterAnalogTimeSeries(&series, filter_options);
        REQUIRE(result.success);

        // Get filtered data
        auto filtered_data = result.filtered_data->getAnalogTimeSeries();

        // Skip first 500 samples to avoid transient response
        float max_amplitude = 0.0f;
        for (size_t i = 500; i < filtered_data.size(); ++i) {
            max_amplitude = std::max(max_amplitude, std::abs(filtered_data[i]));
        }

        // The notch filter should significantly attenuate the 10 Hz signal
        REQUIRE(max_amplitude < 0.2);
    }
}

TEST_CASE("AnalogFilterOperation Class Tests", "[transforms][analog_filter][operation]") {
    AnalogFilterOperation operation;

    SECTION("Basic operation properties") {
        REQUIRE(operation.getName() == "Filter");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(AnalogTimeSeries));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params);
        auto* filterParams = dynamic_cast<AnalogFilterParams*>(params.get());
        REQUIRE(filterParams);
        REQUIRE(filterParams->filter_options.type == FilterType::Butterworth);
        REQUIRE(filterParams->filter_options.response == FilterResponse::LowPass);
    }

    SECTION("Can apply to correct type") {
        auto series = std::make_shared<AnalogTimeSeries>();
        DataTypeVariant variant(series);
        REQUIRE(operation.canApply(variant));
    }

    SECTION("Execute with progress callback") {
        // Create test data
        std::vector<float> data(1000, 1.0f);
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < 1000; ++i) {
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
        }
        auto series = std::make_shared<AnalogTimeSeries>(data, times);
        DataTypeVariant input(series);

        // Create parameters
        auto params = std::make_unique<AnalogFilterParams>();
        params->filter_options = FilterDefaults::lowpass(10.0, 100.0, 4);

        bool progress_called = false;
        auto result = operation.execute(input, params.get(), 
            [&progress_called](int progress) {
                if (progress == 100) progress_called = true;
            });

        REQUIRE(progress_called);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        auto filtered = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == 1000);
    }
} 