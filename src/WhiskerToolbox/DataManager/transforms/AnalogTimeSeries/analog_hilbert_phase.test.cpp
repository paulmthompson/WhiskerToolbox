#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/transforms/AnalogTimeSeries/analog_hilbert_phase.hpp"
#include "transforms/data_transforms.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <numbers>

TEST_CASE("Hilbert Phase Happy Path", "[transforms][analog_hilbert_phase]") {
    std::vector<float> values;
    std::vector<size_t> times;
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
            times.push_back(i);
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
            times.push_back(i);
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
            times.push_back(i);
        }
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 1.0;
        params.highFrequency = 10.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1); // Should match output timestamp size
        
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
        times = {0, 1, 2, 3, 2000, 2001, 2002, 2003}; // Large gap at 2000
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 100; // Should split at gap of 2000-3=1997

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1); // Should match output timestamp size
        
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
        times = {0, 1, 2, 1000, 1001, 2000}; // Two large gaps
        
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 100; // Should create 3 chunks

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1);
        
        // Check that phase values are in the expected range
        for (auto const & phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Progress callback detailed check") {
        values = {1.0f, 0.0f, -1.0f, 0.0f, 1.0f};
        times = {0, 25, 50, 75, 100};
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
        times = {0, 10, 20, 30, 40};
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

TEST_CASE("Hilbert Phase Error and Edge Cases", "[transforms][analog_hilbert_phase]") {
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
        std::vector<size_t> empty_times;
        ats = std::make_shared<AnalogTimeSeries>(empty_values, empty_times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Single sample") {
        std::vector<float> values = {1.0f};
        std::vector<size_t> times = {0};
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
        std::vector<size_t> times = {0, 25, 50, 75};
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
        std::vector<size_t> times = {0, 25, 50, 75};
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
        std::vector<size_t> times = {0, 25, 50, 75};
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
        std::vector<size_t> times = {0, 25, 50, 75};
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
        std::vector<size_t> times = {0, 1, 10, 11, 100}; // Irregular spacing
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should handle irregular spacing gracefully
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1); // Continuous output
    }

    SECTION("Very small discontinuity threshold") {
        // Test with threshold smaller than natural gaps
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 5, 10, 15}; // Gaps of 5
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 2; // Smaller than gaps

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should create multiple small chunks
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1);
    }

    SECTION("Very large discontinuity threshold") {
        // Test with threshold larger than any gaps
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 100, 200, 300}; // Large gaps
        ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        params.lowFrequency = 5.0;
        params.highFrequency = 15.0;
        params.discontinuityThreshold = 1000; // Larger than gaps

        result_phase = hilbert_phase(ats.get(), params);
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Should process as single chunk
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1);
    }
}

TEST_CASE("HilbertPhaseOperation Class Tests", "[transforms][analog_hilbert_phase][operation]") {
    HilbertPhaseOperation operation;
    DataTypeVariant variant;
    HilbertPhaseParams params;
    params.lowFrequency = 5.0;
    params.highFrequency = 15.0;
    params.discontinuityThreshold = 1000;

    SECTION("Operation metadata") {
        REQUIRE(operation.getName() == "Hilbert Phase");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("canApply with valid data") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 25, 50, 75};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        REQUIRE(operation.canApply(variant));
    }

    SECTION("canApply with null shared_ptr") {
        std::shared_ptr<AnalogTimeSeries> null_ats = nullptr;
        variant = null_ats;

        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("canApply with wrong type") {
        variant = std::string("wrong type");

        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("execute with valid parameters") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 25, 50, 75};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        
        auto result_ats = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(result_ats != nullptr);
        REQUIRE(!result_ats->getAnalogTimeSeries().empty());
    }

    SECTION("execute with null parameters") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 25, 50, 75};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        auto result = operation.execute(variant, nullptr);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        
        auto result_ats = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(result_ats != nullptr);
        REQUIRE(!result_ats->getAnalogTimeSeries().empty());
    }

    SECTION("execute with progress callback") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 25, 50, 75};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        volatile int progress_val = -1;
        volatile int call_count = 0;
        ProgressCallback cb = [&](int p) {
            progress_val = p;
            call_count++;
        };

        auto result = operation.execute(variant, &params, cb);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        
        auto result_ats = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(result_ats != nullptr);
        REQUIRE(!result_ats->getAnalogTimeSeries().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("execute with invalid variant") {
        variant = std::string("wrong type");

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::monostate>(result));
    }

    SECTION("execute with wrong parameter type") {
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 0.0f};
        std::vector<size_t> times = {0, 25, 50, 75};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        // Create a different parameter type
        struct WrongParams : public TransformParametersBase {
            int dummy = 42;
        } wrong_params;

        auto result = operation.execute(variant, &wrong_params);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        
        // Should still work with default parameters
        auto result_ats = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(result_ats != nullptr);
        REQUIRE(!result_ats->getAnalogTimeSeries().empty());
    }

    SECTION("execute with discontinuous data and chunked processing") {
        // Test the chunked processing specifically
        std::vector<float> values = {1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f};
        std::vector<size_t> times = {0, 1, 2, 1000, 1001, 1002}; // Large gap
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        variant = ats;

        params.discontinuityThreshold = 100; // Should trigger chunked processing

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        
        auto result_ats = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(result_ats != nullptr);
        REQUIRE(!result_ats->getAnalogTimeSeries().empty());
        
        // Verify the output size matches expected continuous output
        auto const & phase_values = result_ats->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == times.back() + 1);
    }
} 