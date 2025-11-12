#include "transforms/Masks/Mask_Centroid/mask_centroid.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

TEST_CASE("Mask centroid calculation - Core functionality", "[mask][centroid][transform]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating centroid from empty mask data") {
        auto result = calculate_mask_centroid(mask_data.get());

        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Calculating centroid from single mask at one timestamp") {
        // Create a simple mask (3 points forming a triangle)
        std::vector<uint32_t> x_coords = {0, 3, 0};
        std::vector<uint32_t> y_coords = {0, 0, 3};
        mask_data->addAtTime(TimeFrameIndex(10), x_coords, y_coords, NotifyObservers::No);

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

    SECTION("Calculating centroid from multiple masks at one timestamp") {
        // First mask (simple square)
        std::vector<uint32_t> x1 = {0, 1, 0, 1};
        std::vector<uint32_t> y1 = {0, 0, 1, 1};
        mask_data->addAtTime(TimeFrameIndex(20), x1, y1, NotifyObservers::No);

        // Second mask (offset square)
        std::vector<uint32_t> x2 = {4, 5, 4, 5};
        std::vector<uint32_t> y2 = {4, 4, 5, 5};
        mask_data->addAtTime(TimeFrameIndex(20), x2, y2, NotifyObservers::No);

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

    SECTION("Calculating centroids from masks across multiple timestamps") {
        // Timestamp 30: One mask (line of 3 points)
        std::vector<uint32_t> x1 = {0, 2, 4};
        std::vector<uint32_t> y1 = {0, 0, 0};
        mask_data->addAtTime(TimeFrameIndex(30), x1, y1, NotifyObservers::No);

        // Timestamp 40: One mask (vertical line)
        std::vector<uint32_t> x2 = {1, 1, 1};
        std::vector<uint32_t> y2 = {0, 3, 6};
        mask_data->addAtTime(TimeFrameIndex(40), x2, y2, NotifyObservers::No);

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

    SECTION("Verify image size is preserved") {
        // Set image size on mask data
        ImageSize test_size{640, 480};
        mask_data->setImageSize(test_size);

        // Add some mask data
        std::vector<uint32_t> x = {100, 200, 300};
        std::vector<uint32_t> y = {100, 150, 200};
        mask_data->addAtTime(TimeFrameIndex(100), x, y, NotifyObservers::No);

        auto result = calculate_mask_centroid(mask_data.get());

        // Verify image size is copied
        REQUIRE(result->getImageSize().width == test_size.width);
        REQUIRE(result->getImageSize().height == test_size.height);

        // Verify calculation is correct: (100+200+300)/3 = 200, (100+150+200)/3 = 150
        auto const & points = result->getAtTime(TimeFrameIndex(100));
        REQUIRE(points.size() == 1);
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(200.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
    }
}

TEST_CASE("Mask centroid calculation - Edge cases and error handling", "[mask][centroid][transform][edge]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Masks with zero points") {
        // Add an empty mask (should be handled gracefully)
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(TimeFrameIndex(10), empty_x, empty_y, NotifyObservers::No);

        auto result = calculate_mask_centroid(mask_data.get());

        // Empty masks should be skipped
        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Mixed empty and non-empty masks") {
        // Add an empty mask
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(TimeFrameIndex(20), empty_x, empty_y, NotifyObservers::No);

        // Add a non-empty mask at the same timestamp
        std::vector<uint32_t> x = {2, 4};
        std::vector<uint32_t> y = {1, 3};
        mask_data->addAtTime(TimeFrameIndex(20), x, y, NotifyObservers::No);

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

    SECTION("Single point masks") {
        // Each mask has only one point
        std::vector<uint32_t> x1 = {5};
        std::vector<uint32_t> y1 = {7};
        mask_data->addAtTime(TimeFrameIndex(30), x1, y1, NotifyObservers::No);

        std::vector<uint32_t> x2 = {10};
        std::vector<uint32_t> y2 = {15};
        mask_data->addAtTime(TimeFrameIndex(30), x2, y2, NotifyObservers::No);
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

    SECTION("Large coordinates") {
        // Test with large coordinate values to ensure no overflow
        std::vector<uint32_t> x = {1000000, 1000001, 1000002};
        std::vector<uint32_t> y = {2000000, 2000001, 2000002};
        mask_data->addAtTime(TimeFrameIndex(40), x, y, NotifyObservers::No);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & points = result->getAtTime(TimeFrameIndex(40));
        REQUIRE(points.size() == 1);

        // Centroid: (1000000+1000001+1000002)/3 = 1000001, (2000000+2000001+2000002)/3 = 2000001
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1000001.0f, 0.1f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2000001.0f, 0.1f));
    }

    SECTION("Null input handling") {
        auto result = calculate_mask_centroid(nullptr);
        REQUIRE(result->getTimesWithData().empty());
    }
}

TEST_CASE("MaskCentroidOperation - Operation interface", "[mask][centroid][operation]") {
    MaskCentroidOperation operation;

    SECTION("Operation name") {
        REQUIRE(operation.getName() == "Calculate Mask Centroid");
    }

    SECTION("Target type index") {
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
    }

    SECTION("Can apply to valid MaskData") {
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant variant = mask_data;
        REQUIRE(operation.canApply(variant));
    }

    SECTION("Cannot apply to null MaskData") {
        std::shared_ptr<MaskData> null_mask;
        DataTypeVariant variant = null_mask;
        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params != nullptr);
        REQUIRE(dynamic_cast<MaskCentroidParameters *>(params.get()) != nullptr);
    }

    SECTION("Execute operation") {
        auto mask_data = std::make_shared<MaskData>();

        // Add test data
        std::vector<uint32_t> x = {0, 2, 4};
        std::vector<uint32_t> y = {0, 0, 0};
        mask_data->addAtTime(TimeFrameIndex(50), x, y, NotifyObservers::No);

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
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Mask Centroid - JSON pipeline", "[transforms][mask_centroid][json]") {
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code
    auto test_mask = std::make_shared<MaskData>();
    test_mask->setTimeFrame(time_frame);
    
    // Add mask data at different timestamps
    // Timestamp 100: Triangle mask
    std::vector<uint32_t> x1 = {0, 3, 0};
    std::vector<uint32_t> y1 = {0, 0, 3};
    test_mask->addAtTime(TimeFrameIndex(100), x1, y1, NotifyObservers::No);
    
    // Timestamp 200: Square mask
    std::vector<uint32_t> x2 = {1, 3, 1, 3};
    std::vector<uint32_t> y2 = {1, 1, 3, 3};
    test_mask->addAtTime(TimeFrameIndex(200), x2, y2, NotifyObservers::No);
    
    // Timestamp 300: Multiple masks
    std::vector<uint32_t> x3a = {0, 2, 0, 2};
    std::vector<uint32_t> y3a = {0, 0, 2, 2};
    test_mask->addAtTime(TimeFrameIndex(300), x3a, y3a, NotifyObservers::No);
    
    std::vector<uint32_t> x3b = {5, 7, 5, 7};
    std::vector<uint32_t> y3b = {5, 5, 7, 7};
    test_mask->addAtTime(TimeFrameIndex(300), x3b, y3b, NotifyObservers::No);
    // Store the mask data in DataManager with a known key
    dm.setData("test_masks", test_mask, TimeKey("default"));
    
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
        "                \"input_key\": \"test_masks\",\n"
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
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_centroids = dm.getData<PointData>("mask_centroids");
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
    
    // Test another pipeline with different mask data
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
        "                \"input_key\": \"test_masks\",\n"
        "                \"output_key\": \"complex_centroids\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_complex = test_dir / "pipeline_config_complex.json";
    {
        std::ofstream json_file(json_filepath_complex);
        REQUIRE(json_file.is_open());
        json_file << json_config_complex;
        json_file.close();
    }
    
    // Execute the complex pipeline
    auto data_info_list_complex = load_data_from_json_config(&dm, json_filepath_complex.string());
    
    // Verify the complex results (should be identical to the first test since same input)
    auto result_centroids_complex = dm.getData<PointData>("complex_centroids");
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