
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include <fmt/core.h>

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"

// Builder-based test fixtures
#include "fixtures/scenarios/analog/threshold_scenarios.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>

// Using Catch::Matchers::Equals for vectors of floats.

TEST_CASE("Data Transform: Analog Event Threshold - Happy Path", "[transforms][analog_event_threshold]") {
    std::shared_ptr<DigitalEventSeries> result_events;
    ThresholdParams params;
    std::vector<TimeFrameIndex> expected_events;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Positive threshold, no lockout") {
        auto ats = analog_scenarios::positive_threshold_no_lockout();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));

        progress_val = -1;
        call_count = 0;
        result_events = event_threshold(ats.get(), params, cb);
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1));
    }

    SECTION("Positive threshold, with lockout") {
        auto ats = analog_scenarios::positive_threshold_with_lockout();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 150.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));

        progress_val = -1;
        call_count = 0;
        result_events = event_threshold(ats.get(), params, cb);
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1));
    }

    SECTION("Negative threshold, no lockout") {
        auto ats = analog_scenarios::negative_threshold_no_lockout();
        params.thresholdValue = -1.0;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Negative threshold, with lockout") {
        auto ats = analog_scenarios::negative_threshold_with_lockout();
        params.thresholdValue = -1.0;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 150.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Absolute threshold, no lockout") {
        auto ats = analog_scenarios::absolute_threshold_no_lockout();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Absolute threshold, with lockout") {
        auto ats = analog_scenarios::absolute_threshold_with_lockout();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        params.lockoutTime = 150.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("No events expected (threshold too high)") {
        auto ats = analog_scenarios::no_events_high_threshold();
        params.thresholdValue = 10.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        REQUIRE(result_events->getEventSeries().empty());
    }

    SECTION("All events expected (threshold very low, no lockout)") {
        auto ats = analog_scenarios::all_events_low_threshold();
        params.thresholdValue = 0.1;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        expected_events = {TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Progress callback detailed check") {
        // Uses same data as positive_threshold_no_lockout
        auto ats = analog_scenarios::positive_threshold_no_lockout();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
            progress_values_seen.push_back(p);
        };

        result_events = event_threshold(ats.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1)); // N calls in loop + 1 final call

        std::vector<int> expected_progress_sequence = {20, 40, 60, 80, 100, 100};
        REQUIRE_THAT(progress_values_seen, Catch::Matchers::Equals(expected_progress_sequence));
    }
}

TEST_CASE("Data Transform: Analog Event Threshold - Error and Edge Cases", "[transforms][analog_event_threshold]") {
    std::shared_ptr<DigitalEventSeries> result_events;
    ThresholdParams params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input AnalogTimeSeries") {
        AnalogTimeSeries* ats = nullptr; // Deliberately null
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats, params);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());

        progress_val = -1;
        call_count = 0;
        result_events = event_threshold(ats, params, cb);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());
        REQUIRE(progress_val == -1); // Free function returns before calling cb for null ats
        REQUIRE(call_count == 0);
    }

    SECTION("Empty AnalogTimeSeries (no timestamps/values)") {
        auto ats = analog_scenarios::empty_signal();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());

        progress_val = -1;
        call_count = 0;
        result_events = event_threshold(ats.get(), params, cb);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1); // Called once with 100
    }

    SECTION("Lockout time larger than series duration or any interval") {
        auto ats = analog_scenarios::lockout_larger_than_duration();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 500.0;

        result_events = event_threshold(ats.get(), params);
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(100)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Events exactly at threshold value") {
        auto ats = analog_scenarios::events_at_threshold();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        result_events = event_threshold(ats.get(), params);
        std::vector<TimeFrameIndex> expected_events_pos = {TimeFrameIndex(300)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events_pos));

        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.thresholdValue = 0.5;
        result_events = event_threshold(ats.get(), params);
        std::vector<TimeFrameIndex> expected_events_neg = {};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events_neg));
    }

    SECTION("Timestamps are zero or start from zero") {
        auto ats = analog_scenarios::zero_based_timestamps();
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 5.0;

        result_events = event_threshold(ats.get(), params);
        std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(0), TimeFrameIndex(20)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }

    SECTION("Unknown threshold direction (should return empty and log error)") {
        auto ats = analog_scenarios::positive_threshold_no_lockout();
        params.thresholdValue = 1.0;
        params.direction = static_cast<ThresholdParams::ThresholdDirection>(99); // Invalid enum

        result_events = event_threshold(ats.get(), params);
        REQUIRE(result_events->getEventSeries().empty());
    }
}


TEST_CASE("Data Transform: Analog Event Threshold - JSON pipeline", "[transforms][analog_event_threshold][json]") {
    // Set up DataManager with scenario data
    DataManager dm;
    auto signal = analog_scenarios::positive_threshold_no_lockout();
    dm.setData("positive_no_lockout", signal, TimeKey("positive_no_lockout_time"));

    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "threshold_step_1"},
            {"transform_name", "Threshold Event Detection"},
            {"input_key", "positive_no_lockout"},
            {"output_key", "DetectedEvents"},
            {"parameters", {
                {"threshold_value", 1.0},
                {"direction", "Positive (Rising)"},
                {"lockout_time", 0.0}
            }}
        }}}
    };

    TransformRegistry registry;

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto event_series = dm.getData<DigitalEventSeries>("DetectedEvents");
    REQUIRE(event_series != nullptr);

    std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
    REQUIRE_THAT(event_series->getEventSeries(), Catch::Matchers::Equals(expected_events));
}

TEST_CASE("Data Transform: Analog Event Threshold - Parameter Factory", "[transforms][analog_event_threshold][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<ThresholdParams>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"threshold_value", 2.5},
        {"direction", "Negative (Falling)"},
        {"lockout_time", 123.45}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Threshold Event Detection", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<ThresholdParams*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->thresholdValue == 2.5);
    REQUIRE(params->direction == ThresholdParams::ThresholdDirection::NEGATIVE);
    REQUIRE(params->lockoutTime == 123.45);
}

TEST_CASE("Data Transform: Analog Event Threshold - load_data_from_json_config", "[transforms][analog_event_threshold][json_config]") {
    // Set up DataManager with scenario data
    DataManager dm;
    auto signal = analog_scenarios::positive_threshold_no_lockout();
    dm.setData("positive_no_lockout", signal, TimeKey("positive_no_lockout_time"));

    const char* json_config_tmpl =
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Threshold Detection Pipeline\",\n"
        "            \"description\": \"Test threshold event detection on analog signal\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Threshold Event Detection\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"{}\",\n"
        "                \"output_key\": \"detected_events\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold_value\": 1.0,\n"
        "                    \"direction\": \"Positive (Rising)\",\n"
        "                    \"lockout_time\": 0.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::string json_config = json_config_tmpl;
    json_config.replace(json_config.find("{}"), 2, "positive_no_lockout");

    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "analog_threshold_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    auto result_events = dm.getData<DigitalEventSeries>("detected_events");
    REQUIRE(result_events != nullptr);
    
    std::vector<TimeFrameIndex> expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
    REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    
    std::filesystem::remove_all(test_dir);
}
