#include "filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

TEST_CASE("FilterOptions validation", "[filter][validation]") {
    SECTION("Valid default options") {
        FilterOptions options;
        REQUIRE(options.isValid());
        CHECK(options.getValidationError().empty());
    }

    SECTION("Invalid filter order - too low") {
        FilterOptions options;
        options.order = 0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Filter order must be between") != std::string::npos);
    }

    SECTION("Invalid filter order - too high") {
        FilterOptions options;
        options.order = max_filter_order + 1;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Filter order must be between") != std::string::npos);
    }

    SECTION("Invalid sampling rate - negative") {
        FilterOptions options;
        options.sampling_rate_hz = -100.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Sampling rate must be positive") != std::string::npos);
    }

    SECTION("Invalid sampling rate - zero") {
        FilterOptions options;
        options.sampling_rate_hz = 0.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Sampling rate must be positive") != std::string::npos);
    }

    SECTION("Invalid cutoff frequency - negative") {
        FilterOptions options;
        options.cutoff_frequency_hz = -50.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Cutoff frequency must be positive") != std::string::npos);
    }

    SECTION("Invalid cutoff frequency - exceeds Nyquist") {
        FilterOptions options;
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 600.0;// > Nyquist (500 Hz)
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Nyquist frequency") != std::string::npos);
    }

    SECTION("Invalid bandpass - high cutoff <= low cutoff") {
        FilterOptions options;
        options.response = FilterResponse::BandPass;
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 200.0;
        options.high_cutoff_hz = 150.0;// <= low cutoff
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("High cutoff frequency must be greater") != std::string::npos);
    }

    SECTION("Invalid Chebyshev I ripple") {
        FilterOptions options;
        options.type = FilterType::ChebyshevI;
        options.passband_ripple_db = -1.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Chebyshev I passband ripple") != std::string::npos);
    }

    SECTION("Invalid Chebyshev II ripple") {
        FilterOptions options;
        options.type = FilterType::ChebyshevII;
        options.stopband_ripple_db = -1.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("Chebyshev II stopband ripple") != std::string::npos);
    }

    SECTION("Invalid RBJ Q factor") {
        FilterOptions options;
        options.type = FilterType::RBJ;
        options.q_factor = -1.0;
        REQUIRE_FALSE(options.isValid());
        CHECK(options.getValidationError().find("RBJ Q factor") != std::string::npos);
    }
}

TEST_CASE("FilterDefaults factory functions", "[filter][defaults]") {
    SECTION("Lowpass filter defaults") {
        auto options = FilterDefaults::lowpass(100.0, 1000.0, 4);
        REQUIRE(options.isValid());
        CHECK(options.type == FilterType::Butterworth);
        CHECK(options.response == FilterResponse::LowPass);
        CHECK(options.order == 4);
        CHECK(options.cutoff_frequency_hz == 100.0);
        CHECK(options.sampling_rate_hz == 1000.0);
    }

    SECTION("Highpass filter defaults") {
        auto options = FilterDefaults::highpass(50.0, 1000.0, 6);
        REQUIRE(options.isValid());
        CHECK(options.type == FilterType::Butterworth);
        CHECK(options.response == FilterResponse::HighPass);
        CHECK(options.order == 6);
        CHECK(options.cutoff_frequency_hz == 50.0);
        CHECK(options.sampling_rate_hz == 1000.0);
    }

    SECTION("Bandpass filter defaults") {
        auto options = FilterDefaults::bandpass(50.0, 150.0, 1000.0, 4);
        REQUIRE(options.isValid());
        CHECK(options.type == FilterType::Butterworth);
        CHECK(options.response == FilterResponse::BandPass);
        CHECK(options.order == 4);
        CHECK(options.cutoff_frequency_hz == 50.0);
        CHECK(options.high_cutoff_hz == 150.0);
        CHECK(options.sampling_rate_hz == 1000.0);
    }

    SECTION("Notch filter defaults") {
        auto options = FilterDefaults::notch(60.0, 1000.0, 10.0);
        REQUIRE(options.isValid());
        CHECK(options.type == FilterType::RBJ);
        CHECK(options.response == FilterResponse::BandStop);
        CHECK(options.order == 2);// RBJ filters are always 2nd order
        CHECK(options.cutoff_frequency_hz == 60.0);
        CHECK(options.q_factor == 10.0);
        CHECK(options.sampling_rate_hz == 1000.0);
    }
}

TEST_CASE("Sampling rate estimation", "[filter][sampling_rate]") {
    SECTION("Regular sampling with known rate") {
        size_t const num_samples = 100;
        double const expected_rate = 1000.0;

        std::vector<float> data(num_samples, 1.0f);
        std::vector<TimeFrameIndex> times;
        times.reserve(num_samples);

        // Create regularly spaced time indices
        for (size_t i = 0; i < num_samples; ++i) {
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
        }

        AnalogTimeSeries series(std::move(data), std::move(times));
        double estimated_rate = estimateSamplingRate(&series);

        // Since time indices are spaced by 1, estimated rate should be 1.0
        CHECK(estimated_rate == 1.0);
    }

    SECTION("Empty time series") {
        AnalogTimeSeries empty_series;
        double estimated_rate = estimateSamplingRate(&empty_series);
        CHECK(estimated_rate == 0.0);
    }

    SECTION("Single sample") {
        std::vector<float> data = {1.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0)};

        AnalogTimeSeries series(std::move(data), std::move(times));
        double estimated_rate = estimateSamplingRate(&series);
        CHECK(estimated_rate == 0.0);
    }

    SECTION("Null pointer handling") {
        double estimated_rate = estimateSamplingRate(nullptr);
        CHECK(estimated_rate == 0.0);
    }
}

TEST_CASE("Basic filtering functionality", "[filter][basic]") {
    // Create test data: sine wave with noise
    size_t const num_samples = 100;
    double const sampling_rate = 1000.0;
    double const signal_freq = 50.0;
    double const noise_freq = 200.0;
    
    std::vector<float> test_data;
    std::vector<TimeFrameIndex> test_times;
    
    test_data.reserve(num_samples);
    test_times.reserve(num_samples);
    
    // Generate test signal: sine wave + high-frequency noise
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        double signal = std::sin(2.0 * M_PI * signal_freq * t);
        double noise = 0.5 * std::sin(2.0 * M_PI * noise_freq * t);
        
        test_data.push_back(static_cast<float>(signal + noise));
        test_times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    
    auto analog_series = std::make_shared<AnalogTimeSeries>(test_data, test_times);
    
    SECTION("Low-pass filter") {
        auto options = FilterDefaults::lowpass(100.0, sampling_rate, 4);
        auto result = filterAnalogTimeSeries(analog_series.get(), options);

        REQUIRE(result.success);
        CHECK(result.error_message.empty());
        CHECK(result.samples_processed == num_samples);
        CHECK(result.segments_processed == 1);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }

    SECTION("High-pass filter") {
        auto options = FilterDefaults::highpass(25.0, sampling_rate, 4);
        auto result = filterAnalogTimeSeries(analog_series.get(), options);

        REQUIRE(result.success);
        CHECK(result.error_message.empty());
        CHECK(result.samples_processed == num_samples);
        CHECK(result.segments_processed == 1);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }

    SECTION("Band-pass filter") {
        auto options = FilterDefaults::bandpass(40.0, 60.0, sampling_rate, 4);
        auto result = filterAnalogTimeSeries(analog_series.get(), options);

        REQUIRE(result.success);
        CHECK(result.error_message.empty());
        CHECK(result.samples_processed == num_samples);
        CHECK(result.segments_processed == 1);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }

    SECTION("Notch filter (RBJ)") {
        auto options = FilterDefaults::notch(200.0, sampling_rate, 10.0);
        auto result = filterAnalogTimeSeries(analog_series.get(), options);

        REQUIRE(result.success);
        CHECK(result.error_message.empty());
        CHECK(result.samples_processed == num_samples);
        CHECK(result.segments_processed == 1);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }

    SECTION("Zero-phase filtering (filtfilt)") {
        auto options = FilterDefaults::lowpass(100.0, sampling_rate, 4);
        options.zero_phase = true;
        auto result = filterAnalogTimeSeries(analog_series.get(), options);

        REQUIRE(result.success);
        CHECK(result.error_message.empty());
        CHECK(result.samples_processed == num_samples);
        CHECK(result.segments_processed == 1);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }
}

TEST_CASE("Filter type variations", "[filter][types]") {
    // Create simple test data
    size_t const num_samples = 50;
    std::vector<float> data(num_samples, 1.0f);
    std::vector<TimeFrameIndex> times;
    times.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    AnalogTimeSeries series(data, times);

    SECTION("Butterworth filter") {
        FilterOptions options;
        options.type = FilterType::Butterworth;
        options.response = FilterResponse::LowPass;
        options.order = 4;
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 100.0;

        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
    }

    SECTION("Chebyshev I filter") {
        FilterOptions options;
        options.type = FilterType::ChebyshevI;
        options.response = FilterResponse::LowPass;
        options.order = 4;
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 100.0;
        options.passband_ripple_db = 1.0;

        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
    }

    SECTION("Chebyshev II filter") {
        FilterOptions options;
        options.type = FilterType::ChebyshevII;
        options.response = FilterResponse::LowPass;
        options.order = 4;
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 100.0;
        options.stopband_ripple_db = 20.0;

        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
    }

    SECTION("RBJ filter") {
        FilterOptions options;
        options.type = FilterType::RBJ;
        options.response = FilterResponse::LowPass;
        options.order = 2;// RBJ is always 2nd order
        options.sampling_rate_hz = 1000.0;
        options.cutoff_frequency_hz = 100.0;
        options.q_factor = 1.0;

        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
    }
}

TEST_CASE("Simple filter test", "[filter][simple]") {
    // Create simple test data
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f, 0.0f, -1.0f, -2.0f, -1.0f, 0.0f};
    std::vector<TimeFrameIndex> times;
    times.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    
    AnalogTimeSeries series(data, times);
    
    SECTION("Simple Butterworth lowpass filter") {
        FilterOptions options;
        options.type = FilterType::Butterworth;
        options.response = FilterResponse::LowPass;
        options.order = 2;
        options.sampling_rate_hz = 1000.0; // Higher sampling rate
        options.cutoff_frequency_hz = 50.0; // Lower relative cutoff (0.1 normalized)
        
        INFO("Sampling rate: " << options.sampling_rate_hz);
        INFO("Cutoff frequency: " << options.cutoff_frequency_hz);
        INFO("Nyquist frequency: " << options.sampling_rate_hz / 2.0);
        INFO("Normalized cutoff for setup(): " << options.cutoff_frequency_hz / options.sampling_rate_hz);
        INFO("Filter order (from template): " << options.order);
        
        INFO("Options valid: " << (options.isValid() ? "true" : "false"));
        if (!options.isValid()) {
            INFO("Validation error: " << options.getValidationError());
        }
        REQUIRE(options.isValid());
        
        FilterResult result;
        try {
            result = filterAnalogTimeSeries(&series, options);
        } catch (const std::exception& e) {
            FAIL("Exception during filtering: " << e.what());
        }
        
        INFO("Result success: " << (result.success ? "true" : "false"));
        INFO("Error message: '" << result.error_message << "'");
        INFO("Samples processed: " << result.samples_processed);
        INFO("Input samples: " << data.size());
        
        REQUIRE(result.success);
        REQUIRE(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == data.size());
    }
}

TEST_CASE("Filter order variations", "[filter][order]") {
    // Create more realistic test data (sine wave)
    size_t const num_samples = 50;
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    
    // Generate a simple sine wave instead of constant values
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / 1000.0; // Assuming 1000 Hz sampling
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * 50.0 * t))); // 50 Hz sine wave
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    
    AnalogTimeSeries series(data, times);

        // Test different filter orders
    for (int order = 1; order <= max_filter_order; ++order) {
        SECTION("Filter order " + std::to_string(order)) {
            FilterOptions options;
            options.type = FilterType::Butterworth;
            options.response = FilterResponse::LowPass;
            options.order = order;
            options.sampling_rate_hz = 1000.0;
            options.cutoff_frequency_hz = 100.0;
            
            // First check if options are valid
            REQUIRE(options.isValid());
            
            // Try-catch to see if there's an exception
            FilterResult result;
            try {
                result = filterAnalogTimeSeries(&series, options);
            } catch (const std::exception& e) {
                FAIL("Exception thrown during filtering for order " << order << ": " << e.what());
            }
            
            // Print diagnostic info regardless of success
            INFO("Filter order: " << order);
            INFO("Result success: " << (result.success ? "true" : "false"));
            INFO("Error message: '" << result.error_message << "'");
            INFO("Samples processed: " << result.samples_processed);
            INFO("Segments processed: " << result.segments_processed);
            INFO("Filtered data ptr: " << (result.filtered_data ? "valid" : "null"));
            
            REQUIRE(result.success);
            CHECK(result.filtered_data != nullptr);
            CHECK(result.filtered_data->getNumSamples() == num_samples);
        }
    }
}

TEST_CASE("Error handling", "[filter][errors]") {
    SECTION("Null AnalogTimeSeries pointer") {
        auto options = FilterDefaults::lowpass(100.0, 1000.0, 4);
        auto result = filterAnalogTimeSeries(nullptr, options);

        REQUIRE_FALSE(result.success);
        CHECK(result.error_message.find("null") != std::string::npos);
        CHECK(result.filtered_data == nullptr);
    }

    SECTION("Invalid filter options") {
        std::vector<float> data = {1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
        AnalogTimeSeries series(std::move(data), std::move(times));

        FilterOptions invalid_options;
        invalid_options.order = 0;// Invalid order

        auto result = filterAnalogTimeSeries(&series, invalid_options);
        REQUIRE_FALSE(result.success);
        CHECK(result.error_message.find("Invalid filter options") != std::string::npos);
    }
}

TEST_CASE("Time range filtering", "[filter][time_range]") {
    // Create test data with specific time indices
    size_t const num_samples = 100;
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;

    data.reserve(num_samples);
    times.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i * 10)));// Spaced by 10
    }

    AnalogTimeSeries series(std::move(data), std::move(times));
    auto options = FilterDefaults::lowpass(100.0, 1000.0, 4);

    SECTION("Filter specific time range") {
        TimeFrameIndex start_time(200);// Should include indices >= 200
        TimeFrameIndex end_time(500);  // Should include indices <= 500

        auto result = filterAnalogTimeSeries(&series, start_time, end_time, options);

        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
        // Should have samples for times 200, 210, 220, ..., 500
        // That's (500-200)/10 + 1 = 31 samples
        CHECK(result.filtered_data->getNumSamples() == 31);
    }

    SECTION("Filter entire series") {
        auto result = filterAnalogTimeSeries(&series, options);

        REQUIRE(result.success);
        CHECK(result.filtered_data != nullptr);
        CHECK(result.filtered_data->getNumSamples() == num_samples);
    }
} 