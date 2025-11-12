#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "Masks/Mask_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/Masks/Mask_To_Line/mask_to_line.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function

// Using Catch::Matchers::Equals for vectors of floats.

TEST_CASE("Data Transform: Mask To Line - Happy Path", "[transforms][mask_to_line]") {
    std::shared_ptr<MaskData> mask_data;
    std::shared_ptr<LineData> result_line;
    MaskToLineParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Skeletonize method - simple mask") {
        // Create a simple rectangular mask
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
            {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12},
            {10, 13}, {11, 13}, {12, 13}, {13, 13}, {14, 13},
            {10, 14}, {11, 14}, {12, 14}, {13, 14}, {14, 14}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() > 0);

        progress_val = -1;
        call_count = 0;
        result_line = mask_to_line(mask_data.get(), &params, cb);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() > 0);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Nearest to Reference method - simple mask") {
        // Create a simple L-shaped mask
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
            {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12},
            {10, 13}, {11, 13}, {12, 13}, {13, 13}, {14, 13},
            {10, 14}, {11, 14}, {12, 14}, {13, 14}, {14, 14},
            {15, 14}, {16, 14}, {17, 14}, {18, 14}, {19, 14}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::NearestToReference;
        params.reference_x = 5.0f;
        params.reference_y = 5.0f;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() > 0);
    }

    SECTION("Smoothing enabled") {
        // Create a simple mask
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
            {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = true;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() > 0);
    }

    SECTION("Multiple time frames") {
        // Create masks at multiple time frames
        std::vector<Point2D<uint32_t>> mask_points_1 = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11}
        };
        
        std::vector<Point2D<uint32_t>> mask_points_2 = {
            {20, 20}, {21, 20}, {22, 20}, {23, 20}, {24, 20},
            {20, 21}, {21, 21}, {22, 21}, {23, 21}, {24, 21}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points_1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(200), mask_points_2, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() == 2); // Should have lines at both time frames
    }

    SECTION("Progress callback detailed check") {
        // Create a simple mask
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
            {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
            progress_values_seen.push_back(p);
        };

        result_line = mask_to_line(mask_data.get(), &params, detailed_cb);
        REQUIRE(result_line != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
        REQUIRE(!progress_values_seen.empty());
        REQUIRE(progress_values_seen.back() == 100);
    }
}

TEST_CASE("Data Transform: Mask To Line - Error and Edge Cases", "[transforms][mask_to_line]") {
    std::shared_ptr<MaskData> mask_data;
    std::shared_ptr<LineData> result_line;
    MaskToLineParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input MaskData") {
        mask_data = nullptr; // Deliberately null
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line->getAllEntries().size() == 0);

        progress_val = -1;
        call_count = 0;
        result_line = mask_to_line(mask_data.get(), &params, cb);
        REQUIRE(result_line->getAllEntries().size() == 0);
        REQUIRE(progress_val == -1);
        REQUIRE(call_count == 0);
    }

    SECTION("Empty MaskData (no masks)") {
        mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() == 0);

        progress_val = -1;
        call_count = 0;
        result_line = mask_to_line(mask_data.get(), &params, cb);
        REQUIRE(result_line != nullptr);
        REQUIRE(result_line->getAllEntries().size() == 0);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Very small mask (single point)") {
        std::vector<Point2D<uint32_t>> mask_points = {{10, 10}};
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        // Single point might not produce a line, but shouldn't crash
    }

    SECTION("High polynomial order with few points") {
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 10; // Higher than number of points
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        // Should handle gracefully even with insufficient points for polynomial fitting
    }

    SECTION("Zero error threshold") {
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 0.0f; // Zero threshold
        params.remove_outliers = true;
        params.input_point_subsample_factor = 1;
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        // Should handle zero threshold gracefully
    }

    SECTION("High subsample factor") {
        std::vector<Point2D<uint32_t>> mask_points = {
            {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
            {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
            {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12}
        };
        
        mask_data = std::make_shared<MaskData>();
        mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
        mask_data->setImageSize(ImageSize{100, 100});
        
        params.method = LinePointSelectionMethod::Skeletonize;
        params.polynomial_order = 3;
        params.error_threshold = 5.0f;
        params.remove_outliers = true;
        params.input_point_subsample_factor = 5; // High subsample factor
        params.should_smooth_line = false;
        params.output_resolution = 5.0f;

        result_line = mask_to_line(mask_data.get(), &params);
        REQUIRE(result_line != nullptr);
        // Should handle high subsample factor gracefully
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask To Line - JSON pipeline", "[transforms][mask_to_line][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "mask_to_line_step_1"},
            {"transform_name", "Convert Mask To Line"},
            {"input_key", "TestMask"},
            {"output_key", "ConvertedLine"},
            {"parameters", {
                {"method", "Skeletonize"},
                {"reference_x", 0.0},
                {"reference_y", 0.0},
                {"polynomial_order", 3},
                {"error_threshold", 5.0},
                {"remove_outliers", true},
                {"input_point_subsample_factor", 1},
                {"should_smooth_line", false},
                {"output_resolution", 5.0}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test mask data
    std::vector<Point2D<uint32_t>> mask_points = {
        {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
        {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
        {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12},
        {10, 13}, {11, 13}, {12, 13}, {13, 13}, {14, 13},
        {10, 14}, {11, 14}, {12, 14}, {13, 14}, {14, 14}
    };
    
    auto mask_data = std::make_shared<MaskData>();
    mask_data->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
    mask_data->setImageSize(ImageSize{100, 100});
    mask_data->setTimeFrame(time_frame);
    dm.setData("TestMask", mask_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto line_data = dm.getData<LineData>("ConvertedLine");
    REQUIRE(line_data != nullptr);
    REQUIRE(line_data->getAllEntries().size() > 0);
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Mask To Line - Parameter Factory", "[transforms][mask_to_line][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<MaskToLineParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"method", "NearestToReference"},
        {"reference_x", 25.5},
        {"reference_y", 30.2},
        {"polynomial_order", 4},
        {"error_threshold", 7.5},
        {"remove_outliers", false},
        {"input_point_subsample_factor", 2},
        {"should_smooth_line", true},
        {"output_resolution", 3.0}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Convert Mask To Line", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<MaskToLineParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->method == LinePointSelectionMethod::NearestToReference);
    REQUIRE(params->reference_x == 25.5f);
    REQUIRE(params->reference_y == 30.2f);
    REQUIRE(params->polynomial_order == 4);
    REQUIRE(params->error_threshold == 7.5f);
    REQUIRE(params->remove_outliers == false);
    REQUIRE(params->input_point_subsample_factor == 2);
    REQUIRE(params->should_smooth_line == true);
    REQUIRE(params->output_resolution == 3.0f);
}

TEST_CASE("Data Transform: Mask To Line - load_data_from_json_config", "[transforms][mask_to_line][json_config]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code
    std::vector<Point2D<uint32_t>> mask_points = {
        {10, 10}, {11, 10}, {12, 10}, {13, 10}, {14, 10},
        {10, 11}, {11, 11}, {12, 11}, {13, 11}, {14, 11},
        {10, 12}, {11, 12}, {12, 12}, {13, 12}, {14, 12},
        {10, 13}, {11, 13}, {12, 13}, {13, 13}, {14, 13},
        {10, 14}, {11, 14}, {12, 14}, {13, 14}, {14, 14}
    };
    
    auto test_mask = std::make_shared<MaskData>();
    test_mask->addAtTime(TimeFrameIndex(100), mask_points, NotifyObservers::No);
    test_mask->setImageSize(ImageSize{100, 100});
    test_mask->setTimeFrame(time_frame);
    
    // Store the mask data in DataManager with a known key
    dm.setData("test_mask", test_mask, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask to Line Pipeline\",\n"
        "            \"description\": \"Test mask to line conversion\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Convert Mask To Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"converted_line\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"Skeletonize\",\n"
        "                    \"reference_x\": 0.0,\n"
        "                    \"reference_y\": 0.0,\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"error_threshold\": 5.0,\n"
        "                    \"remove_outliers\": true,\n"
        "                    \"input_point_subsample_factor\": 1,\n"
        "                    \"should_smooth_line\": false,\n"
        "                    \"output_resolution\": 5.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_to_line_pipeline_test";
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
    auto result_line = dm.getData<LineData>("converted_line");
    REQUIRE(result_line != nullptr);
    REQUIRE(result_line->getAllEntries().size() > 0);
    
    // Test another pipeline with different parameters (Nearest to Reference method)
    const char* json_config_reference = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask to Line with Reference Point\",\n"
        "            \"description\": \"Test mask to line conversion with reference point\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Convert Mask To Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"converted_line_reference\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"NearestToReference\",\n"
        "                    \"reference_x\": 5.0,\n"
        "                    \"reference_y\": 5.0,\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"error_threshold\": 5.0,\n"
        "                    \"remove_outliers\": true,\n"
        "                    \"input_point_subsample_factor\": 1,\n"
        "                    \"should_smooth_line\": false,\n"
        "                    \"output_resolution\": 5.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_reference = test_dir / "pipeline_config_reference.json";
    {
        std::ofstream json_file(json_filepath_reference);
        REQUIRE(json_file.is_open());
        json_file << json_config_reference;
        json_file.close();
    }
    
    // Execute the reference point pipeline
    auto data_info_list_reference = load_data_from_json_config(&dm, json_filepath_reference.string());
    
    // Verify the reference point results
    auto result_line_reference = dm.getData<LineData>("converted_line_reference");
    REQUIRE(result_line_reference != nullptr);
    REQUIRE(result_line_reference->getAllEntries().size() > 0);
    
    // Test smoothing pipeline
    const char* json_config_smooth = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask to Line with Smoothing\",\n"
        "            \"description\": \"Test mask to line conversion with smoothing\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Convert Mask To Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_mask\",\n"
        "                \"output_key\": \"converted_line_smooth\",\n"
        "                \"parameters\": {\n"
        "                    \"method\": \"Skeletonize\",\n"
        "                    \"reference_x\": 0.0,\n"
        "                    \"reference_y\": 0.0,\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"error_threshold\": 5.0,\n"
        "                    \"remove_outliers\": true,\n"
        "                    \"input_point_subsample_factor\": 1,\n"
        "                    \"should_smooth_line\": true,\n"
        "                    \"output_resolution\": 3.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_smooth = test_dir / "pipeline_config_smooth.json";
    {
        std::ofstream json_file(json_filepath_smooth);
        REQUIRE(json_file.is_open());
        json_file << json_config_smooth;
        json_file.close();
    }
    
    // Execute the smoothing pipeline
    auto data_info_list_smooth = load_data_from_json_config(&dm, json_filepath_smooth.string());
    
    // Verify the smoothing results
    auto result_line_smooth = dm.getData<LineData>("converted_line_smooth");
    REQUIRE(result_line_smooth != nullptr);
    REQUIRE(result_line_smooth->getAllEntries().size() > 0);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
