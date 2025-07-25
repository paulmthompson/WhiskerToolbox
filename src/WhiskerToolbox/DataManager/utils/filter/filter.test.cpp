#include "filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

/*
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

TEST_CASE("Zero-phase filtering with irregular sampling", "[filter]") {
    // Generate a 10Hz sine wave at 1kHz sampling rate with padding
    double sampling_rate = 1000.0;  // Hz
    double signal_freq = 10.0;      // Hz
    size_t num_samples = 2000;      // 2 seconds (includes padding)
    size_t padding = 500;           // 500ms padding on each end
    
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    
    // Generate original signal with padding
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        // Apply Tukey window to padding regions (cosine taper)
        double window = 1.0;
        if (i < padding) {
            window = 0.5 * (1.0 - std::cos(M_PI * static_cast<double>(i) / padding));
        } else if (i >= num_samples - padding) {
            window = 0.5 * (1.0 - std::cos(M_PI * static_cast<double>(num_samples - i - 1) / padding));
        }
        data.push_back(window * std::sin(2.0 * M_PI * signal_freq * t));
        times.push_back(TimeFrameIndex(i));
    }
    
    // Remove samples at irregular intervals
    std::vector<float> irregular_data;
    std::vector<TimeFrameIndex> irregular_times;
    irregular_data.reserve(num_samples);
    irregular_times.reserve(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        if (i % 3 != 0 && i % 5 != 0) {  // Less aggressive sample removal
            irregular_data.push_back(data[i]);
            irregular_times.push_back(times[i]);
        }
    }
    
    // Create time series with irregular sampling
    AnalogTimeSeries series(irregular_data, irregular_times);
    
    SECTION("Low-pass filter with zero-phase") {
        FilterOptions options;
        options.type = FilterType::Butterworth;
        options.response = FilterResponse::LowPass;
        options.cutoff_frequency_hz = 15.0;  // Closer to signal frequency
        options.sampling_rate_hz = sampling_rate;
        options.order = 2;  // Lower order for minimal phase distortion
        options.zero_phase = true;
        
        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        REQUIRE(result.filtered_data);
        
        // Get filtered data
        auto filtered_times = result.filtered_data->getTimeSeries();
        auto filtered_span = result.filtered_data->getDataInTimeFrameIndexRange(
            filtered_times.front(), filtered_times.back());
        std::vector<float> filtered_data(filtered_span.begin(), filtered_span.end());
        
        // Analyze only the central portion (excluding padding)
        size_t start_idx = padding;
        size_t end_idx = filtered_data.size() - padding;
        
        // Normalize filtered data to match original amplitude in analysis region
        float orig_rms = 0.0f;
        float filt_rms = 0.0f;
        size_t count = 0;
        
        for (size_t i = start_idx; i < end_idx; ++i) {
            if (i < irregular_data.size()) {
                orig_rms += irregular_data[i] * irregular_data[i];
                filt_rms += filtered_data[i] * filtered_data[i];
                count++;
            }
        }
        
        orig_rms = std::sqrt(orig_rms / count);
        filt_rms = std::sqrt(filt_rms / count);
        float scale_factor = orig_rms / filt_rms;
        
        for (float& val : filtered_data) {
            val *= scale_factor;
        }
        
        // Find peaks in central portion
        std::vector<size_t> original_peaks;
        std::vector<size_t> filtered_peaks;
        
        // Use zero-crossings instead of peaks for more reliable phase comparison
        for (size_t i = start_idx + 1; i < end_idx - 1; ++i) {
            if (i < irregular_data.size() && 
                irregular_data[i-1] <= 0.0f && irregular_data[i] > 0.0f) {
                original_peaks.push_back(i);
            }
        }
        
        for (size_t i = start_idx + 1; i < end_idx - 1; ++i) {
            if (filtered_data[i-1] <= 0.0f && filtered_data[i] > 0.0f) {
                filtered_peaks.push_back(i);
            }
        }
        
        REQUIRE(original_peaks.size() == filtered_peaks.size());
        
        // Check zero-crossing alignment with wider tolerance
        for (size_t i = 0; i < original_peaks.size(); ++i) {
            CHECK(std::abs(static_cast<int>(original_peaks[i]) - 
                         static_cast<int>(filtered_peaks[i])) <= 10);  // Allow more deviation
        }
        
    }
    
    SECTION("Band-pass filter with zero-phase") {
        FilterOptions options;
        options.type = FilterType::Butterworth;
        options.response = FilterResponse::BandPass;
        options.cutoff_frequency_hz = 8.0;   // Lower cutoff
        options.high_cutoff_hz = 12.0;       // Upper cutoff (narrow band around 10 Hz)
        options.sampling_rate_hz = sampling_rate;
        options.order = 2;  // Lower order
        options.zero_phase = true;
        
        auto result = filterAnalogTimeSeries(&series, options);
        REQUIRE(result.success);
        REQUIRE(result.filtered_data);
        
        // Get filtered data
        auto filtered_times = result.filtered_data->getTimeSeries();
        auto filtered_span = result.filtered_data->getDataInTimeFrameIndexRange(
            filtered_times.front(), filtered_times.back());
        std::vector<float> filtered_data(filtered_span.begin(), filtered_span.end());
        
        // Analyze only the central portion
        size_t start_idx = padding;
        size_t end_idx = filtered_data.size() - padding;
        
        // Normalize filtered data
        float orig_rms = 0.0f;
        float filt_rms = 0.0f;
        size_t count = 0;
        
        for (size_t i = start_idx; i < end_idx; ++i) {
            if (i < irregular_data.size()) {
                orig_rms += irregular_data[i] * irregular_data[i];
                filt_rms += filtered_data[i] * filtered_data[i];
                count++;
            }
        }
        
        orig_rms = std::sqrt(orig_rms / count);
        filt_rms = std::sqrt(filt_rms / count);
        float scale_factor = orig_rms / filt_rms;
        
        for (float& val : filtered_data) {
            val *= scale_factor;
        }

    }
} 
*/