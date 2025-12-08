#include "transforms/Masks/Mask_Centroid/mask_centroid.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

#include "fixtures/scenarios/mask/centroid_scenarios.hpp"

// ============================================================================
// Core Functionality Tests (using scenarios)
// ============================================================================

TEST_CASE("Mask centroid calculation - Empty mask data",
          "[mask][centroid][transform][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_data();
    auto result = calculate_mask_centroid(mask_data.get());

    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("Mask centroid calculation - Single mask at one timestamp",
          "[mask][centroid][transform][scenario]") {
    auto mask_data = mask_scenarios::single_mask_triangle();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(10));

    auto const & points = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(points.size() == 1);

    // Centroid of triangle with vertices (0,0), (3,0), (0,3) should be (1,1)
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Mask centroid calculation - Multiple masks at one timestamp",
          "[mask][centroid][transform][scenario]") {
    auto mask_data = mask_scenarios::multiple_masks_single_timestamp_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(20));

    auto const & points_view = result->getAtTime(TimeFrameIndex(20));
    std::vector<Point2D<float>> points(points_view.begin(), points_view.end());
    REQUIRE(points.size() == 2);// Two masks = two centroids

    // Sort points by x coordinate for consistent testing
    auto sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });

    // First centroid: (0+1+0+1)/4 = 0.5, (0+0+1+1)/4 = 0.5
    REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(0.5f, 0.001f));

    // Second centroid: (4+5+4+5)/4 = 4.5, (4+4+5+5)/4 = 4.5
    REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(4.5f, 0.001f));
    REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(4.5f, 0.001f));
}

TEST_CASE("Mask centroid calculation - Masks across multiple timestamps",
          "[mask][centroid][transform][scenario]") {
    auto mask_data = mask_scenarios::masks_multiple_timestamps_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & times = result->getTimesWithData();
    REQUIRE(times.size() == 2);

    // Check timestamp 30 centroid: (0+2+4)/3 = 2, (0+0+0)/3 = 0
    auto const & points30 = result->getAtTime(TimeFrameIndex(30));
    REQUIRE(points30.size() == 1);
    REQUIRE_THAT(points30[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points30[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));

    // Check timestamp 40 centroid: (1+1+1)/3 = 1, (0+3+6)/3 = 3
    auto const & points40 = result->getAtTime(TimeFrameIndex(40));
    REQUIRE(points40.size() == 1);
    REQUIRE_THAT(points40[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points40[0].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
}

TEST_CASE("Mask centroid calculation - Verify image size is preserved",
          "[mask][centroid][transform][scenario]") {
    auto mask_data = mask_scenarios::mask_with_image_size_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    // Verify image size is copied
    REQUIRE(result->getImageSize().width == 640);
    REQUIRE(result->getImageSize().height == 480);

    // Verify calculation is correct: (100+200+300)/3 = 200, (100+150+200)/3 = 150
    auto const & points = result->getAtTime(TimeFrameIndex(100));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
}

// ============================================================================
// Edge Cases (using scenarios)
// ============================================================================

TEST_CASE("Mask centroid calculation - Masks with zero points",
          "[mask][centroid][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_at_timestamp_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    // Empty masks should be skipped
    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("Mask centroid calculation - Mixed empty and non-empty masks",
          "[mask][centroid][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::mixed_empty_nonempty_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(20));

    auto const & points = result->getAtTime(TimeFrameIndex(20));
    REQUIRE(points.size() == 1);// Only counts the non-empty mask

    // Centroid: (2+4)/2 = 3, (1+3)/2 = 2
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("Mask centroid calculation - Single point masks",
          "[mask][centroid][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::single_point_masks_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & points_view = result->getAtTime(TimeFrameIndex(30));
    std::vector<Point2D<float>> points(points_view.begin(), points_view.end());
    REQUIRE(points.size() == 2);

    // Sort points by x coordinate for consistent testing
    auto sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });

    // First mask centroid should be exactly the single point
    REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(7.0f, 0.001f));

    // Second mask centroid should be exactly the single point
    REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(15.0f, 0.001f));
}

TEST_CASE("Mask centroid calculation - Large coordinates",
          "[mask][centroid][transform][edge][scenario]") {
    auto mask_data = mask_scenarios::large_coordinates_centroid();
    auto result = calculate_mask_centroid(mask_data.get());

    auto const & points = result->getAtTime(TimeFrameIndex(40));
    REQUIRE(points.size() == 1);

    // Centroid: (1000000+1000001+1000002)/3 = 1000001, (2000000+2000001+2000002)/3 = 2000001
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1000001.0f, 0.1f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2000001.0f, 0.1f));
}

TEST_CASE("Mask centroid calculation - Null input handling", "[mask][centroid][transform][edge]") {
    auto result = calculate_mask_centroid(nullptr);
    REQUIRE(result->getTimesWithData().empty());
}

// ============================================================================
// Operation Interface Tests
// ============================================================================

TEST_CASE("MaskCentroidOperation - Operation interface", "[mask][centroid][operation]") {
    MaskCentroidOperation operation;

    SECTION("Operation name") {
        REQUIRE(operation.getName() == "Calculate Mask Centroid");
    }

    SECTION("Target type index") {
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params != nullptr);
        REQUIRE(dynamic_cast<MaskCentroidParameters *>(params.get()) != nullptr);
    }
}

TEST_CASE("MaskCentroidOperation - Can apply to valid MaskData",
          "[mask][centroid][operation][scenario]") {
    MaskCentroidOperation operation;
    auto mask_data = mask_scenarios::single_mask_triangle();
    DataTypeVariant variant = mask_data;
    REQUIRE(operation.canApply(variant));
}

TEST_CASE("MaskCentroidOperation - Cannot apply to null MaskData", "[mask][centroid][operation]") {
    MaskCentroidOperation operation;
    std::shared_ptr<MaskData> null_mask;
    DataTypeVariant variant = null_mask;
    REQUIRE_FALSE(operation.canApply(variant));
}

TEST_CASE("MaskCentroidOperation - Execute operation",
          "[mask][centroid][operation][scenario]") {
    MaskCentroidOperation operation;
    auto mask_data = mask_scenarios::operation_execute_test_centroid();

    DataTypeVariant input_variant = mask_data;
    auto params = operation.getDefaultParameters();

    auto result_variant = operation.execute(input_variant, params.get());

    REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));

    auto result = std::get<std::shared_ptr<PointData>>(result_variant);
    REQUIRE(result != nullptr);

    auto const & points = result->getAtTime(TimeFrameIndex(50));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

// ============================================================================
// JSON Pipeline Tests
// ============================================================================

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Centroid - JSON pipeline",
          "[transforms][mask_centroid][json][scenario]") {
    // Create DataManager with time frame
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto mask_data = mask_scenarios::json_pipeline_basic_centroid();
    dm->setData("json_pipeline_basic", mask_data, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Centroid Pipeline\",\n"
        "            \"description\": \"Test mask centroid calculation on mask data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Mask Centroid\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_basic\",\n"
        "                \"output_key\": \"mask_centroids\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_centroid_pipeline_test";
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
    auto result_centroids = dm->getData<PointData>("mask_centroids");
    REQUIRE(result_centroids != nullptr);
    
    // Verify the centroid calculation results
    auto const & times = result_centroids->getTimesWithData();
    REQUIRE(times.size() == 3); // Three timestamps
    
    // Check timestamp 100: Triangle centroid should be (1,1)
    auto const & points100 = result_centroids->getAtTime(TimeFrameIndex(100));
    REQUIRE(points100.size() == 1);
    REQUIRE_THAT(points100[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points100[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    
    // Check timestamp 200: Square centroid should be (2,2)
    auto const & points200 = result_centroids->getAtTime(TimeFrameIndex(200));
    REQUIRE(points200.size() == 1);
    REQUIRE_THAT(points200[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points200[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    
    // Check timestamp 300: Two centroids from two masks
    auto const & points300_view = result_centroids->getAtTime(TimeFrameIndex(300));
    std::vector<Point2D<float>> points300(points300_view.begin(), points300_view.end());
    REQUIRE(points300.size() == 2);
    
    // Sort points by x coordinate for consistent testing
    auto sorted_points = points300;
    std::sort(sorted_points.begin(), sorted_points.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });
    
    // First centroid: (0+2+0+2)/4 = 1, (0+0+2+2)/4 = 1
    REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    
    // Second centroid: (5+7+5+7)/4 = 6, (5+5+7+7)/4 = 6
    REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(6.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Mask Centroid - Complex JSON pipeline",
          "[transforms][mask_centroid][json][scenario]") {
    // Create DataManager with time frame
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);
    
    // Add test data from scenario
    auto mask_data = mask_scenarios::json_pipeline_basic_centroid();
    dm->setData("json_pipeline_basic", mask_data, TimeKey("default"));
    
    const char* json_config_complex = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Complex Mask Centroid Pipeline\",\n"
        "            \"description\": \"Test mask centroid calculation with complex masks\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Mask Centroid\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"json_pipeline_basic\",\n"
        "                \"output_key\": \"complex_centroids\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_centroid_complex_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath_complex = test_dir / "pipeline_config_complex.json";
    {
        std::ofstream json_file(json_filepath_complex);
        REQUIRE(json_file.is_open());
        json_file << json_config_complex;
        json_file.close();
    }
    
    // Execute the complex pipeline
    auto data_info_list_complex = load_data_from_json_config(dm.get(), json_filepath_complex.string());
    
    // Verify the complex results (should be identical to the first test since same input)
    auto result_centroids_complex = dm->getData<PointData>("complex_centroids");
    REQUIRE(result_centroids_complex != nullptr);
    
    auto const & times_complex = result_centroids_complex->getTimesWithData();
    REQUIRE(times_complex.size() == 3);
    
    // Verify one specific result to ensure the pipeline worked
    auto const & points100_complex = result_centroids_complex->getAtTime(TimeFrameIndex(100));
    REQUIRE(points100_complex.size() == 1);
    REQUIRE_THAT(points100_complex[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points100_complex[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}