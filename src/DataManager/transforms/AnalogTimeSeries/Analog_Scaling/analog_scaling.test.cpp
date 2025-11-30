#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/AnalogTimeSeries/Analog_Scaling/analog_scaling.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include "fixtures/AnalogScalingTestFixture.hpp"

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function
#include <cmath> // std::abs

// Using Catch::Matchers::Equals for vectors of floats.

TEST_CASE_METHOD(AnalogScalingTestFixture, "Data Transform: Scale and Normalize - Happy Path", "[transforms][analog_scaling]") {
    std::shared_ptr<AnalogTimeSeries> result_scaled;
    AnalogScalingParams params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("FixedGain scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::FixedGain;
        params.gain_factor = 2.5;

        result_scaled = scale_analog_time_series(ats.get(), params);
        std::vector<float> expected_values = {2.5f, 5.0f, 7.5f, 10.0f, 12.5f};
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));

        result_scaled = scale_analog_time_series(ats.get(), params);
        auto scaled_span_2 = result_scaled->getAnalogTimeSeries();
        REQUIRE_THAT(std::vector<float>(scaled_span_2.begin(), scaled_span_2.end()), Catch::Matchers::Equals(expected_values));
    }

    SECTION("ZScore scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::ZScore;

        result_scaled = scale_analog_time_series(ats.get(), params);
        // Mean = 3.0, Population Std = sqrt(2.0) ≈ 1.414
        // Z-scores: (1-3)/1.414 ≈ -1.414, (2-3)/1.414 ≈ -0.707, etc.
        auto result_values = result_scaled->getAnalogTimeSeries();
        REQUIRE(result_values.size() == 5);
        REQUIRE(std::abs(result_values[0] - (-1.414f)) < 0.01f);
        REQUIRE(std::abs(result_values[1] - (-0.707f)) < 0.01f);
        REQUIRE(std::abs(result_values[2] - 0.0f) < 0.01f);
        REQUIRE(std::abs(result_values[3] - 0.707f) < 0.01f);
        REQUIRE(std::abs(result_values[4] - 1.414f) < 0.01f);
    }

    SECTION("MinMax scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::MinMax;
        params.min_target = 0.0;
        params.max_target = 1.0;

        result_scaled = scale_analog_time_series(ats.get(), params);
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        std::vector<float> expected_values = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));
    }

    SECTION("Centering scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::Centering;

        result_scaled = scale_analog_time_series(ats.get(), params);
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        std::vector<float> expected_values = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));
    }

    SECTION("UnitVariance scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::UnitVariance;

        result_scaled = scale_analog_time_series(ats.get(), params);
        // Population Std = sqrt(2.0) ≈ 1.414
        // Unit variance: 1/1.414 ≈ 0.707, 2/1.414 ≈ 1.414, etc.
        auto result_values = result_scaled->getAnalogTimeSeries();
        REQUIRE(result_values.size() == 5);
        REQUIRE(std::abs(result_values[0] - 0.707f) < 0.01f);
        REQUIRE(std::abs(result_values[1] - 1.414f) < 0.01f);
        REQUIRE(std::abs(result_values[2] - 2.121f) < 0.01f);
        REQUIRE(std::abs(result_values[3] - 2.828f) < 0.01f);
        REQUIRE(std::abs(result_values[4] - 3.535f) < 0.01f);
    }

    SECTION("StandardDeviation scaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::StandardDeviation;
        params.std_dev_target = 2.0;

        result_scaled = scale_analog_time_series(ats.get(), params);
        // Scale so 2 std devs = 1.0
        // Population Std = sqrt(2.0) ≈ 1.414
        // Scale factor = 1.0 / (2.0 * 1.414) ≈ 0.354
        auto result_values = result_scaled->getAnalogTimeSeries();
        REQUIRE(result_values.size() == 5);
        REQUIRE(std::abs(result_values[0] - (-0.707f)) < 0.01f);
        REQUIRE(std::abs(result_values[1] - (-0.354f)) < 0.01f);
        REQUIRE(std::abs(result_values[2] - 0.0f) < 0.01f);
        REQUIRE(std::abs(result_values[3] - 0.354f) < 0.01f);
        REQUIRE(std::abs(result_values[4] - 0.707f) < 0.01f);
    }

    SECTION("RobustScaling") {
        auto ats = m_test_signals["standard_signal"];
        params.method = ScalingMethod::RobustScaling;

        result_scaled = scale_analog_time_series(ats.get(), params);
        // Median = 3.0, Q1 = 2.0, Q3 = 4.0, IQR = 2.0
        // Robust scaling: (x - median) / IQR
        std::vector<float> expected_values = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));
    }
}

TEST_CASE_METHOD(AnalogScalingTestFixture, "Data Transform: Scale and Normalize - Error and Edge Cases", "[transforms][analog_scaling]") {
    std::shared_ptr<AnalogTimeSeries> ats;
    std::shared_ptr<AnalogTimeSeries> result_scaled;
    AnalogScalingParams params;

    SECTION("Null input AnalogTimeSeries") {
        ats = nullptr; // Deliberately null
        params.method = ScalingMethod::ZScore;

        result_scaled = scale_analog_time_series(ats.get(), params);
        REQUIRE(result_scaled == nullptr);
    }

    SECTION("Empty AnalogTimeSeries (no timestamps/values)") {
        ats = m_test_signals["empty_signal"];
        params.method = ScalingMethod::ZScore;

        result_scaled = scale_analog_time_series(ats.get(), params);
        REQUIRE(result_scaled != nullptr);
        REQUIRE(result_scaled->getAnalogTimeSeries().empty());
    }

    SECTION("Constant values (zero std dev)") {
        ats = m_test_signals["constant_values"];
        params.method = ScalingMethod::ZScore;

        result_scaled = scale_analog_time_series(ats.get(), params);
        REQUIRE(result_scaled != nullptr);
        // With zero std dev, values should remain unchanged
        std::vector<float> expected_values = {3.0f, 3.0f, 3.0f, 3.0f, 3.0f};
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));
    }

    SECTION("Negative values") {
        ats = m_test_signals["negative_values"];
        params.method = ScalingMethod::MinMax;
        params.min_target = 0.0;
        params.max_target = 1.0;

        result_scaled = scale_analog_time_series(ats.get(), params);
        std::vector<float> expected_values = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        auto scaled_span = result_scaled->getAnalogTimeSeries();
        REQUIRE_THAT(std::vector<float>(scaled_span.begin(), scaled_span.end()), Catch::Matchers::Equals(expected_values));
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE_METHOD(AnalogScalingTestFixture, "Data Transform: Scale and Normalize - JSON pipeline", "[transforms][analog_scaling][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "scaling_step_1"},
            {"transform_name", "Scale and Normalize"},
            {"input_key", "TestSignal.channel1"},
            {"output_key", "ScaledSignal"},
            {"parameters", {
                {"method", "ZScore"},
                {"gain_factor", 1.0},
                {"std_dev_target", 3.0},
                {"min_target", 0.0},
                {"max_target", 1.0},
                {"quantile_low", 0.25},
                {"quantile_high", 0.75}
            }}
        }}}
    };

    auto dm_ptr = createDataManagerWithTestSignal("TestSignal.channel1");
    DataManager* dm = dm_ptr.get();
    TransformRegistry registry;

    TransformPipeline pipeline(dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto scaled_series = dm->getData<AnalogTimeSeries>("ScaledSignal");
    REQUIRE(scaled_series != nullptr);

    // ZScore scaling: mean = 3.0, Population std = sqrt(2.0) ≈ 1.414
    auto result_values = scaled_series->getAnalogTimeSeries();
    REQUIRE(result_values.size() == 5);
    REQUIRE(std::abs(result_values[0] - (-1.414f)) < 0.01f);
    REQUIRE(std::abs(result_values[1] - (-0.707f)) < 0.01f);
    REQUIRE(std::abs(result_values[2] - 0.0f) < 0.01f);
    REQUIRE(std::abs(result_values[3] - 0.707f) < 0.01f);
    REQUIRE(std::abs(result_values[4] - 1.414f) < 0.01f);
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Scale and Normalize - Parameter Factory", "[transforms][analog_scaling][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<AnalogScalingParams>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"method", "MinMax"},
        {"gain_factor", 2.5},
        {"std_dev_target", 2.0},
        {"min_target", 0.0},
        {"max_target", 10.0},
        {"quantile_low", 0.1},
        {"quantile_high", 0.9}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Scale and Normalize", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<AnalogScalingParams*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->method == ScalingMethod::MinMax);
    REQUIRE(params->gain_factor == 2.5);
    REQUIRE(params->std_dev_target == 2.0);
    REQUIRE(params->min_target == 0.0);
    REQUIRE(params->max_target == 10.0);
    REQUIRE(params->quantile_low == 0.1);
    REQUIRE(params->quantile_high == 0.9);
}

TEST_CASE_METHOD(AnalogScalingTestFixture, "Data Transform: Scale and Normalize - load_data_from_json_config", "[transforms][analog_scaling][json_config]") {
    // Create DataManager with test data using fixture helper
    auto dm_ptr = createDataManagerWithTestSignal("test_signal");
    DataManager* dm = dm_ptr.get();
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Scaling Pipeline\",\n"
        "            \"description\": \"Test scaling and normalization on analog signal\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Scale and Normalize\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"scaled_signal\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"ZScore\",\n"
        "                    \"gain_factor\": 1.0,\n"
        "                    \"std_dev_target\": 3.0,\n"
        "                    \"min_target\": 0.0,\n"
        "                    \"max_target\": 1.0,\n"
        "                    \"quantile_low\": 0.25,\n"
        "                    \"quantile_high\": 0.75\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "analog_scaling_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_signal = dm->getData<AnalogTimeSeries>("scaled_signal");
    REQUIRE(result_signal != nullptr);
    
    // Verify the ZScore scaling results
    // Mean = 3.0, Population std = sqrt(2.0) ≈ 1.414
    auto result_values = result_signal->getAnalogTimeSeries();
    REQUIRE(result_values.size() == 5);
    REQUIRE(std::abs(result_values[0] - (-1.414f)) < 0.01f);
    REQUIRE(std::abs(result_values[1] - (-0.707f)) < 0.01f);
    REQUIRE(std::abs(result_values[2] - 0.0f) < 0.01f);
    REQUIRE(std::abs(result_values[3] - 0.707f) < 0.01f);
    REQUIRE(std::abs(result_values[4] - 1.414f) < 0.01f);
    
    // Test another pipeline with different parameters (MinMax scaling)
    const char* json_config_minmax = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"MinMax Scaling Pipeline\",\n"
        "            \"description\": \"Test MinMax scaling on analog signal\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Scale and Normalize\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"scaled_signal_minmax\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"MinMax\",\n"
        "                    \"gain_factor\": 1.0,\n"
        "                    \"std_dev_target\": 3.0,\n"
        "                    \"min_target\": 0.0,\n"
        "                    \"max_target\": 1.0,\n"
        "                    \"quantile_low\": 0.25,\n"
        "                    \"quantile_high\": 0.75\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_minmax = test_dir / "pipeline_config_minmax.json";
    {
        std::ofstream json_file(json_filepath_minmax);
        REQUIRE(json_file.is_open());
        json_file << json_config_minmax;
        json_file.close();
    }
    
    // Execute the MinMax scaling pipeline
    auto data_info_list_minmax = load_data_from_json_config(dm, json_filepath_minmax.string());
    
    // Verify the MinMax scaling results
    auto result_scaled_minmax = dm->getData<AnalogTimeSeries>("scaled_signal_minmax");
    REQUIRE(result_scaled_minmax != nullptr);
    
    std::vector<float> expected_values_minmax = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    auto scaled_span_minmax = result_scaled_minmax->getAnalogTimeSeries();
    REQUIRE_THAT(std::vector<float>(scaled_span_minmax.begin(), scaled_span_minmax.end()), Catch::Matchers::Equals(expected_values_minmax));
    
    // Test FixedGain scaling
    const char* json_config_fixedgain = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"FixedGain Scaling Pipeline\",\n"
        "            \"description\": \"Test FixedGain scaling on analog signal\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Scale and Normalize\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_signal\",\n"
        "                \"output_key\": \"scaled_signal_fixedgain\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"FixedGain\",\n"
        "                    \"gain_factor\": 2.5,\n"
        "                    \"std_dev_target\": 3.0,\n"
        "                    \"min_target\": 0.0,\n"
        "                    \"max_target\": 1.0,\n"
        "                    \"quantile_low\": 0.25,\n"
        "                    \"quantile_high\": 0.75\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_fixedgain = test_dir / "pipeline_config_fixedgain.json";
    {
        std::ofstream json_file(json_filepath_fixedgain);
        REQUIRE(json_file.is_open());
        json_file << json_config_fixedgain;
        json_file.close();
    }
    
    // Execute the FixedGain scaling pipeline
    auto data_info_list_fixedgain = load_data_from_json_config(dm, json_filepath_fixedgain.string());
    
    // Verify the FixedGain scaling results
    auto result_scaled_fixedgain = dm->getData<AnalogTimeSeries>("scaled_signal_fixedgain");
    REQUIRE(result_scaled_fixedgain != nullptr);
    
    std::vector<float> expected_values_fixedgain = {2.5f, 5.0f, 7.5f, 10.0f, 12.5f};
    auto scaled_span_fixedgain = result_scaled_fixedgain->getAnalogTimeSeries();
    REQUIRE_THAT(std::vector<float>(scaled_span_fixedgain.begin(), scaled_span_fixedgain.end()), Catch::Matchers::Equals(expected_values_fixedgain));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
