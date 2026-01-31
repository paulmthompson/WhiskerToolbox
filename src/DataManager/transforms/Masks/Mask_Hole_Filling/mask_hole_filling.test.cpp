#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_hole_filling.hpp"
#include "Masks/Mask_Data.hpp"
#include "DataManagerTypes.hpp"

#include "fixtures/scenarios/mask/hole_filling_scenarios.hpp"

// ============================================================================
// Core Functionality Tests (using scenarios)
// ============================================================================

TEST_CASE("MaskHoleFilling - Fills holes in hollow rectangle",
          "[mask_hole_filling][transform][scenario]") {
    auto mask_data = mask_scenarios::hollow_rectangle_6x6();
    
    // Get original size for comparison
    auto original_masks = mask_data->getAtTime(TimeFrameIndex(0));
    REQUIRE(original_masks.size() == 1);
    size_t original_size = original_masks[0].size();
    
    MaskHoleFillingParameters params;
    auto result = fill_mask_holes(mask_data.get(), &params);
    
    REQUIRE(result);
    REQUIRE(result->getTimesWithData().size() == 1);
    
    auto filled_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(filled_masks.size() == 1);
    
    // The hole should now be filled - should have more points than original
    REQUIRE(filled_masks[0].size() > original_size);
    
    // Check that interior points are now present
    bool found_interior = false;
    for (auto const& point : filled_masks[0]) {
        if (point.x == 4 && point.y == 4) { // Center should be filled
            found_interior = true;
            break;
        }
    }
    REQUIRE(found_interior);
}

TEST_CASE("MaskHoleFilling - Preserves solid masks without holes",
          "[mask_hole_filling][transform][scenario]") {
    auto mask_data = mask_scenarios::solid_square_3x3();
    
    // Get original size for comparison
    auto original_masks = mask_data->getAtTime(TimeFrameIndex(1));
    REQUIRE(original_masks.size() == 1);
    size_t original_size = original_masks[0].size();
    
    MaskHoleFillingParameters params;
    auto result = fill_mask_holes(mask_data.get(), &params);
    
    REQUIRE(result);
    REQUIRE(result->getTimesWithData().size() == 1);
    
    auto result_masks = result->getAtTime(TimeFrameIndex(1));
    REQUIRE(result_masks.size() == 1);
    
    // Should have same number of points (no holes to fill)
    REQUIRE(result_masks[0].size() == original_size);
}

TEST_CASE("MaskHoleFilling - Handles multiple masks at same time",
          "[mask_hole_filling][transform][scenario]") {
    auto mask_data = mask_scenarios::multiple_masks_hollow_and_solid();
    
    MaskHoleFillingParameters params;
    auto result = fill_mask_holes(mask_data.get(), &params);
    
    REQUIRE(result);
    auto result_masks = result->getAtTime(TimeFrameIndex(2));
    REQUIRE(result_masks.size() == 2);
    
    // Verify we have the expected sizes
    std::vector<size_t> mask_sizes;
    for (auto const& mask : result_masks) {
        mask_sizes.push_back(mask.size());
    }
    std::sort(mask_sizes.begin(), mask_sizes.end());
    
    // Should have one mask with 4 points (unchanged 2x2) and one with 16 points (filled 4x4)
    REQUIRE(mask_sizes[0] == 4);  // Small solid square unchanged
    REQUIRE(mask_sizes[1] == 16); // Hollow rectangle filled to solid 4x4
}

// ============================================================================
// Edge Cases (using scenarios)
// ============================================================================

TEST_CASE("MaskHoleFilling - Handles empty mask data",
          "[mask_hole_filling][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_data();
    
    MaskHoleFillingParameters params;
    auto result = fill_mask_holes(mask_data.get(), &params);
    
    REQUIRE(result);
    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("MaskHoleFilling - Handles null input",
          "[mask_hole_filling][transform][edge]") {
    MaskHoleFillingParameters params;
    auto result = fill_mask_holes(nullptr, &params);
    
    REQUIRE(result);
    REQUIRE(result->getTimesWithData().empty());
}

// ============================================================================
// Operation Interface Tests
// ============================================================================

TEST_CASE("MaskHoleFillingOperation - Operation metadata",
          "[mask_hole_filling][operation]") {
    MaskHoleFillingOperation op;
    
    REQUIRE(op.getName() == "Fill Mask Holes");
    REQUIRE(op.getTargetInputTypeIndex() == std::type_index(typeid(std::shared_ptr<MaskData>)));
    
    auto default_params = op.getDefaultParameters();
    REQUIRE(default_params != nullptr);
    auto hole_filling_params = dynamic_cast<MaskHoleFillingParameters*>(default_params.get());
    REQUIRE(hole_filling_params != nullptr);
}

TEST_CASE("MaskHoleFillingOperation - Can apply to valid MaskData",
          "[mask_hole_filling][operation][scenario]") {
    MaskHoleFillingOperation op;
    auto mask_data = mask_scenarios::hollow_rectangle_6x6();
    DataTypeVariant valid_variant = mask_data;
    REQUIRE(op.canApply(valid_variant));
}

TEST_CASE("MaskHoleFillingOperation - Cannot apply to null MaskData",
          "[mask_hole_filling][operation]") {
    MaskHoleFillingOperation op;
    std::shared_ptr<MaskData> null_mask_data = nullptr;
    DataTypeVariant null_variant = null_mask_data;
    REQUIRE_FALSE(op.canApply(null_variant));
}

TEST_CASE("MaskHoleFillingOperation - Execute with valid input",
          "[mask_hole_filling][operation][scenario]") {
    MaskHoleFillingOperation op;
    auto mask_data = mask_scenarios::donut_shape_4x4();
    
    DataTypeVariant input_variant = mask_data;
    MaskHoleFillingParameters params;
    
    auto result_variant = op.execute(input_variant, &params, [](int){});
    
    REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
    
    auto result_mask_data = std::get<std::shared_ptr<MaskData>>(result_variant);
    REQUIRE(result_mask_data);
    
    auto result_masks = result_mask_data->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    
    // Should have filled the hole (16 points total for 4x4 square)
    REQUIRE(result_masks[0].size() == 16);
}

// ============================================================================
// JSON Pipeline Tests
// ============================================================================

#include "DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Hole Filling - JSON pipeline",
          "[transforms][mask_hole_filling][json][scenario]") {
    // Create DataManager with time frame
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto test_mask = mask_scenarios::json_pipeline_hollow_rectangle_hole_filling();
    test_mask->setTimeFrame(time_frame);
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
    // Get original size for comparison
    auto original_masks = test_mask->getAtTime(TimeFrameIndex(0));
    REQUIRE(original_masks.size() == 1);
    size_t original_size = original_masks[0].size();
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Hole Filling Pipeline\",\n"
        "            \"description\": \"Test mask hole filling on hollow rectangle\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Fill Mask Holes\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"filled_mask\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_hole_filling_pipeline_test";
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
    auto result_mask = dm.getData<MaskData>("filled_mask");
    REQUIRE(result_mask != nullptr);
    
    // Verify the hole filling results
    auto result_masks = result_mask->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    
    // The hole should now be filled - should have more points than original
    REQUIRE(result_masks[0].size() > original_size);
    
    // Check that interior points are now present (the hole should be filled)
    bool found_interior = false;
    for (auto const& point : result_masks[0]) {
        if (point.x == 4 && point.y == 4) { // Center should be filled
            found_interior = true;
            break;
        }
    }
    REQUIRE(found_interior);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Hole Filling - Multi-mask JSON pipeline",
          "[transforms][mask_hole_filling][json][scenario]") {
    // Create DataManager with time frame
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto test_mask_multi = mask_scenarios::json_pipeline_multi_mask_hole_filling();
    test_mask_multi->setTimeFrame(time_frame);
    dm.setData("test_mask_multi", test_mask_multi, TimeKey("default"));
    
    const char* json_config_multi = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Hole Filling Multi Pipeline\",\n"
        "            \"description\": \"Test mask hole filling on multiple masks\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Fill Mask Holes\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask_multi\",\n"
        "                \"output_key\": \"filled_mask_multi\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_hole_filling_multi_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_multi = test_dir / "pipeline_config_multi.json";
    {
        std::ofstream json_file(json_filepath_multi);
        REQUIRE(json_file.is_open());
        json_file << json_config_multi;
        json_file.close();
    }
    
    // Execute the multi-mask pipeline
    auto data_info_list_multi = load_data_from_json_config(&dm, json_filepath_multi.string());
    
    // Verify the multi-mask results
    auto result_mask_multi = dm.getData<MaskData>("filled_mask_multi");
    REQUIRE(result_mask_multi != nullptr);
    
    auto result_masks_multi = result_mask_multi->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks_multi.size() == 2);
    
    // Verify we have the expected sizes
    std::vector<size_t> mask_sizes;
    for (auto const& mask : result_masks_multi) {
        mask_sizes.push_back(mask.size());
    }
    std::sort(mask_sizes.begin(), mask_sizes.end());
    
    // Should have one mask with 4 points (unchanged 2x2) and one with 16 points (filled 4x4)
    REQUIRE(mask_sizes[0] == 4);  // Small solid square unchanged
    REQUIRE(mask_sizes[1] == 16); // Hollow rectangle filled to solid 4x4
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
} 