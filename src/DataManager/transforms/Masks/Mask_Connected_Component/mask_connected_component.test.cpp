#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_connected_component.hpp"
#include "Masks/Mask_Data.hpp"

#include <set>

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/scenarios/mask/connected_component_scenarios.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

// ============================================================================
// Core Functionality Tests (using scenarios)
// ============================================================================

TEST_CASE("MaskConnectedComponent - removes small components while preserving large ones",
          "[mask_connected_component][scenario]") {
    auto mask_data = mask_scenarios::large_and_small_components();
    
    // Set threshold to 5 - should keep the 9-pixel component, remove the 1 and 2-pixel components
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = 5;
    
    auto result = remove_small_connected_components(mask_data.get(), params.get());
    
    REQUIRE(result != nullptr);
    
    // Check that we have data at time 0
    auto times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(0));
    
    // Get masks at time 0
    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    
    // Should have one mask (the large component preserved)
    REQUIRE(result_masks.size() == 1);
    
    // The preserved mask should have 9 points
    auto const & preserved_mask = result_masks[0];
    REQUIRE(preserved_mask.size() == 9);
}

TEST_CASE("MaskConnectedComponent - preserves all components when threshold is 1",
          "[mask_connected_component][scenario]") {
    auto mask_data = mask_scenarios::multiple_small_components();
    
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = 1;
    
    auto result = remove_small_connected_components(mask_data.get(), params.get());
    
    REQUIRE(result != nullptr);
    
    auto const & result_masks = result->getAtTime(TimeFrameIndex(10));
    
    // Should preserve all 3 components
    REQUIRE(result_masks.size() == 3);
    
    // Total pixels should be 1 + 1 + 2 = 4
    size_t total_pixels = 0;
    for (auto const & mask : result_masks) {
        total_pixels += mask.size();
    }
    REQUIRE(total_pixels == 4);
}

TEST_CASE("MaskConnectedComponent - removes all components when threshold is too high",
          "[mask_connected_component][scenario]") {
    auto mask_data = mask_scenarios::medium_components();
    
    // Set threshold higher than any component (max is 3 pixels)
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = 10;
    
    auto result = remove_small_connected_components(mask_data.get(), params.get());
    
    REQUIRE(result != nullptr);
    
    // Should have no masks at time 5 (all removed)
    auto const & result_masks = result->getAtTime(TimeFrameIndex(5));
    REQUIRE(result_masks.empty());
    
    // Should have no times with data
    auto times = result->getTimesWithData();
    REQUIRE(times.empty());
}

TEST_CASE("MaskConnectedComponent - handles empty mask data",
          "[mask_connected_component][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_data();
    
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = 5;
    
    auto result = remove_small_connected_components(mask_data.get(), params.get());
    
    REQUIRE(result != nullptr);
    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("MaskConnectedComponent - handles multiple time points",
          "[mask_connected_component][scenario]") {
    auto mask_data = mask_scenarios::multiple_timestamps();
    
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = 4;
    
    auto result = remove_small_connected_components(mask_data.get(), params.get());
    
    REQUIRE(result != nullptr);
    
    auto times = result->getTimesWithData();
    
    // Should preserve times 0 and 2, remove time 1
    REQUIRE(times.size() == 2);
    REQUIRE(*times.begin() == TimeFrameIndex(0));
    auto it = times.begin();
    std::advance(it, 1);
    REQUIRE(*it == TimeFrameIndex(2));
    
    // Verify preserved components have correct sizes
    auto const & masks_t0 = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(masks_t0.size() == 1);
    REQUIRE(masks_t0[0].size() == 6);
    
    auto const & masks_t2 = result->getAtTime(TimeFrameIndex(2));
    REQUIRE(masks_t2.size() == 1);
    REQUIRE(masks_t2[0].size() == 5);
    
    // Time 1 should be empty
    auto const & masks_t1 = result->getAtTime(TimeFrameIndex(1));
    REQUIRE(masks_t1.empty());
}

// ============================================================================
// Operation Interface Tests (using scenarios)
// ============================================================================

TEST_CASE("MaskConnectedComponentOperation - name and type checking",
          "[mask_connected_component][operation][scenario]") {
    MaskConnectedComponentOperation op;
    
    REQUIRE(op.getName() == "Remove Small Connected Components");
    REQUIRE(op.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
    
    // Test canApply with correct type
    auto mask_data = mask_scenarios::large_and_small_components();
    DataTypeVariant valid_variant = mask_data;
    REQUIRE(op.canApply(valid_variant));
    
    // Test canApply with null pointer
    std::shared_ptr<MaskData> null_mask;
    DataTypeVariant null_variant = null_mask;
    REQUIRE_FALSE(op.canApply(null_variant));
}

TEST_CASE("MaskConnectedComponentOperation - default parameters",
          "[mask_connected_component][operation][scenario]") {
    MaskConnectedComponentOperation op;
    auto params = op.getDefaultParameters();
    
    REQUIRE(params != nullptr);
    
    auto mask_params = dynamic_cast<MaskConnectedComponentParameters*>(params.get());
    REQUIRE(mask_params != nullptr);
    REQUIRE(mask_params->threshold == 10);
}

TEST_CASE("MaskConnectedComponentOperation - execute operation",
          "[mask_connected_component][operation][scenario]") {
    auto mask_data = mask_scenarios::operation_test_data();
    
    MaskConnectedComponentOperation op;
    DataTypeVariant input_variant = mask_data;
    
    auto params = op.getDefaultParameters();  // threshold = 10
    auto result_variant = op.execute(input_variant, params.get());
    
    REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
    
    auto result = std::get<std::shared_ptr<MaskData>>(result_variant);
    REQUIRE(result != nullptr);
    
    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);  // Only large component preserved
    REQUIRE(result_masks[0].size() == 12);  // Large component has 12 pixels
} 

// ============================================================================
// JSON Pipeline Tests (using scenarios)
// ============================================================================

TEST_CASE("Data Transform: Mask Connected Component - JSON pipeline",
          "[transforms][mask_connected_component][json][scenario]") {
    // Create DataManager and register test data
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);
    
    auto mask_data = mask_scenarios::json_pipeline_mixed();
    mask_data->setTimeFrame(time_frame);
    dm->setData("json_pipeline_mixed", mask_data, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Connected Component Pipeline\",\n"
        "            \"description\": \"Test connected component analysis on mask data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Remove Small Connected Components\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_mixed\",\n"
        "                \"output_key\": \"filtered_mask\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold\": 3\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_connected_component_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(dm.get(), json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_mask = dm->getData<MaskData>("filtered_mask");
    REQUIRE(result_mask != nullptr);
    
    // Verify the connected component filtering results
    auto const & result_masks = result_mask->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 2);  // Should have 2 components (large and medium)
    
    // Check that we have the correct total number of pixels
    size_t total_pixels = 0;
    for (auto const & mask : result_masks) {
        total_pixels += mask.size();
    }
    REQUIRE(total_pixels == 13);  // 9 (large) + 4 (medium) = 13 pixels
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Connected Component - strict threshold JSON pipeline",
          "[transforms][mask_connected_component][json][scenario]") {
    // Create DataManager and register test data
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);
    
    auto mask_data = mask_scenarios::json_pipeline_mixed();
    mask_data->setTimeFrame(time_frame);
    dm->setData("json_pipeline_mixed", mask_data, TimeKey("default"));
    
    // Test pipeline with higher threshold (should remove more components)
    const char* json_config_strict = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Strict Connected Component Filtering\",\n"
        "            \"description\": \"Test connected component filtering with higher threshold\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Remove Small Connected Components\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_mixed\",\n"
        "                \"output_key\": \"strictly_filtered_mask\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold\": 5\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_connected_component_strict_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_strict = test_dir / "pipeline_config_strict.json";
    {
        std::ofstream json_file(json_filepath_strict);
        REQUIRE(json_file.is_open());
        json_file << json_config_strict;
        json_file.close();
    }
    
    // Execute the strict filtering pipeline
    auto data_info_list_strict = load_data_from_json_config(dm.get(), json_filepath_strict.string());
    
    // Verify the strict filtering results
    auto result_mask_strict = dm->getData<MaskData>("strictly_filtered_mask");
    REQUIRE(result_mask_strict != nullptr);
    
    auto const & result_masks_strict = result_mask_strict->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks_strict.size() == 1);  // Should have only 1 component (large)
    REQUIRE(result_masks_strict[0].size() == 9);  // Large component has 9 pixels
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Connected Component - permissive threshold JSON pipeline",
          "[transforms][mask_connected_component][json][scenario]") {
    // Create DataManager and register test data
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);
    
    auto mask_data = mask_scenarios::json_pipeline_mixed();
    mask_data->setTimeFrame(time_frame);
    dm->setData("json_pipeline_mixed", mask_data, TimeKey("default"));
    
    // Test pipeline with very low threshold (should preserve all components)
    const char* json_config_permissive = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Permissive Connected Component Filtering\",\n"
        "            \"description\": \"Test connected component filtering with very low threshold\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Remove Small Connected Components\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_mixed\",\n"
        "                \"output_key\": \"permissive_filtered_mask\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold\": 1\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_connected_component_permissive_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_permissive = test_dir / "pipeline_config_permissive.json";
    {
        std::ofstream json_file(json_filepath_permissive);
        REQUIRE(json_file.is_open());
        json_file << json_config_permissive;
        json_file.close();
    }
    
    // Execute the permissive filtering pipeline
    auto data_info_list_permissive = load_data_from_json_config(dm.get(), json_filepath_permissive.string());
    
    // Verify the permissive filtering results
    auto result_mask_permissive = dm->getData<MaskData>("permissive_filtered_mask");
    REQUIRE(result_mask_permissive != nullptr);
    
    auto const & result_masks_permissive = result_mask_permissive->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks_permissive.size() == 3);  // Should preserve all 3 components
    
    // Check total pixels in permissive result
    size_t total_pixels_permissive = 0;
    for (auto const & mask : result_masks_permissive) {
        total_pixels_permissive += mask.size();
    }
    REQUIRE(total_pixels_permissive == 14);  // 9 + 1 + 4 = 14 pixels
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
} 