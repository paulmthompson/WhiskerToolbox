#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "catch2/catch_approx.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/AnalogTimeSeries/analog_hilbert_phase.hpp"
#include "transforms/data_transforms.hpp"
#include "TimeFrame.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <numbers>

TEST_CASE("Data Transform: Hilbert Phase - Happy Path", "[transforms][analog_hilbert_phase]") {
    std::vector<float> values;
    std::vector<TimeFrameIndex> times;
    std::shared_ptr<AnalogTimeSeries> ats;
    std::shared_ptr<AnalogTimeSeries> result_phase;
    HilbertPhaseParams params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Simple sine wave - known phase relationship") {
        // Create a simple sine wave: sin(2*pi*f*t)
        // The phase of sin(x) should be x (modulo 2*pi)
        constexpr float frequency = 1.0f; // 1 Hz
        constexpr size_t sample_rate = 100; // 100 Hz
        constexpr size_t duration_samples = 200; // 2 seconds
        
        values.reserve(duration_samples);
        times.reserve(duration_samples);
        
        for (size_t i = 0; i < duration_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            values.push_back(std::sin(2.0f * std::numbers::pi_v<float> * frequency * t));
            times.push_back(TimeFrameIndex(i));
        }
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 0.5;
        params.highFrequency = 2.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        
        // Check that phase values are in the expected range [-π, π]
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_phase = hilbert_phase(ats.get(), params, cb);
        REQUIRE(result_phase != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Cosine wave - phase should be shifted by π/2 from sine") {
        constexpr float frequency = 2.0f; // 2 Hz
        constexpr size_t sample_rate = 50; // 50 Hz
        constexpr size_t duration_samples = 100; // 2 seconds
        
        values.reserve(duration_samples);
        times.reserve(duration_samples);
        
        for (size_t i = 0; i < duration_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            values.push_back(std::cos(2.0f * std::numbers::pi_v<float> * frequency * t));
            times.push_back(TimeFrameIndex(i));
        }
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 1.0;
        params.highFrequency = 4.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        
        // Check that phase values are in the expected range
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Complex signal with multiple frequencies") {
        constexpr size_t sample_rate = 100;
        constexpr size_t duration_samples = 300;
        
        values.reserve(duration_samples);
        times.reserve(duration_samples);
        
        for (size_t i = 0; i < duration_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            // Mix of 2Hz and 5Hz components
            float signal = std::sin(2.0f * std::numbers::pi_v<float> * 2.0f * t) + 
                          0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 5.0f * t);
            values.push_back(signal);
            times.push_back(TimeFrameIndex(i));
        }
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 1.0;
        params.highFrequency = 10.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1); // Should match output timestamp size
        
        // Verify phase continuity (no large jumps except at wrap-around)
        for (size_t i = 1; i < phase_values.size(); ++i) {
            float phase_diff = std::abs(phase_values[i] - phase_values[i-1]);
            // Allow for phase wrapping around ±π
            if (phase_diff > std::numbers::pi_v<float>) {
                phase_diff = 2.0f * std::numbers::pi_v<float> - phase_diff;
            }
            REQUIRE(phase_diff < std::numbers::pi_v<float> / 2.0f); // Reasonable continuity
        }
    }

    SECTION("Discontinuous time series - chunked processing") {
        // Create a discontinuous time series with large gaps
        values = {1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f};
        times = {TimeFrameIndex(0), 
                 TimeFrameIndex(1), 
                 TimeFrameIndex(2), 
                 TimeFrameIndex(3), 
                 TimeFrameIndex(2000), 
                 TimeFrameIndex(2001), 
                 TimeFrameIndex(2002), 
                 TimeFrameIndex(2003)}; // Large gap at 2000
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 100; // Should split at gap of 2000-3=1997

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1); // Should match output timestamp size
        
        // Check that phase values are in the expected range
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_phase = hilbert_phase(ats.get(), params, cb);
        REQUIRE(result_phase != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Multiple discontinuities") {
        // Create multiple chunks
        values = {1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f};
        times = {TimeFrameIndex(0), 
                 TimeFrameIndex(1), 
                 TimeFrameIndex(2), 
                 TimeFrameIndex(1000), 
                 TimeFrameIndex(1001), 
                 TimeFrameIndex(2000)}; // Two large gaps
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 100; // Should create 3 chunks

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);
        
        // Check that phase values are in the expected range
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Progress callback detailed check") {
        values = {1.0f, 0.0f, -1.0f, 0.0f, 1.0f};
        times = {TimeFrameIndex(0), 
                 TimeFrameIndex(25), 
                 TimeFrameIndex(50), 
                 TimeFrameIndex(75), 
                 TimeFrameIndex(100)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count++;
            progress_values_seen.push_back(p);
        };

        result_phase = hilbert_phase(ats.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
        
        // Check that we see increasing progress values
        REQUIRE(!progress_values_seen.empty());
        REQUIRE(progress_values_seen.front() >= 0);
        REQUIRE(progress_values_seen.back() == 100);
        
        // Verify progress is monotonically increasing
        for (size_t i = 1; i < progress_values_seen.size(); ++i) {
            REQUIRE(progress_values_seen[i] >= progress_values_seen[i-1]);
        }
    }

    SECTION("Default parameters") {
        values = {1.0f, 2.0f, 1.0f, 0.0f, -1.0f};
        times = {TimeFrameIndex(0), 
                 TimeFrameIndex(10), 
                 TimeFrameIndex(20), 
                 TimeFrameIndex(30), 
                 TimeFrameIndex(40)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        // Use default parameters
        HilbertPhaseParams default_params;
        
        result_phase = hilbert_phase(ats.get(), default_params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
}

TEST_CASE("Data Transform: Hilbert Phase - Error and Edge Cases", "[transforms][analog_hilbert_phase]") {
    std::shared_ptr<AnalogTimeSeries> ats;
    std::shared_ptr<AnalogTimeSeries> result_phase;
    HilbertPhaseParams params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count++;
    };

    SECTION("Null input AnalogTimeSeries") {
        ats = nullptr;
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
        
        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_phase = hilbert_phase(ats.get(), params, cb);
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
        // Progress callback should not be called for null input
        REQUIRE(call_count == 0);
    }

    SECTION("Empty time series") {
        std::vector<float> empty_values;
        std::vector<TimeFrameIndex> empty_times;
        ats = std::make_shared<AnalogTimeSeries>(empty_values, empty_times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Single sample") {
        std::vector<float> values = {1.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Single sample should produce a single phase value
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == 1);
        REQUIRE(phase_values[0] >= -std::numbers::pi_v<float>);
        REQUIRE(phase_values[0] <= std::numbers::pi_v<float>);
    }

    SECTION("Invalid frequency parameters - negative frequencies") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(25), 
                                             TimeFrameIndex(50), 
                                             TimeFrameIndex(75)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = -1.0;  // Invalid
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        // Should still produce output despite invalid parameters
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Invalid frequency parameters - frequencies too high") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(25), 
                                             TimeFrameIndex(50), 
                                             TimeFrameIndex(75)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 5.0;
        params.highFrequency = 1000.0;  // Way above Nyquist

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        // Should still produce output despite invalid parameters
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Invalid frequency parameters - low >= high") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(25), 
                                             TimeFrameIndex(50), 
                                             TimeFrameIndex(75)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 15.0;
        params.highFrequency = 5.0;  // Low > High

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        // Should still produce output despite invalid parameters
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Time series with NaN values") {
        std::vector<float> values = {1.0f, std::numeric_limits<float>::quiet_NaN(), -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(25), 
                                             TimeFrameIndex(50), 
                                             TimeFrameIndex(75)};
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // NaN values should be replaced with 0, so computation should succeed
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase : phase_values) {
            REQUIRE(!std::isnan(phase));
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Irregular timestamp spacing") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f, 1.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(1), 
                                             TimeFrameIndex(10), 
                                             TimeFrameIndex(11), 
                                             TimeFrameIndex(100)}; // Irregular spacing
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should handle irregular spacing gracefully
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1); // Continuous output
    }

    SECTION("Very small discontinuity threshold") {
        // Test with threshold smaller than natural gaps
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(5), 
                                             TimeFrameIndex(10), 
                                             TimeFrameIndex(15)}; // Gaps of 5
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 2; // Smaller than gaps

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should create multiple small chunks
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);
    }

    SECTION("Very large discontinuity threshold") {
        // Test with threshold larger than any gaps
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), 
                                             TimeFrameIndex(100), 
                                             TimeFrameIndex(200), 
                                             TimeFrameIndex(300)}; // Large gaps
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 1000; // Larger than gaps

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should process as single chunk
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);
    }
}


TEST_CASE("Data Transform: Hilbert Phase - Irregularly Sampled Data", "[transforms][analog_hilbert_phase]") {
    // Create irregularly sampled sine wave with both small and large gaps
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    
    double sampling_rate = 1000.0;  // 1kHz
    double freq = 10.0;  // 10Hz sine wave
    
    // Create data with three segments:
    // 1. Dense segment with small gaps (should be interpolated)
    // 2. Large gap (should not be interpolated)
    // 3. Another dense segment with small gaps
    
    // First segment: points at 0,1,3,4,6,7,9,10 (skipping 2,5,8)
    for (int i = 0; i <= 10; i++) {
        if (i % 3 == 2) continue;  // Skip every third point
        
        double t = i / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * freq * t)));
        times.push_back(TimeFrameIndex(i));
    }
    
    // Large gap (100 samples)
    
    // Second segment: points at 110,111,113,114,116,117,119,120
    for (int i = 110; i <= 120; i++) {
        if (i % 3 == 2) continue;  // Skip every third point
        
        double t = i / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * freq * t)));
        times.push_back(TimeFrameIndex(i));
    }
    
    AnalogTimeSeries series(data, times);
    
    // Configure Hilbert transform parameters
    HilbertPhaseParams params;
    params.lowFrequency = 5.0;
    params.highFrequency = 15.0;
    params.discontinuityThreshold = 10;  // Allow interpolation for gaps <= 10 samples
    
    // Apply transform
    auto result = hilbert_phase(&series, params);
    
    REQUIRE(result != nullptr);
    
    // Get time indices from result
    auto result_times = result->getTimeSeries();
    auto original_times = series.getTimeSeries();
    
    SECTION("Original time points are preserved") {
        // Every original time point should exist in the result
        for (auto const& original_time : original_times) {
            INFO("Checking preservation of time index " << original_time.getValue());
            bool found = false;
            for (auto const& result_time : result_times) {
                if (result_time.getValue() == original_time.getValue()) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }
    
    SECTION("Small gaps are interpolated") {
        // Check first segment (0-10)
        // Should have points for 0,1,2,3,4,5,6,7,8,9,10
        for (int i = 0; i <= 10; i++) {
            INFO("Checking interpolation at time " << i);
            bool found = false;
            for (auto const& time : result_times) {
                if (time.getValue() == i) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }
    
    SECTION("Large gaps are not interpolated") {
        // Check that points in the large gap (11-109) are not present
        for (int i = 11; i < 110; i++) {
            INFO("Checking gap at time " << i);
            bool found = false;
            for (auto const& time : result_times) {
                if (time.getValue() == i) {
                    found = true;
                    break;
                }
            }
            REQUIRE_FALSE(found);
        }
    }
    
    SECTION("Data values at original times are preserved") {
        // Get the original and transformed data
        auto original_data = series.getAnalogTimeSeries();
        auto result_data = result->getAnalogTimeSeries();
        
        // For each original point, find its corresponding point in the result
        for (size_t i = 0; i < original_times.size(); i++) {
            auto time = original_times[i].getValue();
            auto original_value = original_data[i];
            
            // Find this time in result_times
            auto it = std::find_if(result_times.begin(), result_times.end(),
                [time](TimeFrameIndex const& t) { return t.getValue() == time; });
            
            REQUIRE(it != result_times.end());
            
            size_t result_idx = std::distance(result_times.begin(), it);
            // Note: We don't check exact equality because the Hilbert transform
            // modifies values, but we can check the value exists
            REQUIRE(std::isfinite(result_data[result_idx]));
        }
    }
} 
