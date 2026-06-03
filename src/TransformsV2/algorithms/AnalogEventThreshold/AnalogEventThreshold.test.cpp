#include "AnalogEventThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp" //registerContainerTransform
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "fixtures/vectors/analog/analog_event_threshold_test_helpers.hpp"
#include "fixtures/vectors/analog/analog_event_threshold_vectors.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration (would normally be in RegisteredTransforms.hpp)
// ============================================================================

namespace {
    struct RegisterAnalogEventThreshold {
        RegisterAnalogEventThreshold() {
            auto& registry = ElementRegistry::instance();
            registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
                "AnalogEventThreshold",
                analogEventThreshold,
                ContainerTransformMetadata{
                    .description = "Detect threshold crossing events with lockout period",
                    .category = "Signal Processing",
                    .supports_cancellation = true
                });
        }
    };
    
    static RegisterAnalogEventThreshold register_analog_event_threshold;
}

namespace {

using analog_event_threshold_test::buildAnalogTimeSeries;
using analog_event_threshold_test::requireEventTimes;
using analog_event_threshold_vectors::algorithmCases;
using analog_event_threshold_vectors::findCaseByDmKey;
using analog_event_threshold_vectors::Direction;

bool isHappyPathCase(std::string_view dm_key) {
    return dm_key != "empty_signal" && dm_key != "lockout_larger_than_duration" &&
           dm_key != "events_at_threshold" && dm_key != "zero_based_timestamps";
}

AnalogEventThresholdParams toAnalogEventThresholdParams(analog_event_threshold_vectors::Case const& tc) {
    AnalogEventThresholdParams params;
    params.threshold_value = tc.threshold;
    params.lockout_time = tc.lockout;
    switch (tc.direction) {
    case Direction::positive:
        params.direction = AnalogEventThresholdParams::Direction::positive;
        break;
    case Direction::negative:
        params.direction = AnalogEventThresholdParams::Direction::negative;
        break;
    case Direction::absolute:
        params.direction = AnalogEventThresholdParams::Direction::absolute;
        break;
    }
    return params;
}

} // namespace

// ============================================================================
// Tests: Algorithm Correctness (shared I/O vectors)
// ============================================================================

TEST_CASE("V2 Container Transform: Analog Event Threshold - Happy Path",
          "[transforms][v2][container][analog_event_threshold]") {

    auto& registry = ElementRegistry::instance();
    ComputeContext ctx;
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };

    for (auto const& tc : algorithmCases()) {
        if (!isHappyPathCase(tc.dm_key)) {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toAnalogEventThresholdParams(tc);

            auto result_events =
                    registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                      AnalogEventThresholdParams>(
                            "AnalogEventThreshold", *ats, params, ctx);

            requireEventTimes(*result_events, tc.expected_event_times);

            if (tc.dm_key == "positive_no_lockout") {
                REQUIRE(progress_val == 100);
                REQUIRE(call_count > 0);
            }
        }
    }
}

TEST_CASE("V2 Container Transform: Analog Event Threshold - Edge Cases",
          "[transforms][v2][container][analog_event_threshold]") {

    auto& registry = ElementRegistry::instance();
    ComputeContext ctx;

    for (auto const& tc : algorithmCases()) {
        if (tc.dm_key != "empty_signal" && tc.dm_key != "lockout_larger_than_duration" &&
            tc.dm_key != "events_at_threshold" && tc.dm_key != "zero_based_timestamps") {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toAnalogEventThresholdParams(tc);

            auto result_events =
                    registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                      AnalogEventThresholdParams>(
                            "AnalogEventThreshold", *ats, params, ctx);

            REQUIRE(result_events != nullptr);
            if (tc.dm_key == "lockout_larger_than_duration") {
                REQUIRE(result_events->size() == 1);
            } else {
                requireEventTimes(*result_events, tc.expected_event_times);
            }
        }
    }

    SECTION("Cancellation support") {
        auto const* tc = findCaseByDmKey("all_events_low_threshold");
        REQUIRE(tc != nullptr);
        auto ats = buildAnalogTimeSeries(*tc);
        auto const params = toAnalogEventThresholdParams(*tc);

        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };

        auto result_events =
                registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries,
                                                  AnalogEventThresholdParams>(
                        "AnalogEventThreshold", *ats, params, ctx);

        REQUIRE(result_events->view().empty());
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogEventThresholdParams - JSON Loading", 
          "[transforms][v2][params][json]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "threshold_value": 2.5,
            "direction": "negative",
            "lockout_time": 100.0
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.threshold_value == 2.5f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::negative);
        REQUIRE(params.lockout_time.value() == 100.0f);
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.threshold_value == 1.0f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::positive);
        REQUIRE(params.lockout_time.value() == 0.0f);
    }
    
    SECTION("Load with only some fields") {
        std::string json = R"({
            "threshold_value": 3.0,
            "direction": "absolute"
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.threshold_value == 3.0f);
        REQUIRE(params.direction == AnalogEventThresholdParams::Direction::absolute);
        REQUIRE(params.lockout_time.value() == 0.0f);  // Default
    }
    
    SECTION("Reject negative lockout time") {
        std::string json = R"({
            "lockout_time": -10.0
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        // Should fail validation
        REQUIRE_FALSE(result);
    }
    
    SECTION("JSON round-trip preserves values") {
        AnalogEventThresholdParams original;
        original.threshold_value = 1.5f;
        original.direction = AnalogEventThresholdParams::Direction::positive;
        original.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(50.0f);
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.threshold_value == 1.5f);
        REQUIRE(recovered.direction == AnalogEventThresholdParams::Direction::positive);
        REQUIRE(recovered.lockout_time.value() == 50.0f);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: Registry Integration", 
          "[transforms][v2][registry][container]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogEventThreshold"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogEventThreshold"));  // Not an element transform
    }
    
    SECTION("Can retrieve container metadata") {
        auto const* metadata = registry.getContainerMetadata("AnalogEventThreshold");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogEventThreshold");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->supports_cancellation == true);
    }
    
    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE_FALSE(transforms.empty());
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogEventThreshold") != transforms.end());
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

namespace {

void populateDataManagerWithThresholdVectors(DataManager& dm) {
    for (auto const& tc : algorithmCases()) {
        std::string const time_key = std::string(tc.dm_key) + "_time";
        dm.setData(std::string(tc.dm_key), buildAnalogTimeSeries(tc), TimeKey(time_key));
    }
}

} // namespace

TEST_CASE("V2 Container Transform: AnalogEventThreshold - load_data_from_json_config_v2",
                 "[transforms][v2][container][analog_event_threshold][json_config]") {
    
    using namespace WhiskerToolbox::Transforms::V2;
    
    // Set up DataManager with scenario data
    DataManager dm;
    populateDataManagerWithThresholdVectors(dm);
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "analog_event_threshold_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Execute V2 pipeline via load_data_from_json_config_v2 - positive threshold no lockout") {
        // Create JSON config in V1-compatible format (used by load_data_from_json_config_v2)
        const char* json_config = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold Detection Pipeline\",\n"
            "            \"description\": \"Test V2 event threshold detection on analog signal\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"positive_no_lockout\",\n"
            "                \"output_key\": \"v2_detected_events\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": 1.0,\n"
            "                    \"direction\": \"positive\",\n"
            "                    \"lockout_time\": 0.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events");
        REQUIRE(result_events != nullptr);
        
        auto const* tc = findCaseByDmKey("positive_no_lockout");
        REQUIRE(tc != nullptr);
        requireEventTimes(*result_events, tc->expected_event_times);
    }
    
    SECTION("Execute V2 pipeline with lockout") {
        const char* json_config_lockout = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold with Lockout\",\n"
            "            \"description\": \"Test V2 event detection with lockout time\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"positive_with_lockout\",\n"
            "                \"output_key\": \"v2_detected_events_lockout\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": 1.0,\n"
            "                    \"direction\": \"positive\",\n"
            "                    \"lockout_time\": 150.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config_lockout.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_lockout;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events_lockout");
        REQUIRE(result_events != nullptr);
        
        auto const* tc = findCaseByDmKey("positive_with_lockout");
        REQUIRE(tc != nullptr);
        requireEventTimes(*result_events, tc->expected_event_times);
    }
    
    SECTION("Execute V2 pipeline with negative threshold") {
        const char* json_config_negative = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold Negative Direction\",\n"
            "            \"description\": \"Test V2 event detection with negative threshold\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"negative_no_lockout\",\n"
            "                \"output_key\": \"v2_detected_events_negative\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": -1.0,\n"
            "                    \"direction\": \"negative\",\n"
            "                    \"lockout_time\": 0.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config_negative.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_negative;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events_negative");
        REQUIRE(result_events != nullptr);
        
        auto const* tc = findCaseByDmKey("negative_no_lockout");
        REQUIRE(tc != nullptr);
        requireEventTimes(*result_events, tc->expected_event_times);
    }
    
    SECTION("Execute V2 pipeline with absolute threshold") {
        const char* json_config_absolute = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold Absolute Direction\",\n"
            "            \"description\": \"Test V2 event detection with absolute threshold\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"absolute_no_lockout\",\n"
            "                \"output_key\": \"v2_detected_events_absolute\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": 1.0,\n"
            "                    \"direction\": \"absolute\",\n"
            "                    \"lockout_time\": 0.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config_absolute.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_absolute;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events_absolute");
        REQUIRE(result_events != nullptr);
        
        auto const* tc = findCaseByDmKey("absolute_no_lockout");
        REQUIRE(tc != nullptr);
        requireEventTimes(*result_events, tc->expected_event_times);
    }
    
    SECTION("Execute V2 pipeline - no events expected (high threshold)") {
        const char* json_config_high = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold High Threshold\",\n"
            "            \"description\": \"Test V2 event detection with high threshold - no events\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"no_events_high_threshold\",\n"
            "                \"output_key\": \"v2_detected_events_high\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": 10.0,\n"
            "                    \"direction\": \"positive\",\n"
            "                    \"lockout_time\": 0.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config_high.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_high;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events_high");
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->size() == 0);
    }
    
    SECTION("Execute V2 pipeline - empty signal") {
        const char* json_config_empty = 
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"V2 Event Threshold Empty Signal\",\n"
            "            \"description\": \"Test V2 event detection on empty signal\",\n"
            "            \"version\": \"2.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"AnalogEventThreshold\",\n"
            "                \"input_key\": \"empty_signal\",\n"
            "                \"output_key\": \"v2_detected_events_empty\",\n"
            "                \"parameters\": {\n"
            "                    \"threshold_value\": 1.0,\n"
            "                    \"direction\": \"positive\",\n"
            "                    \"lockout_time\": 0.0\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path json_filepath = test_dir / "v2_pipeline_config_empty.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config_empty;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_events = dm.getData<DigitalEventSeries>("v2_detected_events_empty");
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->size() == 0);
    }
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
