#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/filter/FilterFactory.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <iostream>

TEST_CASE("Data Transform: Filter Analog Time Series", "[transforms][analog_filter]") {
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
        // Use the new filter interface with Butterworth lowpass
        auto filter = FilterFactory::createButterworthLowpass<4>(3.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

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
        // Use the new filter interface with Butterworth highpass
        auto filter = FilterFactory::createButterworthHighpass<4>(20.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

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
        // Use the new filter interface with Butterworth bandpass
        auto filter = FilterFactory::createButterworthBandpass<4>(9.0, 11.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

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
        // Create a notch filter at 10 Hz with very narrow bandwidth using RBJ
        auto filter = FilterFactory::createRBJBandstop(signal_freq, sampling_rate, 200.0, true); // Zero-phase for better attenuation
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);

        // Get filtered data
        auto& filtered_data = filtered->getAnalogTimeSeries();

        // Skip first 1000 samples to avoid transient response
        float max_amplitude = 0.0f;
        for (size_t i = 1000; i < filtered_data.size(); ++i) {
            max_amplitude = std::max(max_amplitude, std::abs(filtered_data[i]));
        }

        // The notch filter should significantly attenuate the 10 Hz signal
        REQUIRE(max_amplitude < 0.2);
    }
}

TEST_CASE("Data Transform: Filter Analog Time Series - Operation Class Tests", "[transforms][analog_filter][operation]") {
    AnalogFilterOperation operation;

    SECTION("Basic operation properties") {
        REQUIRE(operation.getName() == "Filter");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params);
        auto* filterParams = dynamic_cast<AnalogFilterParams*>(params.get());
        REQUIRE(filterParams);
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

        // Create parameters using new filter interface
        auto filter = FilterFactory::createButterworthLowpass<4>(10.0, 100.0, false);
        auto params = std::make_unique<AnalogFilterParams>(
            AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter))));

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

TEST_CASE("Data Transform: Filter Analog Time Series - New Interface Features", "[transforms][analog_filter][new_interface]") {
    // Create test data
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    const size_t num_samples = 1000;
    const double sampling_rate = 1000.0;

    for (size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i % 10)); // Simple pattern
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    auto series = std::make_shared<AnalogTimeSeries>(data, times);

    SECTION("Filter factory function approach") {
        // Create parameters using factory function
        auto params = AnalogFilterParams::withFactory([]() {
            return FilterFactory::createButterworthLowpass<2>(50.0, 1000.0, false);
        });

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Direct filter instance approach") {
        // Create filter instance directly
        auto filter = FilterFactory::createChebyshevILowpass<3>(100.0, 1000.0, 1.0, true);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that filter name is available
        REQUIRE_FALSE(params.getFilterName().empty());
        REQUIRE(params.getFilterName().find("Chebyshev I") != std::string::npos);
    }

    SECTION("Default parameters test") {
        // Test that default parameters work correctly
        auto params = AnalogFilterParams::createDefault(1000.0, 75.0);

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Default constructor test") {
        // Test that default constructor creates a working filter
        auto params = AnalogFilterParams();  // Should use default constructor

        REQUIRE(params.isValid());
        REQUIRE_FALSE(params.getFilterName().empty());

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Different filter types comparison") {
        // Compare different filter types on the same data
        auto butterworth = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        auto chebyshev1 = FilterFactory::createChebyshevILowpass<4>(50.0, sampling_rate, 1.0, false);
        auto chebyshev2 = FilterFactory::createChebyshevIILowpass<4>(50.0, sampling_rate, 20.0, false);
        auto rbj = FilterFactory::createRBJLowpass(50.0, sampling_rate, 0.707, false);

        auto params_butter = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(butterworth)));
        auto params_cheby1 = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(chebyshev1)));
        auto params_cheby2 = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(chebyshev2)));
        auto params_rbj = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(rbj)));

        auto filtered_butter = filter_analog(series.get(), params_butter);
        auto filtered_cheby1 = filter_analog(series.get(), params_cheby1);
        auto filtered_cheby2 = filter_analog(series.get(), params_cheby2);
        auto filtered_rbj = filter_analog(series.get(), params_rbj);

        REQUIRE(filtered_butter);
        REQUIRE(filtered_cheby1);
        REQUIRE(filtered_cheby2);
        REQUIRE(filtered_rbj);

        // All should have same number of samples
        REQUIRE(filtered_butter->getNumSamples() == num_samples);
        REQUIRE(filtered_cheby1->getNumSamples() == num_samples);
        REQUIRE(filtered_cheby2->getNumSamples() == num_samples);
        REQUIRE(filtered_rbj->getNumSamples() == num_samples);
    }
}