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

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("MaskConnectedComponent basic functionality", "[mask_connected_component]") {
    
    SECTION("removes small components while preserving large ones") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a mask with a large component (9 pixels) and small components (1-2 pixels each)
        Mask2D large_component = {
            {1, 1}, {2, 1}, {3, 1},  // Row 1
            {1, 2}, {2, 2}, {3, 2},  // Row 2  
            {1, 3}, {2, 3}, {3, 3}   // Row 3 (3x3 square)
        };
        
        Mask2D small_component1 = {
            {7, 1}  // Single pixel
        };
        
        Mask2D small_component2 = {
            {7, 7}, {8, 7}  // Two adjacent pixels
        };
        
        // Add all components to the same time
        mask_data->addAtTime(TimeFrameIndex(0), large_component, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), small_component1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), small_component2, NotifyObservers::No);

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
        
        // Verify the preserved points are from the large component
        std::set<std::pair<uint32_t, uint32_t>> expected_points;
        for (auto const & point : large_component) {
            expected_points.insert({point.x, point.y});
        }
        
        std::set<std::pair<uint32_t, uint32_t>> actual_points;
        for (auto const & point : preserved_mask) {
            actual_points.insert({point.x, point.y});
        }
        
        REQUIRE(actual_points == expected_points);
    }
    
    SECTION("preserves all components when threshold is 1") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({5, 5});
        
        // Create several small components
        Mask2D component1 = {{1, 1}};
        Mask2D component2 = {{3, 3}};
        Mask2D component3 = {{0, 4}, {1, 4}};  // 2-pixel component

        mask_data->addAtTime(TimeFrameIndex(10), component1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), component2, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10), component3, NotifyObservers::No);

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
    
    SECTION("removes all components when threshold is too high") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create some medium-sized components
        Mask2D component1 = {{0, 0}, {1, 0}, {0, 1}};  // 3 pixels
        Mask2D component2 = {{5, 5}, {6, 5}};  // 2 pixels

        mask_data->addAtTime(TimeFrameIndex(5), component1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(5), component2, NotifyObservers::No);

        // Set threshold higher than any component
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
    
    SECTION("handles empty mask data") {
        auto mask_data = std::make_shared<MaskData>();
        
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 5;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("handles multiple time points") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({8, 8});
        
        // Time 0: Large component (should be preserved)
        Mask2D large_comp = {
            {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}  // 6 pixels
        };
        
        // Time 1: Small component (should be removed)
        Mask2D small_comp = {
            {5, 5}, {5, 6}  // 2 pixels
        };
        
        // Time 2: Medium component (should be preserved)
        Mask2D medium_comp = {
            {3, 3}, {4, 3}, {3, 4}, {4, 4}, {3, 5}  // 5 pixels
        };

        mask_data->addAtTime(TimeFrameIndex(0), large_comp, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(1), small_comp, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), medium_comp, NotifyObservers::No);

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
}

TEST_CASE("MaskConnectedComponentOperation interface", "[mask_connected_component][operation]") {
    
    SECTION("operation name and type checking") {
        MaskConnectedComponentOperation op;
        
        REQUIRE(op.getName() == "Remove Small Connected Components");
        REQUIRE(op.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
        
        // Test canApply with correct type
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant valid_variant = mask_data;
        REQUIRE(op.canApply(valid_variant));
        
        // Test canApply with null pointer
        std::shared_ptr<MaskData> null_mask;
        DataTypeVariant null_variant = null_mask;
        REQUIRE_FALSE(op.canApply(null_variant));
    }
    
    SECTION("default parameters") {
        MaskConnectedComponentOperation op;
        auto params = op.getDefaultParameters();
        
        REQUIRE(params != nullptr);
        
        auto mask_params = dynamic_cast<MaskConnectedComponentParameters*>(params.get());
        REQUIRE(mask_params != nullptr);
        REQUIRE(mask_params->threshold == 10);
    }
    
    SECTION("execute operation") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({6, 6});
        
        // Add a component larger than default threshold
        Mask2D large_comp = {
            {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1},
            {0, 2}, {1, 2}, {2, 2}, {3, 0}, {3, 1}, {3, 2}  // 12 pixels
        };
        
        // Add a small component 
        Mask2D small_comp = {
            {5, 5}  // 1 pixel
        };

        mask_data->addAtTime(TimeFrameIndex(0), large_comp, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), small_comp, NotifyObservers::No);

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
} 

TEST_CASE("Data Transform: Mask Connected Component - JSON pipeline", "[transforms][mask_connected_component][json]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data with multiple components of different sizes
    auto test_mask = std::make_shared<MaskData>();
    test_mask->setImageSize({10, 10});
    test_mask->setTimeFrame(time_frame);
    
    // Large component (9 pixels) - should be preserved
    Mask2D large_component = {
        {1, 1}, {2, 1}, {3, 1},  // Row 1
        {1, 2}, {2, 2}, {3, 2},  // Row 2  
        {1, 3}, {2, 3}, {3, 3}   // Row 3 (3x3 square)
    };
    
    // Small component (1 pixel) - should be removed
    Mask2D small_component1 = {
        {7, 1}  // Single pixel
    };
    
    // Medium component (4 pixels) - should be preserved
    Mask2D medium_component = {
        {5, 5}, {6, 5}, {5, 6}, {6, 6}  // 2x2 square
    };
    
    // Add all components to the mask
    test_mask->addAtTime(TimeFrameIndex(0), large_component, NotifyObservers::No);
    test_mask->addAtTime(TimeFrameIndex(0), small_component1, NotifyObservers::No);
    test_mask->addAtTime(TimeFrameIndex(0), medium_component, NotifyObservers::No);

    // Store the mask data in DataManager with a known key
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
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
        "                \"input_key\": \"test_mask\",\n"
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
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_mask = dm.getData<MaskData>("filtered_mask");
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
    
    // Test another pipeline with different threshold (should remove more components)
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
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"strictly_filtered_mask\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold\": 5\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_strict = test_dir / "pipeline_config_strict.json";
    {
        std::ofstream json_file(json_filepath_strict);
        REQUIRE(json_file.is_open());
        json_file << json_config_strict;
        json_file.close();
    }
    
    // Execute the strict filtering pipeline
    auto data_info_list_strict = load_data_from_json_config(&dm, json_filepath_strict.string());
    
    // Verify the strict filtering results
    auto result_mask_strict = dm.getData<MaskData>("strictly_filtered_mask");
    REQUIRE(result_mask_strict != nullptr);
    
    auto const & result_masks_strict = result_mask_strict->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks_strict.size() == 1);  // Should have only 1 component (large)
    REQUIRE(result_masks_strict[0].size() == 9);  // Large component has 9 pixels
    
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
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"permissive_filtered_mask\",\n"
        "                \"parameters\": {\n"
        "                    \"threshold\": 1\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_permissive = test_dir / "pipeline_config_permissive.json";
    {
        std::ofstream json_file(json_filepath_permissive);
        REQUIRE(json_file.is_open());
        json_file << json_config_permissive;
        json_file.close();
    }
    
    // Execute the permissive filtering pipeline
    auto data_info_list_permissive = load_data_from_json_config(&dm, json_filepath_permissive.string());
    
    // Verify the permissive filtering results
    auto result_mask_permissive = dm.getData<MaskData>("permissive_filtered_mask");
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