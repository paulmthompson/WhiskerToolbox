
#include "transforms/Masks/Mask_Area/mask_area.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include "fixtures/MaskAreaTestFixture.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

// ============================================================================
// Core Functionality Tests (using fixture)
// ============================================================================

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Empty mask data", 
                 "[mask][area][transform][fixture]") {
    auto mask_data = m_test_masks["empty_mask_data"];
    auto result = area(mask_data.get());

    REQUIRE(result->getAnalogTimeSeries().empty());
    REQUIRE(result->getTimeSeries().empty());
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Single mask at one timestamp", 
                 "[mask][area][transform][fixture]") {
    auto mask_data = m_test_masks["single_mask_single_timestamp"];
    auto result = area(mask_data.get());

    auto const & values = result->getAnalogTimeSeries();
    auto const & times = result->getTimeSeries();

    REQUIRE(times.size() == 1);
    REQUIRE(values.size() == 1);
    REQUIRE(times[0] == TimeFrameIndex(10));
    REQUIRE(values[0] == 3.0f);
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Multiple masks at one timestamp", 
                 "[mask][area][transform][fixture]") {
    auto mask_data = m_test_masks["multiple_masks_single_timestamp"];
    auto result = area(mask_data.get());

    auto const & values = result->getAnalogTimeSeries();
    auto const & times = result->getTimeSeries();

    REQUIRE(times.size() == 1);
    REQUIRE(values.size() == 1);
    REQUIRE(times[0] == TimeFrameIndex(20));
    REQUIRE(values[0] == 8.0f); // 3 + 5 = 8 points total
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Masks across multiple timestamps", 
                 "[mask][area][transform][fixture]") {
    auto mask_data = m_test_masks["masks_multiple_timestamps"];
    auto result = area(mask_data.get());

    auto const & values = result->getAnalogTimeSeries();
    auto const & times = result->getTimeSeries();

    REQUIRE(times.size() == 2);
    REQUIRE(values.size() == 2);

    // Check timestamp 30
    auto time30_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(30)));
    REQUIRE(values[time30_idx] == 2.0f);

    // Check timestamp 40
    auto time40_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(40)));
    REQUIRE(values[time40_idx] == 7.0f); // 3 + 4 = 7 points
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Verify AnalogTimeSeries structure and statistics", 
                 "[mask][area][transform][fixture]") {
    auto mask_data = m_test_masks["single_mask_for_statistics"];
    auto result = area(mask_data.get());

    // Verify it's a proper AnalogTimeSeries
    REQUIRE(result != nullptr);
    REQUIRE(result->getAnalogTimeSeries().size() == 1);
    REQUIRE(result->getTimeSeries().size() == 1);

    // Statistics should work on the result
    REQUIRE(calculate_mean(*result.get()) == 4.0f);
    REQUIRE(calculate_min(*result.get()) == 4.0f);
    REQUIRE(calculate_max(*result.get()) == 4.0f);
}

// ============================================================================
// Edge Cases (using fixture)
// ============================================================================

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Empty mask (zero pixels)", 
                 "[mask][area][transform][edge][fixture]") {
    auto mask_data = m_test_masks["empty_mask_at_timestamp"];
    auto result = area(mask_data.get());

    REQUIRE(result->getAnalogTimeSeries().size() == 1);
    REQUIRE(result->getTimeSeries().size() == 1);
    REQUIRE(result->getAnalogTimeSeries()[0] == 0.0f);
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Mixed empty and non-empty masks", 
                 "[mask][area][transform][edge][fixture]") {
    auto mask_data = m_test_masks["mixed_empty_nonempty"];
    auto result = area(mask_data.get());

    REQUIRE(result->getAnalogTimeSeries().size() == 1);
    REQUIRE(result->getTimeSeries().size() == 1);
    REQUIRE(result->getAnalogTimeSeries()[0] == 3.0f); // 0 + 3 = 3
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Mask area calculation - Large number of masks", 
                 "[mask][area][transform][edge][fixture]") {
    auto mask_data = m_test_masks["large_mask_count"];
    auto result = area(mask_data.get());

    // Sum of 1 + 2 + 3 + ... + 10 = 55
    REQUIRE(result->getAnalogTimeSeries()[0] == 55.0f);
}

// ============================================================================
// JSON Pipeline Tests (using fixture)
// ============================================================================

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Data Transform: Mask Area - JSON pipeline", 
                 "[transforms][mask_area][json][fixture]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "mask_area_step_1"},
            {"transform_name", "Calculate Area"},
            {"input_key", "json_pipeline_basic"},
            {"output_key", "MaskAreas"},
            {"parameters", {}}
        }}}
    };

    auto dm = getDataManager();
    TransformRegistry registry;

    TransformPipeline pipeline(dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto area_series = dm->getData<AnalogTimeSeries>("MaskAreas");
    REQUIRE(area_series != nullptr);

    auto const & values = area_series->getAnalogTimeSeries();
    auto const & times = area_series->getTimeSeries();

    REQUIRE(times.size() == 2);
    REQUIRE(values.size() == 2);
    
    // Check timestamp 100 (3 points)
    auto time100_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(100)));
    REQUIRE(values[time100_idx] == 3.0f);
    
    // Check timestamp 200 (4 points)
    auto time200_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(200)));
    REQUIRE(values[time200_idx] == 4.0f);
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Data Transform: Mask Area - load_data_from_json_config", 
                 "[transforms][mask_area][json_config][fixture]") {
    auto dm = getDataManager();
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Area Calculation Pipeline\",\n"
        "            \"description\": \"Test mask area calculation on mask data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Area\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_multi_timestamp\",\n"
        "                \"output_key\": \"calculated_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_area_pipeline_test";
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
    auto result_areas = dm->getData<AnalogTimeSeries>("calculated_areas");
    REQUIRE(result_areas != nullptr);
    
    // Verify the area calculation results
    auto const & values = result_areas->getAnalogTimeSeries();
    auto const & times = result_areas->getTimeSeries();
    
    REQUIRE(times.size() == 3);
    REQUIRE(values.size() == 3);
    
    // Check timestamp 100 (3 points)
    auto time100_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(100)));
    REQUIRE(values[time100_idx] == 3.0f);
    
    // Check timestamp 200 (5 points)
    auto time200_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(200)));
    REQUIRE(values[time200_idx] == 5.0f);
    
    // Check timestamp 300 (2 points)
    auto time300_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(300)));
    REQUIRE(values[time300_idx] == 2.0f);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Data Transform: Mask Area - Empty mask JSON pipeline", 
                 "[transforms][mask_area][json_config][fixture]") {
    auto dm = getDataManager();
    
    const char* json_config_empty = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Empty Mask Area Calculation\",\n"
        "            \"description\": \"Test mask area calculation on empty mask data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Area\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"empty_mask_data\",\n"
        "                \"output_key\": \"empty_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_area_empty_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_empty = test_dir / "pipeline_config_empty.json";
    {
        std::ofstream json_file(json_filepath_empty);
        REQUIRE(json_file.is_open());
        json_file << json_config_empty;
        json_file.close();
    }
    
    // Execute the empty mask pipeline
    auto data_info_list_empty = load_data_from_json_config(dm, json_filepath_empty.string());
    
    // Verify the empty mask results
    auto result_empty_areas = dm->getData<AnalogTimeSeries>("empty_areas");
    REQUIRE(result_empty_areas != nullptr);
    REQUIRE(result_empty_areas->getAnalogTimeSeries().empty());
    REQUIRE(result_empty_areas->getTimeSeries().empty());
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE_METHOD(MaskAreaTestFixture, 
                 "Data Transform: Mask Area - Multi-mask JSON pipeline", 
                 "[transforms][mask_area][json_config][fixture]") {
    auto dm = getDataManager();
    
    const char* json_config_multi = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Multiple Masks Area Calculation\",\n"
        "            \"description\": \"Test mask area calculation with multiple masks at same timestamp\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Area\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_multi_mask\",\n"
        "                \"output_key\": \"multi_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_area_multi_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_multi = test_dir / "pipeline_config_multi.json";
    {
        std::ofstream json_file(json_filepath_multi);
        REQUIRE(json_file.is_open());
        json_file << json_config_multi;
        json_file.close();
    }
    
    // Execute the multi-mask pipeline
    auto data_info_list_multi = load_data_from_json_config(dm, json_filepath_multi.string());
    
    // Verify the multi-mask results (2 + 3 = 5 points total)
    auto result_multi_areas = dm->getData<AnalogTimeSeries>("multi_areas");
    REQUIRE(result_multi_areas != nullptr);
    
    auto const & multi_values = result_multi_areas->getAnalogTimeSeries();
    auto const & multi_times = result_multi_areas->getTimeSeries();
    
    REQUIRE(multi_times.size() == 1);
    REQUIRE(multi_values.size() == 1);
    REQUIRE(multi_times[0] == TimeFrameIndex(500));
    REQUIRE(multi_values[0] == 5.0f); // 2 + 3 = 5 points
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
