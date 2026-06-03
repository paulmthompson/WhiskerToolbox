
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include <fmt/core.h>

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"

#include "fixtures/vectors/analog/analog_event_threshold_test_helpers.hpp"
#include "fixtures/vectors/analog/analog_event_threshold_vectors.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

using namespace analog_event_threshold_test;
using namespace analog_event_threshold_vectors;

namespace {

bool isHappyPathCase(std::string_view dm_key) {
    return dm_key != "empty_signal" && dm_key != "lockout_larger_than_duration" &&
           dm_key != "events_at_threshold" && dm_key != "zero_based_timestamps";
}

} // namespace

TEST_CASE("Data Transform: Analog Event Threshold - Happy Path", "[transforms][analog_event_threshold]") {
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    for (auto const& tc : algorithmCases()) {
        if (!isHappyPathCase(tc.dm_key)) {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toThresholdParams(tc);

            auto result_events = event_threshold(ats.get(), params);
            requireEventTimes(*result_events, tc.expected_event_times);

            if (tc.dm_key == "positive_no_lockout") {
                progress_val = -1;
                call_count = 0;
                result_events = event_threshold(ats.get(), params, cb);
                requireEventTimes(*result_events, tc.expected_event_times);
                REQUIRE(progress_val == 100);
                REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1));
            } else if (tc.dm_key == "positive_with_lockout") {
                progress_val = -1;
                call_count = 0;
                result_events = event_threshold(ats.get(), params, cb);
                requireEventTimes(*result_events, tc.expected_event_times);
                REQUIRE(progress_val == 100);
                REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1));
            }
        }
    }

    SECTION("Progress callback detailed check") {
        auto const* tc = findCaseByDmKey("positive_no_lockout");
        REQUIRE(tc != nullptr);
        auto ats = buildAnalogTimeSeries(*tc);
        auto const params = toThresholdParams(*tc);

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
            progress_values_seen.push_back(p);
        };

        auto result_events = event_threshold(ats.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == static_cast<int>(ats->getNumSamples() + 1));

        std::vector<int> expected_progress_sequence = {20, 40, 60, 80, 100, 100};
        REQUIRE_THAT(progress_values_seen, Catch::Matchers::Equals(expected_progress_sequence));
    }
}

TEST_CASE("Data Transform: Analog Event Threshold - Error and Edge Cases",
          "[transforms][analog_event_threshold]") {
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input AnalogTimeSeries") {
        AnalogTimeSeries* ats = nullptr;
        ThresholdParams params;
        params.thresholdValue = 1.0;
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        params.lockoutTime = 0.0;

        auto result_events = event_threshold(ats, params);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->size() == 0);

        progress_val = -1;
        call_count = 0;
        result_events = event_threshold(ats, params, cb);
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->size() == 0);
        REQUIRE(progress_val == -1);
        REQUIRE(call_count == 0);
    }

    for (auto const& tc : algorithmCases()) {
        if (tc.dm_key != "empty_signal" && tc.dm_key != "lockout_larger_than_duration" &&
            tc.dm_key != "events_at_threshold" && tc.dm_key != "zero_based_timestamps") {
            continue;
        }
        DYNAMIC_SECTION(tc.name) {
            auto ats = buildAnalogTimeSeries(tc);
            auto const params = toThresholdParams(tc);
            auto result_events = event_threshold(ats.get(), params);

            if (tc.dm_key == "empty_signal") {
                REQUIRE(result_events != nullptr);
                REQUIRE(result_events->size() == 0);

                progress_val = -1;
                call_count = 0;
                result_events = event_threshold(ats.get(), params, cb);
                REQUIRE(result_events != nullptr);
                REQUIRE(result_events->size() == 0);
                REQUIRE(progress_val == 100);
                REQUIRE(call_count == 1);
            } else {
                requireEventTimes(*result_events, tc.expected_event_times);
            }
        }
    }

    SECTION("Events exactly at threshold value - negative direction") {
        auto const* tc = findCaseByDmKey("events_at_threshold");
        REQUIRE(tc != nullptr);
        auto ats = buildAnalogTimeSeries(*tc);
        ThresholdParams params;
        params.thresholdValue = 0.5;
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        params.lockoutTime = 0.0;

        auto result_events = event_threshold(ats.get(), params);
        std::vector<TimeFrameIndex> expected_events_neg;
        REQUIRE_THAT(result_events->view() | std::views::transform([](auto e) { return e.time(); }),
                     Catch::Matchers::RangeEquals(expected_events_neg));
    }

    SECTION("Unknown threshold direction (should return empty and log error)") {
        auto const* tc = findCaseByDmKey("positive_no_lockout");
        REQUIRE(tc != nullptr);
        auto ats = buildAnalogTimeSeries(*tc);
        ThresholdParams params = toThresholdParams(*tc);
        params.direction = static_cast<ThresholdParams::ThresholdDirection>(99);

        auto result_events = event_threshold(ats.get(), params);
        REQUIRE(result_events->size() == 0);
    }
}

TEST_CASE("Data Transform: Analog Event Threshold - JSON pipeline", "[transforms][analog_event_threshold][json]") {
    DataManager dm;
    auto const* tc = findCaseByDmKey("positive_no_lockout");
    REQUIRE(tc != nullptr);
    dm.setData(std::string(tc->dm_key), buildAnalogTimeSeries(*tc), TimeKey("positive_no_lockout_time"));

    const nlohmann::json json_config = {
            {"steps",
             {{{"step_id", "threshold_step_1"},
               {"transform_name", "Threshold Event Detection"},
               {"input_key", "positive_no_lockout"},
               {"output_key", "DetectedEvents"},
               {"parameters",
                {{"threshold_value", 1.0}, {"direction", "Positive (Rising)"}, {"lockout_time", 0.0}}}}}}};

    TransformRegistry registry;

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    auto event_series = dm.getData<DigitalEventSeries>("DetectedEvents");
    REQUIRE(event_series != nullptr);
    requireEventTimes(*event_series, tc->expected_event_times);
}

TEST_CASE("Data Transform: Analog Event Threshold - Parameter Factory",
          "[transforms][analog_event_threshold][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<ThresholdParams>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {{"threshold_value", 2.5},
                                        {"direction", "Negative (Falling)"},
                                        {"lockout_time", 123.45}};

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Threshold Event Detection", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<ThresholdParams*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->thresholdValue == 2.5);
    REQUIRE(params->direction == ThresholdParams::ThresholdDirection::NEGATIVE);
    REQUIRE(params->lockoutTime == 123.45);
}

TEST_CASE("Data Transform: Analog Event Threshold - load_data_from_json_config",
          "[transforms][analog_event_threshold][json_config]") {
    DataManager dm;
    auto const* tc = findCaseByDmKey("positive_no_lockout");
    REQUIRE(tc != nullptr);
    dm.setData(std::string(tc->dm_key), buildAnalogTimeSeries(*tc), TimeKey("positive_no_lockout_time"));

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

    std::filesystem::path test_dir =
            std::filesystem::temp_directory_path() / "analog_threshold_pipeline_test";
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
    requireEventTimes(*result_events, tc->expected_event_times);

    std::filesystem::remove_all(test_dir);
}
