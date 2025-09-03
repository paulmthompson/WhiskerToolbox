
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

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Mask area calculation - Core functionality", "[mask][area][transform]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating area from empty mask data") {
        auto result = area(mask_data.get());

        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Calculating area from single mask at one timestamp") {
        // Create a simple mask (3 points)
        std::vector<uint32_t> x_coords = {1, 2, 3};
        std::vector<uint32_t> y_coords = {1, 2, 3};
        mask_data->addAtTime(TimeFrameIndex(10), x_coords, y_coords);

        auto result = area(mask_data.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(10));
        REQUIRE(values[0] == 3.0f); // 3 points in the mask
    }

    SECTION("Calculating area from multiple masks at one timestamp") {
        // First mask (3 points)
        std::vector<uint32_t> x1 = {1, 2, 3};
        std::vector<uint32_t> y1 = {1, 2, 3};
        mask_data->addAtTime(TimeFrameIndex(20), x1, y1);

        // Second mask (5 points)
        std::vector<uint32_t> x2 = {4, 5, 6, 7, 8};
        std::vector<uint32_t> y2 = {4, 5, 6, 7, 8};
        mask_data->addAtTime(TimeFrameIndex(20), x2, y2);

        auto result = area(mask_data.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(20));
        REQUIRE(values[0] == 8.0f); // 3 + 5 = 8 points total
    }

    SECTION("Calculating areas from masks across multiple timestamps") {
        // Timestamp 30: One mask with 2 points
        std::vector<uint32_t> x1 = {1, 2};
        std::vector<uint32_t> y1 = {1, 2};
        mask_data->addAtTime(TimeFrameIndex(30), x1, y1);

        // Timestamp 40: Two masks with 3 and 4 points
        std::vector<uint32_t> x2 = {1, 2, 3};
        std::vector<uint32_t> y2 = {1, 2, 3};
        mask_data->addAtTime(TimeFrameIndex(40), x2, y2);

        std::vector<uint32_t> x3 = {4, 5, 6, 7};
        std::vector<uint32_t> y3 = {4, 5, 6, 7};
        mask_data->addAtTime(TimeFrameIndex(40), x3, y3);

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

    SECTION("Verify returned AnalogTimeSeries structure") {
        // Add some mask data
        std::vector<uint32_t> x = {1, 2, 3, 4};
        std::vector<uint32_t> y = {1, 2, 3, 4};
        mask_data->addAtTime(TimeFrameIndex(100), x, y);

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
}

TEST_CASE("Mask area calculation - Edge cases and error handling", "[mask][area][transform][edge]") {

    auto mask_data = std::make_shared<MaskData>();

    SECTION("Masks with zero points") {
        // Add an empty mask (should be handled gracefully)
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(TimeFrameIndex(10), empty_x, empty_y);

        auto result = area(mask_data.get());

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE(result->getAnalogTimeSeries()[0] == 0.0f);
    }

    SECTION("Mixed empty and non-empty masks") {
        // Add an empty mask
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(TimeFrameIndex(20), empty_x, empty_y);

        // Add a non-empty mask at the same timestamp
        std::vector<uint32_t> x = {1, 2, 3};
        std::vector<uint32_t> y = {1, 2, 3};
        mask_data->addAtTime(TimeFrameIndex(20), x, y);

        auto result = area(mask_data.get());

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE(result->getAnalogTimeSeries()[0] == 3.0f); // Only counts the non-empty mask
    }

    SECTION("Large number of masks and points") {
        // Create a timestamp with many small masks
        for (int i = 0; i < 10; i++) {
            std::vector<uint32_t> x(i+1, 1);
            std::vector<uint32_t> y(i+1, 1);
            mask_data->addAtTime(TimeFrameIndex(30), x, y);
        }

        auto result = area(mask_data.get());

        // Sum of 1 + 2 + 3 + ... + 10 = 55
        REQUIRE(result->getAnalogTimeSeries()[0] == 55.0f);
    }
}

TEST_CASE("Data Transform: Mask Area - JSON pipeline", "[transforms][mask_area][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "mask_area_step_1"},
            {"transform_name", "Calculate Area"},
            {"input_key", "TestMaskData"},
            {"output_key", "MaskAreas"},
            {"parameters", {}}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test mask data
    auto mask_data = std::make_shared<MaskData>();
    
    // Add masks at different timestamps
    std::vector<uint32_t> x1 = {1, 2, 3};
    std::vector<uint32_t> y1 = {1, 2, 3};
    mask_data->addAtTime(TimeFrameIndex(100), x1, y1);

    std::vector<uint32_t> x2 = {4, 5, 6, 7};
    std::vector<uint32_t> y2 = {4, 5, 6, 7};
    mask_data->addAtTime(TimeFrameIndex(200), x2, y2);

    dm.setData("TestMaskData", mask_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto area_series = dm.getData<AnalogTimeSeries>("MaskAreas");
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

TEST_CASE("Data Transform: Mask Area - load_data_from_json_config", "[transforms][mask_area][json_config]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code
    auto test_mask_data = std::make_shared<MaskData>();
    
    // Add multiple masks at different timestamps
    std::vector<uint32_t> x1 = {1, 2, 3};
    std::vector<uint32_t> y1 = {1, 2, 3};
    test_mask_data->addAtTime(TimeFrameIndex(100), x1, y1);

    std::vector<uint32_t> x2 = {4, 5, 6, 7, 8};
    std::vector<uint32_t> y2 = {4, 5, 6, 7, 8};
    test_mask_data->addAtTime(TimeFrameIndex(200), x2, y2);

    std::vector<uint32_t> x3 = {9, 10};
    std::vector<uint32_t> y3 = {9, 10};
    test_mask_data->addAtTime(TimeFrameIndex(300), x3, y3);
    
    // Store the mask data in DataManager with a known key
    dm.setData("test_mask_data", test_mask_data, TimeKey("default"));
    
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
        "                \"input_key\": \"test_mask_data\",\n"
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
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_areas = dm.getData<AnalogTimeSeries>("calculated_areas");
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
    
    // Test with empty mask data
    auto empty_mask_data = std::make_shared<MaskData>();
    dm.setData("empty_mask_data", empty_mask_data, TimeKey("default"));
    
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
    
    std::filesystem::path json_filepath_empty = test_dir / "pipeline_config_empty.json";
    {
        std::ofstream json_file(json_filepath_empty);
        REQUIRE(json_file.is_open());
        json_file << json_config_empty;
        json_file.close();
    }
    
    // Execute the empty mask pipeline
    auto data_info_list_empty = load_data_from_json_config(&dm, json_filepath_empty.string());
    
    // Verify the empty mask results
    auto result_empty_areas = dm.getData<AnalogTimeSeries>("empty_areas");
    REQUIRE(result_empty_areas != nullptr);
    REQUIRE(result_empty_areas->getAnalogTimeSeries().empty());
    REQUIRE(result_empty_areas->getTimeSeries().empty());
    
    // Test with multiple masks at same timestamp
    auto multi_mask_data = std::make_shared<MaskData>();
    
    // Add two masks at the same timestamp
    std::vector<uint32_t> x1_multi = {1, 2};
    std::vector<uint32_t> y1_multi = {1, 2};
    multi_mask_data->addAtTime(TimeFrameIndex(500), x1_multi, y1_multi);
    
    std::vector<uint32_t> x2_multi = {3, 4, 5};
    std::vector<uint32_t> y2_multi = {3, 4, 5};
    multi_mask_data->addAtTime(TimeFrameIndex(500), x2_multi, y2_multi);
    
    dm.setData("multi_mask_data", multi_mask_data, TimeKey("default"));
    
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
        "                \"input_key\": \"multi_mask_data\",\n"
        "                \"output_key\": \"multi_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_multi = test_dir / "pipeline_config_multi.json";
    {
        std::ofstream json_file(json_filepath_multi);
        REQUIRE(json_file.is_open());
        json_file << json_config_multi;
        json_file.close();
    }
    
    // Execute the multi-mask pipeline
    auto data_info_list_multi = load_data_from_json_config(&dm, json_filepath_multi.string());
    
    // Verify the multi-mask results (2 + 3 = 5 points total)
    auto result_multi_areas = dm.getData<AnalogTimeSeries>("multi_areas");
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
