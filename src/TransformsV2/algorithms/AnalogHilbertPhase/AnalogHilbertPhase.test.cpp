#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Builder-based test scenarios
#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/analog/hilbert_scenarios.hpp"

#include <cmath>
#include <numbers>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration
// ============================================================================


namespace {
struct RegisterAnalogHilbertPhase {
    RegisterAnalogHilbertPhase() {
        auto & registry = ElementRegistry::instance();
        registry.registerContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase",
                analogHilbertPhase,
                ContainerTransformMetadata{
                        .description = "Calculate instantaneous phase or amplitude using Hilbert transform",
                        .category = "Signal Processing",
                        .supports_cancellation = true});
    }
};

RegisterAnalogHilbertPhase register_analog_hilbert_phase;
}// namespace

// ============================================================================
// Tests: Algorithm Correctness (using scenarios)
// ============================================================================

TEST_CASE("V2 Container Transform: Analog Hilbert Phase - Happy Path",
          "[transforms][v2][container][analog_hilbert_phase]") {

    auto & registry = ElementRegistry::instance();
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
        auto ats = hilbert_scenarios::sine_1hz_200();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();

        // Check that phase values are in the expected range [-π, π]
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }

        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Cosine wave - phase should be shifted by π/2 from sine") {
        auto ats = hilbert_scenarios::cosine_2hz_100();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();

        // Check that phase values are in the expected range
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Complex signal with multiple frequencies") {
        auto ats = hilbert_scenarios::multi_freq_2_5();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
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
        auto ats = hilbert_scenarios::discontinuous_large_gap();
        params.discontinuity_threshold = 100;// Should split at gap of 2000-3=1997

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        auto times = ats->getTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);

        // Check that phase values are in the expected range
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }

        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Multiple discontinuities") {
        auto ats = hilbert_scenarios::multiple_discontinuities();
        params.discontinuity_threshold = 100;// Should create 3 chunks

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        auto times = ats->getTimeSeries();
        REQUIRE(phase_values.size() == times.back().getValue() + 1);

        // Check that phase values are in the expected range
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Default parameters") {
        auto ats = hilbert_scenarios::default_params_signal();

        AnalogHilbertPhaseParams const default_params;

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, default_params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Amplitude extraction - simple sine wave") {
        auto ats = hilbert_scenarios::amplitude_sine_2_5();
        constexpr float amplitude = 2.5f;
        params.output_type = AnalogHilbertPhaseParams::OutputType::amplitude;

        auto result_amplitude = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());

        auto const & amplitude_values = result_amplitude->getAnalogTimeSeries();

        // Amplitude should be non-negative
        for (auto const & amp: amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }

        // Check that most amplitude values are close to the expected amplitude (within 20% tolerance)
        size_t count_within_tolerance = 0;
        for (auto const & amp: amplitude_values) {
            if (amp > 0.1f && std::abs(amp - amplitude) < amplitude * 0.2f) {
                count_within_tolerance++;
            }
        }
        // At least 70% of non-zero values should be close to expected amplitude
        REQUIRE(count_within_tolerance > amplitude_values.size() * 0.7);
    }

    SECTION("Amplitude extraction - amplitude modulated signal") {
        auto ats = hilbert_scenarios::amplitude_modulated();
        params.output_type = AnalogHilbertPhaseParams::OutputType::amplitude;

        auto result_amplitude = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());

        auto const & amplitude_values = result_amplitude->getAnalogTimeSeries();

        // Amplitude should be non-negative
        for (auto const & amp: amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }

        // Filter out zeros for this check
        std::vector<float> non_zero_amps;
        for (auto amp: amplitude_values) {
            if (amp > 0.1f) {
                non_zero_amps.push_back(amp);
            }
        }

        if (!non_zero_amps.empty()) {
            float const min_amp = *std::min_element(non_zero_amps.begin(), non_zero_amps.end());
            float const max_amp = *std::max_element(non_zero_amps.begin(), non_zero_amps.end());
            REQUIRE(min_amp < 1.0f);// Should have values below 1
            REQUIRE(max_amp > 1.0f);// Should have values above 1
        }
    }
}

TEST_CASE("V2 Container Transform: Analog Hilbert Phase - Edge Cases",
          "[transforms][v2][container][analog_hilbert_phase]") {

    auto & registry = ElementRegistry::instance();
    std::shared_ptr<AnalogTimeSeries> result_phase;
    AnalogHilbertPhaseParams const params;
    ComputeContext ctx;

    SECTION("Empty time series") {
        auto ats = hilbert_scenarios::empty_signal();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }

    SECTION("Single sample") {
        auto ats = hilbert_scenarios::single_sample();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        REQUIRE(phase_values.size() == 1);
        REQUIRE(phase_values[0] >= -std::numbers::pi_v<float>);
        REQUIRE(phase_values[0] <= std::numbers::pi_v<float>);
    }

    SECTION("Time series with NaN values") {
        auto ats = hilbert_scenarios::signal_with_nan();

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        // NaN values should be handled, so computation should succeed
        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase: phase_values) {
            REQUIRE(!std::isnan(phase));
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Cancellation support") {
        auto ats = hilbert_scenarios::sine_1hz_200();

        // Set cancellation flag immediately
        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };

        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
                "AnalogHilbertPhase", *ats, params, ctx);

        // Should return empty due to cancellation
        REQUIRE(result_phase != nullptr);
        REQUIRE(result_phase->getAnalogTimeSeries().empty());
    }
}

// ============================================================================
// Tests: JSON Parameter Rejection
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogHilbertPhaseParams - JSON Rejection",
          "[transforms][v2][params][json]") {

    SECTION("Reject zero discontinuity_threshold") {
        std::string const json = R"({"discontinuity_threshold": 0})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject negative overlap_fraction") {
        std::string const json = R"({"overlap_fraction": -0.1})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject negative filter_low_freq") {
        std::string const json = R"({"filter_low_freq": -1.0})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject zero filter_order") {
        std::string const json = R"({"filter_order": 0})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject unknown output_type") {
        std::string const json = R"({"output_type": "invalid"})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject wrong casing for output_type") {
        std::string const json = R"({"output_type": "Phase"})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }

    SECTION("Reject non-string output_type") {
        std::string const json = R"({"output_type": 1})";
        REQUIRE_FALSE(loadParametersFromJson<AnalogHilbertPhaseParams>(json));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogHilbertPhase - Registry Integration",
          "[transforms][v2][registry][container]") {

    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogHilbertPhase"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogHilbertPhase"));
    }

    SECTION("Can retrieve container metadata") {
        auto const * metadata = registry.getContainerMetadata("AnalogHilbertPhase");
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

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

namespace {
// Helper function to populate DataManager with Hilbert phase test scenarios
// This follows the pattern from the v1 test: create an empty TimeFrame,
// register it with DataManager using setTime(), set it on the signal, then store
// the signal with that TimeKey.
void populateDataManagerWithHilbertScenarios(DataManager & dm) {
    // Create a shared TimeFrame and register it with DataManager
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("hilbert_v2_time"), time_frame);

    // Create separate signals for each key (to avoid duplicate rejection)
    auto pipeline_signal = hilbert_scenarios::pipeline_test_signal();
    pipeline_signal->setTimeFrame(time_frame);
    dm.setData("pipeline_test_signal", pipeline_signal, TimeKey("hilbert_v2_time"));

    auto amplitude_signal = hilbert_scenarios::amplitude_sine_2_5();
    amplitude_signal->setTimeFrame(time_frame);
    dm.setData("amplitude_sine_2_5", amplitude_signal, TimeKey("hilbert_v2_time"));

    auto discontinuous_signal = hilbert_scenarios::discontinuous_large_gap();
    discontinuous_signal->setTimeFrame(time_frame);
    dm.setData("discontinuous_large_gap", discontinuous_signal, TimeKey("hilbert_v2_time"));
}
}// namespace

TEST_CASE("V2 Container Transform: AnalogHilbertPhase - load_data_from_json_config_v2",
          "[transforms][v2][container][analog_hilbert_phase][json_config]") {

    using namespace pipeline_json_test;

    DataManager dm;
    populateDataManagerWithHilbertScenarios(dm);

    SECTION("Execute V2 pipeline via load_data_from_json_config_v2 - phase extraction") {
        AnalogHilbertPhaseParams params;
        params.output_type = AnalogHilbertPhaseParams::OutputType::phase;
        params.discontinuity_threshold = 1000;

        auto const pipeline = makeSingleStepPipeline(
                "AnalogHilbertPhase", "pipeline_test_signal", "v2_phase_signal", params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_phase = dm.getData<AnalogTimeSeries>("v2_phase_signal");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Execute V2 pipeline with amplitude extraction") {
        AnalogHilbertPhaseParams params;
        params.output_type = AnalogHilbertPhaseParams::OutputType::amplitude;
        params.discontinuity_threshold = 1000;

        auto const pipeline = makeSingleStepPipeline(
                "AnalogHilbertPhase", "amplitude_sine_2_5", "v2_amplitude_signal", params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_amplitude = dm.getData<AnalogTimeSeries>("v2_amplitude_signal");
        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());

        auto const & amplitude_values = result_amplitude->getAnalogTimeSeries();
        for (auto const & amp: amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }

        constexpr float expected_amplitude = 2.5f;
        size_t count_within_tolerance = 0;
        for (auto const & amp: amplitude_values) {
            if (amp > 0.1f && std::abs(amp - expected_amplitude) < expected_amplitude * 0.2f) {
                count_within_tolerance++;
            }
        }
        REQUIRE(count_within_tolerance > amplitude_values.size() * 0.7);
    }

    SECTION("Execute V2 pipeline with bandpass filter") {
        AnalogHilbertPhaseParams params;
        params.output_type = AnalogHilbertPhaseParams::OutputType::phase;
        params.apply_bandpass_filter = true;
        params.filter_low_freq = 5.0;
        params.filter_high_freq = 15.0;
        params.filter_order = 4;
        params.sampling_rate = 100.0;

        auto const pipeline = makeSingleStepPipeline(
                "AnalogHilbertPhase", "pipeline_test_signal", "v2_phase_filtered", params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_phase = dm.getData<AnalogTimeSeries>("v2_phase_filtered");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }

    SECTION("Execute V2 pipeline with discontinuous signal") {
        AnalogHilbertPhaseParams params;
        params.output_type = AnalogHilbertPhaseParams::OutputType::phase;
        params.discontinuity_threshold = 100;

        auto const pipeline = makeSingleStepPipeline(
                "AnalogHilbertPhase", "discontinuous_large_gap", "v2_phase_discontinuous", params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_phase = dm.getData<AnalogTimeSeries>("v2_phase_discontinuous");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());

        auto const & phase_values = result_phase->getAnalogTimeSeries();
        for (auto const & phase: phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
}
