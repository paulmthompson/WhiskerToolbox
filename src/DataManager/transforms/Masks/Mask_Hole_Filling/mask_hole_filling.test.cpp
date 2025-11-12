#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_hole_filling.hpp"
#include "Masks/Mask_Data.hpp"
#include "DataManagerTypes.hpp"

TEST_CASE("MaskHoleFilling basic functionality", "[mask_hole_filling]") {
    
    SECTION("fills holes in simple rectangular mask") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a hollow rectangle with hole in the middle
        std::vector<Point2D<uint32_t>> hollow_rect;
        
        // Outer border
        for (uint32_t row = 2; row < 8; ++row) {
            for (uint32_t col = 2; col < 8; ++col) {
                if (row == 2 || row == 7 || col == 2 || col == 7) {
                    hollow_rect.emplace_back(col, row);
                }
            }
        }
        
        mask_data->addAtTime(TimeFrameIndex(0), hollow_rect, NotifyObservers::No);
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().size() == 1);
        
        auto filled_masks = result->getAtTime(TimeFrameIndex(0));
        REQUIRE(filled_masks.size() == 1);
        
        // The hole should now be filled - should have more points than original
        REQUIRE(filled_masks[0].size() > hollow_rect.size());
        
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
    
    SECTION("preserves solid masks without holes") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({8, 8});
        
        // Create a solid 3x3 square
        std::vector<Point2D<uint32_t>> solid_square;
        for (uint32_t row = 2; row < 5; ++row) {
            for (uint32_t col = 2; col < 5; ++col) {
                solid_square.emplace_back(col, row);
            }
        }

        mask_data->addAtTime(TimeFrameIndex(1), solid_square, NotifyObservers::No);

        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().size() == 1);
        
        auto result_masks = result->getAtTime(TimeFrameIndex(1));
        REQUIRE(result_masks.size() == 1);
        
        // Should have same number of points (no holes to fill)
        REQUIRE(result_masks[0].size() == solid_square.size());
    }
    
    SECTION("handles multiple masks at same time") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({12, 8});
        
        // First mask: hollow rectangle (4x4 with hole in middle)
        std::vector<Point2D<uint32_t>> hollow_rect;
        for (uint32_t row = 1; row < 5; ++row) {
            for (uint32_t col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    hollow_rect.emplace_back(col, row);
                }
            }
        }
        mask_data->addAtTime(TimeFrameIndex(2), hollow_rect, NotifyObservers::No);
        
        // Second mask: small solid 2x2 square
        std::vector<Point2D<uint32_t>> solid_square;
        for (uint32_t row = 1; row < 3; ++row) {
            for (uint32_t col = 7; col < 9; ++col) {
                solid_square.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(2), solid_square, NotifyObservers::No);
        
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
    
    SECTION("handles empty mask data") {
        auto mask_data = std::make_shared<MaskData>();
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("handles null input") {
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(nullptr, &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
}

TEST_CASE("MaskHoleFillingOperation interface tests", "[mask_hole_filling][operation]") {
    
    SECTION("operation metadata") {
        MaskHoleFillingOperation op;
        
        REQUIRE(op.getName() == "Fill Mask Holes");
        REQUIRE(op.getTargetInputTypeIndex() == std::type_index(typeid(std::shared_ptr<MaskData>)));
        
        auto default_params = op.getDefaultParameters();
        REQUIRE(default_params != nullptr);
        auto hole_filling_params = dynamic_cast<MaskHoleFillingParameters*>(default_params.get());
        REQUIRE(hole_filling_params != nullptr);
    }
    
    SECTION("canApply method") {
        MaskHoleFillingOperation op;
        
        // Test with valid MaskData
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant valid_variant = mask_data;
        REQUIRE(op.canApply(valid_variant));
        
        // Test with null MaskData
        std::shared_ptr<MaskData> null_mask_data = nullptr;
        DataTypeVariant null_variant = null_mask_data;
        REQUIRE_FALSE(op.canApply(null_variant));
        
    }
    
    SECTION("execute with valid input") {
        MaskHoleFillingOperation op;
        
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({6, 6});
        
        // Create a donut shape (outer ring)
        std::vector<Point2D<uint32_t>> donut;
        for (uint32_t row = 1; row < 5; ++row) {
            for (uint32_t col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    donut.emplace_back(col, row);
                }
            }
        }
        mask_data->addAtTime(TimeFrameIndex(0), donut, NotifyObservers::No);
        
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
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "transforms/ParameterFactory.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Hole Filling - JSON pipeline", "[transforms][mask_hole_filling][json]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code - a hollow rectangle with hole in the middle
    auto test_mask = std::make_shared<MaskData>();
    test_mask->setImageSize({10, 10});
    test_mask->setTimeFrame(time_frame);
    
    // Create a hollow rectangle (6x6 with hole in middle)
    std::vector<Point2D<uint32_t>> hollow_rect;
    for (uint32_t row = 2; row < 8; ++row) {
        for (uint32_t col = 2; col < 8; ++col) {
            if (row == 2 || row == 7 || col == 2 || col == 7) {
                hollow_rect.emplace_back(col, row);
            }
        }
    }
    test_mask->addAtTime(TimeFrameIndex(0), hollow_rect, NotifyObservers::No);
    
    // Store the mask data in DataManager with a known key
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
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
    REQUIRE(result_masks[0].size() > hollow_rect.size());
    
    // Check that interior points are now present (the hole should be filled)
    bool found_interior = false;
    for (auto const& point : result_masks[0]) {
        if (point.x == 4 && point.y == 4) { // Center should be filled
            found_interior = true;
            break;
        }
    }
    REQUIRE(found_interior);
    
    // Test another pipeline with multiple masks
    auto test_mask_multi = std::make_shared<MaskData>();
    test_mask_multi->setImageSize({12, 8});
    test_mask_multi->setTimeFrame(time_frame);
    
    // First mask: hollow rectangle (4x4 with hole in middle)
    std::vector<Point2D<uint32_t>> hollow_rect_small;
    for (uint32_t row = 1; row < 5; ++row) {
        for (uint32_t col = 1; col < 5; ++col) {
            if (row == 1 || row == 4 || col == 1 || col == 4) {
                hollow_rect_small.emplace_back(col, row);
            }
        }
    }
    test_mask_multi->addAtTime(TimeFrameIndex(0), hollow_rect_small, NotifyObservers::No);
    
    // Second mask: small solid 2x2 square
    std::vector<Point2D<uint32_t>> solid_square;
    for (uint32_t row = 1; row < 3; ++row) {
        for (uint32_t col = 7; col < 9; ++col) {
            solid_square.emplace_back(col, row);
        }
    }
    test_mask_multi->addAtTime(TimeFrameIndex(0), solid_square, NotifyObservers::No);
    
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