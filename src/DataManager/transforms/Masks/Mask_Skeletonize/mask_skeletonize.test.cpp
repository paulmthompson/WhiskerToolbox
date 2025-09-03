#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "Masks/Mask_Data.hpp"
#include "transforms/Masks/Mask_Skeletonize/mask_skeletonize.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function

// Using Catch::Matchers::Equals for vectors of floats.

TEST_CASE("Data Transform: Mask Skeletonize - Happy Path", "[transforms][mask_skeletonize]") {
    std::shared_ptr<MaskData> mask_data;
    std::shared_ptr<MaskData> result_skeletonized;
    MaskSkeletonizeParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Simple rectangular mask") {
        // Create a simple rectangular mask
        mask_data = std::make_shared<MaskData>();
        
        // Create a 10x10 rectangular mask at time 100 - all points in one mask
        std::vector<uint32_t> x_coords;
        std::vector<uint32_t> y_coords;
        
        // Build all points for a 10x10 rectangle
        for (uint32_t row = 1; row <= 10; ++row) {
            for (uint32_t col = 1; col <= 10; ++col) {
                x_coords.push_back(col);
                y_coords.push_back(row);
            }
        }
        
        mask_data->addAtTime(TimeFrameIndex(100), x_coords, y_coords);

        result_skeletonized = skeletonize_mask(mask_data.get(), &params);
        REQUIRE(result_skeletonized != nullptr);
        
        // Verify that skeletonization produced a result (should be thinner than original)
        auto original_masks = mask_data->getAtTime(TimeFrameIndex(100));
        auto skeletonized_masks = result_skeletonized->getAtTime(TimeFrameIndex(100));
        
        REQUIRE(!original_masks.empty());
        REQUIRE(!skeletonized_masks.empty());
        
        // The skeletonized version should have fewer points than the original
        size_t original_points = 0;
        for (const auto& mask : original_masks) {
            original_points += mask.size();
        }
        
        size_t skeletonized_points = 0;
        for (const auto& mask : skeletonized_masks) {
            skeletonized_points += mask.size();
        }
        
        REQUIRE(skeletonized_points < original_points);

        // Test with progress callback
        progress_val = -1;
        call_count = 0;
        result_skeletonized = skeletonize_mask(mask_data.get(), &params, cb);
        REQUIRE(result_skeletonized != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Empty mask data") {
        mask_data = std::make_shared<MaskData>();
        
        result_skeletonized = skeletonize_mask(mask_data.get(), &params);
        REQUIRE(result_skeletonized != nullptr);
        
        // Should return empty result for empty input
        auto result_masks = result_skeletonized->getAtTime(TimeFrameIndex(100));
        REQUIRE(result_masks.empty());
    }

    SECTION("Null mask data") {
        result_skeletonized = skeletonize_mask(nullptr, &params);
        REQUIRE(result_skeletonized != nullptr);
        
        // Should return empty result for null input
        auto result_masks = result_skeletonized->getAtTime(TimeFrameIndex(100));
        REQUIRE(result_masks.empty());
    }
}

TEST_CASE("Data Transform: Mask Skeletonize - Error and Edge Cases", "[transforms][mask_skeletonize]") {
    std::shared_ptr<MaskData> mask_data;
    std::shared_ptr<MaskData> result_skeletonized;
    MaskSkeletonizeParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Single point mask") {
        mask_data = std::make_shared<MaskData>();
        
        // Create a single point mask
        std::vector<uint32_t> x_coords = {5};
        std::vector<uint32_t> y_coords = {5};
        mask_data->addAtTime(TimeFrameIndex(100), x_coords, y_coords);

        result_skeletonized = skeletonize_mask(mask_data.get(), &params);
        REQUIRE(result_skeletonized != nullptr);
        
        // Single point should remain a single point after skeletonization
        auto result_masks = result_skeletonized->getAtTime(TimeFrameIndex(100));
        REQUIRE(!result_masks.empty());
        REQUIRE(result_masks[0].size() == 1);
    }

    SECTION("Multiple time frames") {
        mask_data = std::make_shared<MaskData>();
        
        // Create masks at multiple time frames
        for (int time = 100; time <= 105; time += 5) {
            // Create a 5x5 square mask at each time frame
            std::vector<uint32_t> x_coords;
            std::vector<uint32_t> y_coords;
            
            for (uint32_t row = 1; row <= 5; ++row) {
                for (uint32_t col = 1; col <= 5; ++col) {
                    x_coords.push_back(col);
                    y_coords.push_back(row);
                }
            }
            
            mask_data->addAtTime(TimeFrameIndex(time), x_coords, y_coords);
        }

        result_skeletonized = skeletonize_mask(mask_data.get(), &params);
        REQUIRE(result_skeletonized != nullptr);
        
        // Should process all time frames
        for (int time = 100; time <= 105; time += 5) {
            auto result_masks = result_skeletonized->getAtTime(TimeFrameIndex(time));
            REQUIRE(!result_masks.empty());
        }
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Skeletonize - JSON pipeline", "[transforms][mask_skeletonize][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "skeletonize_step_1"},
            {"transform_name", "Skeletonize Mask"},
            {"input_key", "TestMask"},
            {"output_key", "SkeletonizedMask"},
            {"parameters", {
                // No parameters needed for skeletonization
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test mask data
    auto mask_data = std::make_shared<MaskData>();
    
    // Create a simple rectangular mask - all points in one mask
    std::vector<uint32_t> x_coords;
    std::vector<uint32_t> y_coords;
    
    // Build all points for a 10x10 rectangle
    for (uint32_t row = 1; row <= 10; ++row) {
        for (uint32_t col = 1; col <= 10; ++col) {
            x_coords.push_back(col);
            y_coords.push_back(row);
        }
    }
    
    mask_data->addAtTime(TimeFrameIndex(100), x_coords, y_coords);
    
    mask_data->setTimeFrame(time_frame);
    dm.setData("TestMask", mask_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto skeletonized_mask = dm.getData<MaskData>("SkeletonizedMask");
    REQUIRE(skeletonized_mask != nullptr);

    // Verify that skeletonization produced a result
    auto original_masks = mask_data->getAtTime(TimeFrameIndex(100));
    auto result_masks = skeletonized_mask->getAtTime(TimeFrameIndex(100));
    
    REQUIRE(!original_masks.empty());
    REQUIRE(!result_masks.empty());
    
    // The skeletonized version should have fewer points than the original
    size_t original_points = 0;
    for (const auto& mask : original_masks) {
        original_points += mask.size();
    }
    
    size_t skeletonized_points = 0;
    for (const auto& mask : result_masks) {
        skeletonized_points += mask.size();
    }
    
    REQUIRE(skeletonized_points < original_points);
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Mask Skeletonize - Parameter Factory", "[transforms][mask_skeletonize][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<MaskSkeletonizeParameters>();
    REQUIRE(params_base != nullptr);

    // No parameters to set for skeletonization, but test the factory works
    const nlohmann::json params_json = {
        // Empty since skeletonization has no parameters
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Skeletonize Mask", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<MaskSkeletonizeParameters*>(params_base.get());
    REQUIRE(params != nullptr);
}

TEST_CASE("Data Transform: Mask Skeletonize - load_data_from_json_config", "[transforms][mask_skeletonize][json_config]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code
    auto test_mask = std::make_shared<MaskData>();
    
    // Create a simple rectangular mask - all points in one mask
    std::vector<uint32_t> x_coords;
    std::vector<uint32_t> y_coords;
    
    // Build all points for a 10x10 rectangle
    for (uint32_t row = 1; row <= 10; ++row) {
        for (uint32_t col = 1; col <= 10; ++col) {
            x_coords.push_back(col);
            y_coords.push_back(row);
        }
    }
    
    test_mask->addAtTime(TimeFrameIndex(100), x_coords, y_coords);
    
    test_mask->setTimeFrame(time_frame);
    
    // Store the mask data in DataManager with a known key
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Skeletonization Pipeline\",\n"
        "            \"description\": \"Test mask skeletonization on rectangular mask\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Skeletonize Mask\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"skeletonized_mask\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_skeletonize_pipeline_test";
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
    auto result_skeletonized = dm.getData<MaskData>("skeletonized_mask");
    REQUIRE(result_skeletonized != nullptr);
    
    // Verify the skeletonization results
    auto original_masks = test_mask->getAtTime(TimeFrameIndex(100));
    auto result_masks = result_skeletonized->getAtTime(TimeFrameIndex(100));
    
    REQUIRE(!original_masks.empty());
    REQUIRE(!result_masks.empty());
    
    // The skeletonized version should have fewer points than the original
    size_t original_points = 0;
    for (const auto& mask : original_masks) {
        original_points += mask.size();
    }
    
    size_t skeletonized_points = 0;
    for (const auto& mask : result_masks) {
        skeletonized_points += mask.size();
    }
    
    REQUIRE(skeletonized_points < original_points);
    
    // Test another pipeline with multiple time frames
    const char* json_config_multiframe = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Multi-frame Mask Skeletonization\",\n"
        "            \"description\": \"Test mask skeletonization on multiple time frames\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Skeletonize Mask\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"skeletonized_mask_multiframe\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_multiframe = test_dir / "pipeline_config_multiframe.json";
    {
        std::ofstream json_file(json_filepath_multiframe);
        REQUIRE(json_file.is_open());
        json_file << json_config_multiframe;
        json_file.close();
    }
    
    // Execute the multi-frame pipeline
    auto data_info_list_multiframe = load_data_from_json_config(&dm, json_filepath_multiframe.string());
    
    // Verify the multi-frame results
    auto result_skeletonized_multiframe = dm.getData<MaskData>("skeletonized_mask_multiframe");
    REQUIRE(result_skeletonized_multiframe != nullptr);
    
    auto result_masks_multiframe = result_skeletonized_multiframe->getAtTime(TimeFrameIndex(100));
    REQUIRE(!result_masks_multiframe.empty());
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
