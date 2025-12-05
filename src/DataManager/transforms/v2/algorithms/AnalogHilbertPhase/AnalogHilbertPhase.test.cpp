#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fixtures/AnalogHilbertPhaseTestFixture.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration
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
// Tests: Algorithm Correctness (using fixture)
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
        params.discontinuity_threshold = 100;  // Should split at gap of 2000-3=1997
        
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
        
        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }
    
    SECTION("Multiple discontinuities") {
        auto ats = m_test_analog_signals["multiple_discontinuities"];
        params.discontinuity_threshold = 100;  // Should create 3 chunks
        
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
    
    SECTION("Default parameters") {
        auto ats = m_test_analog_signals["default_params_signal"];
        
        AnalogHilbertPhaseParams default_params;
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, default_params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Amplitude extraction - simple sine wave") {
        auto ats = m_test_analog_signals["amplitude_sine_2_5"];
        constexpr float amplitude = 2.5f;
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
            if (amp > 0.1f && std::abs(amp - amplitude) < amplitude * 0.2f) {
                count_within_tolerance++;
            }
        }
        // At least 70% of non-zero values should be close to expected amplitude
        REQUIRE(count_within_tolerance > amplitude_values.size() * 0.7);
    }
    
    SECTION("Amplitude extraction - amplitude modulated signal") {
        auto ats = m_test_analog_signals["amplitude_modulated"];
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
        
        // Filter out zeros for this check
        std::vector<float> non_zero_amps;
        for (auto amp : amplitude_values) {
            if (amp > 0.1f) {
                non_zero_amps.push_back(amp);
            }
        }
        
        if (!non_zero_amps.empty()) {
            float min_amp = *std::min_element(non_zero_amps.begin(), non_zero_amps.end());
            float max_amp = *std::max_element(non_zero_amps.begin(), non_zero_amps.end());
            REQUIRE(min_amp < 1.0f);  // Should have values below 1
            REQUIRE(max_amp > 1.0f);  // Should have values above 1
        }
    }
}

TEST_CASE_METHOD(AnalogHilbertPhaseTestFixture,
                 "V2 Container Transform: Analog Hilbert Phase - Edge Cases",
                 "[transforms][v2][container][analog_hilbert_phase]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<AnalogTimeSeries> result_phase;
    AnalogHilbertPhaseParams params;
    ComputeContext ctx;
    
    SECTION("Empty time series") {
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
    
    SECTION("Time series with NaN values") {
        auto ats = m_test_analog_signals["signal_with_nan"];
        
        result_phase = registry.executeContainerTransform<AnalogTimeSeries, AnalogTimeSeries, AnalogHilbertPhaseParams>(
            "AnalogHilbertPhase", *ats, params, ctx);
        
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // NaN values should be handled, so computation should succeed
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(!std::isnan(phase));
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Cancellation support") {
        auto ats = m_test_analog_signals["sine_1hz_200"];
        
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
        REQUIRE(params.getFilterLowFreq() == Catch::Approx(5.0));
        REQUIRE(params.getFilterHighFreq() == Catch::Approx(15.0));
        REQUIRE(params.getFilterOrder() == 4);
        REQUIRE(params.getSamplingRate() == Catch::Approx(1000.0));
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
        original.discontinuity_threshold = 750;
        original.apply_bandpass_filter = true;
        
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
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogHilbertPhase - Registry Integration",
          "[transforms][v2][registry][container]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogHilbertPhase"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogHilbertPhase"));
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

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE_METHOD(AnalogHilbertPhaseTestFixture,
                 "V2 Container Transform: AnalogHilbertPhase - load_data_from_json_config_v2",
                 "[transforms][v2][container][analog_hilbert_phase][json_config]") {
    
    using namespace WhiskerToolbox::Transforms::V2;
    
    // Get DataManager from fixture
    DataManager* dm = getDataManager();
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "analog_hilbert_phase_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Execute V2 pipeline via load_data_from_json_config_v2 - phase extraction") {
        const char* json_config = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Hilbert Phase Pipeline\",\n"
            "            \"description\": \"Test V2 Hilbert phase calculation on analog signal\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogHilbertPhase\",\n"
            "                \"input_key\": \"pipeline_test_signal\",\n"
            "                \"output_key\": \"v2_phase_signal\",\n"
            "                \"parameters\": {\n"
            "                    \"output_type\": \"phase\",\n"
            "                    \"discontinuity_threshold\": 1000\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_hilbert_phase_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        auto result_phase = dm->getData<AnalogTimeSeries>("v2_phase_signal");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Verify the phase calculation results
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Execute V2 pipeline with amplitude extraction") {
        const char* json_config_amplitude = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Hilbert Amplitude Pipeline\",\n"
            "            \"description\": \"Test V2 Hilbert amplitude extraction\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogHilbertPhase\",\n"
            "                \"input_key\": \"amplitude_sine_2_5\",\n"
            "                \"output_key\": \"v2_amplitude_signal\",\n"
            "                \"parameters\": {\n"
            "                    \"output_type\": \"amplitude\",\n"
            "                    \"discontinuity_threshold\": 1000\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_hilbert_amplitude_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_amplitude;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        auto result_amplitude = dm->getData<AnalogTimeSeries>("v2_amplitude_signal");
        REQUIRE(result_amplitude != nullptr);
        REQUIRE(!result_amplitude->getAnalogTimeSeries().empty());
        
        // Amplitude should be non-negative
        auto const& amplitude_values = result_amplitude->getAnalogTimeSeries();
        for (auto const& amp : amplitude_values) {
            REQUIRE(amp >= 0.0f);
        }
        
        // Check that most amplitude values are close to expected amplitude of 2.5
        constexpr float expected_amplitude = 2.5f;
        size_t count_within_tolerance = 0;
        for (auto const& amp : amplitude_values) {
            if (amp > 0.1f && std::abs(amp - expected_amplitude) < expected_amplitude * 0.2f) {
                count_within_tolerance++;
            }
        }
        REQUIRE(count_within_tolerance > amplitude_values.size() * 0.7);
    }
    
    SECTION("Execute V2 pipeline with bandpass filter") {
        const char* json_config_filter = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Hilbert Phase with Filter\",\n"
            "            \"description\": \"Test V2 Hilbert phase with bandpass filtering\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogHilbertPhase\",\n"
            "                \"input_key\": \"pipeline_test_signal\",\n"
            "                \"output_key\": \"v2_phase_filtered\",\n"
            "                \"parameters\": {\n"
            "                    \"output_type\": \"phase\",\n"
            "                    \"apply_bandpass_filter\": true,\n"
            "                    \"filter_low_freq\": 5.0,\n"
            "                    \"filter_high_freq\": 15.0,\n"
            "                    \"filter_order\": 4,\n"
            "                    \"sampling_rate\": 100.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_hilbert_filter_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_filter;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        auto result_phase = dm->getData<AnalogTimeSeries>("v2_phase_filtered");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Verify the phase calculation results
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    SECTION("Execute V2 pipeline with discontinuous signal") {
        const char* json_config_discontinuous = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Hilbert Phase Discontinuous\",\n"
            "            \"description\": \"Test V2 Hilbert phase on discontinuous signal\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogHilbertPhase\",\n"
            "                \"input_key\": \"discontinuous_large_gap\",\n"
            "                \"output_key\": \"v2_phase_discontinuous\",\n"
            "                \"parameters\": {\n"
            "                    \"output_type\": \"phase\",\n"
            "                    \"discontinuity_threshold\": 100\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_hilbert_discontinuous_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_discontinuous;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        auto result_phase = dm->getData<AnalogTimeSeries>("v2_phase_discontinuous");
        REQUIRE(result_phase != nullptr);
        REQUIRE(!result_phase->getAnalogTimeSeries().empty());
        
        // Verify the phase calculation results
        auto const& phase_values = result_phase->getAnalogTimeSeries();
        for (auto const& phase : phase_values) {
            REQUIRE(phase >= -std::numbers::pi_v<float>);
            REQUIRE(phase <= std::numbers::pi_v<float>);
        }
    }
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
