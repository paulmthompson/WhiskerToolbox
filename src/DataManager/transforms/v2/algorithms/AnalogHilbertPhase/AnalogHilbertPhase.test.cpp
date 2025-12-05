#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fixtures/AnalogHilbertPhaseTestFixture.hpp"

#include <cmath>
#include <numbers>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration (would normally be in RegisteredTransforms.hpp)
// ============================================================================

namespace {
    struct RegisterAnalogHilbertPhase {
        RegisterAnalogHilbertPhase() {
            auto& registry = ElementRegistry::instance();
            registry.registerContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase",
                analogHilbertPhase,
                ContainerTransformMetadata{
                    .description = "Calculate instantaneous phase or amplitude using Hilbert transform",
                    .category = "Signal Processing",
                    .supports_cancellation = true
                });
        }
    };
    
    static RegisterAnalogHilbertPhase register_analog_hilbert_phase;
}

// ============================================================================
// Tests: Algorithm Correctness (using shared fixture)
// ============================================================================

TEST_CASE_METHOD(AnalogHilbertPhaseTestFixture,
                 "V2 Container Transform: Analog Hilbert Phase - Happy Path",
                 "[transforms][v2][container][analog_hilbert_phase]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<AnalogTimeSeries> result_phase;
    AnalogHilbertPhaseParams params;
    ComputeContext ctx;
    
    // Progress tracking
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };
    
    SECTION("Simple sine wave - known phase relationship") {
        auto ats = m_test_analog_signals["sine_1hz_200"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        
        // Check that phase values are in the expected range [-π, π]
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
        
        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }
    
    SECTION("Cosine wave - phase should be shifted by π/2 from sine") {
        auto ats = m_test_analog_signals["cosine_2hz_100"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        
        // Check that phase values are in the expected range
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Complex signal with multiple frequencies") {
        auto ats = m_test_analog_signals["multi_freq_2_5"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        auto times = ats->getTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);
        
        // Verify phase continuity (no large jumps except at wrap-around)
        for (size_t i = 1; i < phase_values.size(); ++i) {
            float phase_diff = std::abs(phase_values[i] - phase_values[i - 1]);
            // Allow for phase wrapping around ±π
            if (phase_diff > std::numbers::pi_v<float>) {
                phase_diff = 2.0f * std::numbers::pi_v<float> - phase_diff;
            }
            REQUIRE(phase_diff < std::numbers::pi_v<float> / 2.0f);
        }
    }
    
    SECTION("Discontinuous time series - chunked processing") {
        auto ats = m_test_analog_signals["discontinuous_large_gap"];
        params.discontinuity_threshold = rfl::Validator<int, rfl::Minimum<1>>(100);
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        auto times = ats->getTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);
        
        // Check that phase values are in the expected range
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Amplitude extraction - simple sine wave") {
        auto ats = m_test_analog_signals["amplitude_sine_2_5"];
        constexpr float expected_amplitude = 2.5f;
        params.output_type = "amplitude";
        
        auto result_amplitude = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());
        
        auto const& amplitude_values = result_amplitude->getAnalogTimeSeries();
        
        // Amplitude should be non-negative
        for (auto const& amp : amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }
        
        // Check that most amplitude values are close to the expected amplitude (within 20% tolerance)
        size_t count_within_tolerance = 0;
        for (auto const& amp : amplitude_values) {
            if (amp > 0.1f && std::abs(amp - expected_amplitude) < expected_amplitude * 0.2f) {
                count_within_tolerance++;
            }
        }
        // At least 70% of non-zero values should be close to expected amplitude
        REQUIRE(count_within_tolerance > amplitude_values.size() * 0.7);
    }
    
    SECTION("Windowed processing - long signal") {
        auto ats = m_test_analog_signals["long_sine_5hz"];
        constexpr float expected_amplitude = 2.0f;
        
        params.output_type = "amplitude";
        params.max_chunk_size = rfl::Validator<int, rfl::Minimum<0>>(10000);
        params.overlap_fraction = rfl::Validator<float, rfl::Minimum<0.0f>>(0.25f);
        params.use_windowing = false;
        
        auto result_amplitude = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());
        
        auto const& amplitude_values = result_amplitude->getAnalogTimeSeries();
        
        // Amplitude should be non-negative
        for (auto const& amp : amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }
        
        // Check that most amplitude values are close to the expected amplitude
        size_t count_within_tolerance = 0;
        size_t count_non_zero = 0;
        for (auto const& amp : amplitude_values) {
            if (amp > 0.1f) {
                count_non_zero++;
                if (std::abs(amp - expected_amplitude) < expected_amplitude * 0.3f) {
                    count_within_tolerance++;
                }
            }
        }
        REQUIRE(count_non_zero > 0);
        REQUIRE(static_cast<double>(count_within_tolerance) / count_non_zero > 0.95);
    }
}

TEST_CASE_METHOD(AnalogHilbertPhaseTestFixture,
                 "V2 Container Transform: Analog Hilbert Phase - Edge Cases",
                 "[transforms][v2][container][analog_hilbert_phase]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<AnalogTimeSeries> result_phase;
    AnalogHilbertPhaseParams params;
    ComputeContext ctx;
    
    SECTION("Empty analog time series") {
        auto ats = m_test_analog_signals["empty_signal"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }
    
    SECTION("Single sample") {
        auto ats = m_test_analog_signals["single_sample"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == 1);
        REQUIRE(phase_values[0] >= -std::numbers::pi_v<float>);
        REQUIRE(phase_values[0] <= std::numbers::pi_v<float>);
    }
    
    SECTION("Cancellation support") {
        auto ats = m_test_analog_signals["sine_1hz_200"];
        
        // Set cancellation flag
        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        // Should return empty due to cancellation
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }
    
    SECTION("Time series with NaN values") {
        auto ats = m_test_analog_signals["signal_with_nan"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // NaN values should be handled, so output should not contain NaN
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(!std::isnan(phase));
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogHilbertPhaseParams - JSON Loading",
          "[transforms][v2][params][json]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "output_type": "amplitude",
            "discontinuity_threshold": 500,
            "max_chunk_size": 50000,
            "overlap_fraction": 0.3,
            "use_windowing": false,
            "apply_bandpass_filter": true,
            "filter_low_freq": 2.0,
            "filter_high_freq": 20.0,
            "filter_order": 6,
            "sampling_rate": 500.0
        })";
        
        auto result = loadParametersFromJson<AnalogHilbertPhaseParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getOutputType() == "amplitude");
        REQUIRE(params.getDiscontinuityThreshold() == 500);
        REQUIRE(params.getMaxChunkSize() == 50000);
        REQUIRE(params.getOverlapFraction() == Catch::Approx(0.3));
        REQUIRE(params.getUseWindowing() == false);
        REQUIRE(params.getApplyBandpassFilter() == true);
        REQUIRE(params.getFilterLowFreq() == Catch::Approx(2.0));
        REQUIRE(params.getFilterHighFreq() == Catch::Approx(20.0));
        REQUIRE(params.getFilterOrder() == 6);
        REQUIRE(params.getSamplingRate() == Catch::Approx(500.0));
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<AnalogHilbertPhaseParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getOutputType() == "phase");
        REQUIRE(params.getDiscontinuityThreshold() == 1000);
        REQUIRE(params.getMaxChunkSize() == 100000);
        REQUIRE(params.getOverlapFraction() == Catch::Approx(0.25));
        REQUIRE(params.getUseWindowing() == true);
        REQUIRE(params.getApplyBandpassFilter() == false);
    }
    
    SECTION("Load with only some fields") {
        std::string json = R"({
            "output_type": "phase",
            "discontinuity_threshold": 200
        })";
        
        auto result = loadParametersFromJson<AnalogHilbertPhaseParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getOutputType() == "phase");
        REQUIRE(params.getDiscontinuityThreshold() == 200);
        REQUIRE(params.getMaxChunkSize() == 100000);  // Default
    }
    
    SECTION("JSON round-trip preserves values") {
        AnalogHilbertPhaseParams original;
        original.output_type = "amplitude";
        original.discontinuity_threshold = rfl::Validator<int, rfl::Minimum<1>>(750);
        original.apply_bandpass_filter = true;
        original.filter_low_freq = rfl::Validator<float, rfl::Minimum<0.0f>>(3.5f);
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<AnalogHilbertPhaseParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.getOutputType() == "amplitude");
        REQUIRE(recovered.getDiscontinuityThreshold() == 750);
        REQUIRE(recovered.getApplyBandpassFilter() == true);
        REQUIRE(recovered.getFilterLowFreq() == Catch::Approx(3.5));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogHilbertPhase Registry Integration",
          "[transforms][v2][registry][container]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogHilbertPhase"));
        REQUIRE_FALSE(registry.hasTransform("AnalogHilbertPhase"));  // Not an element transform
    }
    
    SECTION("Can retrieve container metadata") {
        auto const* metadata = registry.getContainerMetadata("AnalogHilbertPhase");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogHilbertPhase");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->supports_cancellation == true);
    }
    
    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE_FALSE(transforms.empty());
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogHilbertPhase") != transforms.end());
    }
}
