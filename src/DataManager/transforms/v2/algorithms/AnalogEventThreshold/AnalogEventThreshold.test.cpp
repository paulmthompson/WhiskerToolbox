#include "AnalogEventThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp" //registerContainerTransform
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

// Builder-based test fixtures
#include "fixtures/scenarios/analog/threshold_scenarios.hpp"

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

// ============================================================================
// Tests: Algorithm Correctness (using builder-based scenarios)
// ============================================================================

TEST_CASE("V2 Container Transform: Analog Event Threshold - Happy Path", 
                 "[transforms][v2][container][analog_event_threshold]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<DigitalEventSeries> result_events;
    AnalogEventThresholdParams params;
    std::vector<TimeFrameIndex> expected_events;
    ComputeContext ctx;
    
    // Progress tracking
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };
    
    SECTION("Positive threshold, no lockout") {
        auto ats = analog_scenarios::positive_threshold_no_lockout();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
        
        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }
    
    SECTION("Positive threshold, with lockout") {
        auto ats = analog_scenarios::positive_threshold_with_lockout();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Negative threshold, no lockout") {
        auto ats = analog_scenarios::negative_threshold_no_lockout();
        params.threshold_value = -1.0f;
        params.direction = "negative";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Negative threshold, with lockout") {
        auto ats = analog_scenarios::negative_threshold_with_lockout();
        params.threshold_value = -1.0f;
        params.direction = "negative";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Absolute threshold, no lockout") {
        auto ats = analog_scenarios::absolute_threshold_no_lockout();
        params.threshold_value = 1.0f;
        params.direction = "absolute";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Absolute threshold, with lockout") {
        auto ats = analog_scenarios::absolute_threshold_with_lockout();
        params.threshold_value = 1.0f;
        params.direction = "absolute";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("No events expected (threshold too high)") {
        auto ats = analog_scenarios::no_events_high_threshold();
        params.threshold_value = 10.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        REQUIRE(result_events->getEventSeries().empty());
    }
    
    SECTION("All events expected (threshold very low, no lockout)") {
        auto ats = analog_scenarios::all_events_low_threshold();
        params.threshold_value = 0.1f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
                          TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
}

TEST_CASE("V2 Container Transform: Analog Event Threshold - Edge Cases",
                 "[transforms][v2][container][analog_event_threshold]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<DigitalEventSeries> result_events;
    AnalogEventThresholdParams params;
    ComputeContext ctx;
    
    SECTION("Empty analog time series") {
        auto ats = analog_scenarios::empty_signal();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());
    }
    
    SECTION("Lockout time larger than series duration") {
        auto ats = analog_scenarios::lockout_larger_than_duration();
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 1000.0f;  // Much larger than series
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        // Should only get first event
        REQUIRE(result_events->getEventSeries().size() == 1);
    }
    
    SECTION("Cancellation support") {
        auto ats = analog_scenarios::all_events_low_threshold();
        params.threshold_value = 0.1f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        // Set cancellation flag
        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        // Should return empty due to cancellation
        REQUIRE(result_events->getEventSeries().empty());
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
        
        REQUIRE(params.getThresholdValue() == 2.5f);
        REQUIRE(params.getDirection() == "negative");
        REQUIRE(params.getLockoutTime() == 100.0f);
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getThresholdValue() == 1.0f);
        REQUIRE(params.getDirection() == "positive");
        REQUIRE(params.getLockoutTime() == 0.0f);
    }
    
    SECTION("Load with only some fields") {
        std::string json = R"({
            "threshold_value": 3.0,
            "direction": "absolute"
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getThresholdValue() == 3.0f);
        REQUIRE(params.getDirection() == "absolute");
        REQUIRE(params.getLockoutTime() == 0.0f);  // Default
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
        original.direction = "positive";
        original.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(50.0f);
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.getThresholdValue() == 1.5f);
        REQUIRE(recovered.getDirection() == "positive");
        REQUIRE(recovered.getLockoutTime() == 50.0f);
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
    // Helper to set up DataManager with all threshold test scenarios
    void populateDataManagerWithThresholdScenarios(DataManager& dm) {
        dm.setData("positive_no_lockout", analog_scenarios::positive_threshold_no_lockout(), 
                   TimeKey("positive_no_lockout_time"));
        dm.setData("positive_with_lockout", analog_scenarios::positive_threshold_with_lockout(), 
                   TimeKey("positive_with_lockout_time"));
        dm.setData("negative_no_lockout", analog_scenarios::negative_threshold_no_lockout(), 
                   TimeKey("negative_no_lockout_time"));
        dm.setData("negative_with_lockout", analog_scenarios::negative_threshold_with_lockout(), 
                   TimeKey("negative_with_lockout_time"));
        dm.setData("absolute_no_lockout", analog_scenarios::absolute_threshold_no_lockout(), 
                   TimeKey("absolute_no_lockout_time"));
        dm.setData("absolute_with_lockout", analog_scenarios::absolute_threshold_with_lockout(), 
                   TimeKey("absolute_with_lockout_time"));
        dm.setData("no_events_high_threshold", analog_scenarios::no_events_high_threshold(), 
                   TimeKey("no_events_high_threshold_time"));
        dm.setData("all_events_low_threshold", analog_scenarios::all_events_low_threshold(), 
                   TimeKey("all_events_low_threshold_time"));
        dm.setData("empty_signal", analog_scenarios::empty_signal(), 
                   TimeKey("empty_signal_time"));
        dm.setData("lockout_larger_than_duration", analog_scenarios::lockout_larger_than_duration(), 
                   TimeKey("lockout_larger_than_duration_time"));
        dm.setData("events_at_threshold", analog_scenarios::events_at_threshold(), 
                   TimeKey("events_at_threshold_time"));
        dm.setData("zero_based_timestamps", analog_scenarios::zero_based_timestamps(), 
                   TimeKey("zero_based_timestamps_time"));
    }
}

TEST_CASE("V2 Container Transform: AnalogEventThreshold - load_data_from_json_config_v2",
                 "[transforms][v2][container][analog_event_threshold][json_config]") {
    
    using namespace WhiskerToolbox::Transforms::V2;
    
    // Set up DataManager with scenario data
    DataManager dm;
    populateDataManagerWithThresholdScenarios(dm);
    
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
        
        // Verify the event detection results
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
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
        
        // With lockout of 150, event at 300 should be skipped
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
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
        
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
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
        
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
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
        REQUIRE(result_events->getEventSeries().empty());
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
        REQUIRE(result_events->getEventSeries().empty());
    }
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
