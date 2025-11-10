#include "transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Line to point minimum distance calculation - Core functionality", "[line][point][distance][transform]") {
    auto line_data = std::make_shared<LineData>();
    auto point_data = std::make_shared<PointData>();

    SECTION("Basic distance calculation between a line and a point") {
        // Create a simple horizontal line from (0,0) to (10,0)
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 10;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create a point at (5, 5) - should be 5 units away from the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(5.0f, 0.001f));
        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(timestamp));
    }

    SECTION("Multiple points with different distances") {
        // Create a simple vertical line from (5,0) to (5,10)
        std::vector<float> line_x = {5.0f, 5.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        int timestamp = 20;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create multiple points at different distances from the line
        std::vector<Point2D<float>> points = {
                Point2D<float>{0.0f, 5.0f},    // 5 units away
                Point2D<float>{8.0f, 5.0f},    // 3 units away
                Point2D<float>{5.0f, 15.0f},   // 5 units away
                Point2D<float>{6.0f, 8.0f}     // 1 unit away
        };
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result - minimum distance should be 1.0
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(timestamp));
    }

    SECTION("Multiple timesteps with lines and points") {
        // Create lines at different timestamps
        // Timestamp 30: Line from (0,0) to (10,0)
        std::vector<float> line_x1 = {0.0f, 10.0f};
        std::vector<float> line_y1 = {0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(30), line_x1, line_y1);

        // Timestamp 40: Line from (0,0) to (0,10)
        std::vector<float> line_x2 = {0.0f, 0.0f};
        std::vector<float> line_y2 = {0.0f, 10.0f};
        line_data->emplaceAtTime(TimeFrameIndex(40), line_x2, line_y2);

        // Create points at different timestamps
        // Timestamp 30: Point at (5,2) - should be 2 units away
        std::vector<Point2D<float>> points1 = {Point2D<float>{5.0f, 2.0f}};
        point_data->addAtTime(TimeFrameIndex(30), points1);

        // Timestamp 40: Point at (3,5) - should be 3 units away
        std::vector<Point2D<float>> points2 = {Point2D<float>{3.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(40), points2);

        // Timestamp 50: No line data - should be skipped
        std::vector<Point2D<float>> points3 = {Point2D<float>{1.0f, 1.0f}};
        point_data->addAtTime(TimeFrameIndex(50), points3);

        // Calculate the minimum distances
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 2); // Only timesteps 30 and 40 should have results

        // Find the indices for both timestamps
        size_t idx30 = 0;
        size_t idx40 = 1;
        if (result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(40)) {
            idx30 = 1;
            idx40 = 0;
        }

        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx30)) == TimeFrameIndex(30));
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(idx30)), Catch::Matchers::WithinAbs(2.0f, 0.001f));

        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx40)) == TimeFrameIndex(40));
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(idx40)), Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Scaling points with different image sizes") {
        // Set different image sizes for line and point data
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50}); // Half the resolution

        // Create a line from (0,0) to (100,0) in the line's coordinate space
        std::vector<float> line_x = {0.0f, 100.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 60;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create a point at (25, 10) in the point's coordinate space
        // After scaling to line coordinates, this should be at (50, 20)
        // Distance to the line should be 20 units
        std::vector<Point2D<float>> points = {Point2D<float>{25.0f, 10.0f}};
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance with scaling
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(20.0f, 0.001f));
    }

    SECTION("Point directly on the line has zero distance") {
        // Create a diagonal line from (0,0) to (10,10)
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        int timestamp = 70;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create a point at (5, 5) - exactly on the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("Line to point minimum distance calculation - Edge cases and error handling", "[line][point][distance][edge]") {
    auto line_data = std::make_shared<LineData>();
    auto point_data = std::make_shared<PointData>();

    SECTION("Empty line data results in empty output") {
        // Create only point data
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(10), points);

        // Calculate the minimum distance with no line data
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Empty point data results in empty output") {
        // Create only line data
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(10), line_x, line_y);

        // Calculate the minimum distance with no point data
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("No matching timestamps between line and point data") {
        // Line at timestamp 20
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(20), line_x, line_y);

        // Point at timestamp 30 (different from line)
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(30), points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Line with only one point (invalid)") {
        // Create a "line" with only one point
        std::vector<float> line_x = {5.0f};
        std::vector<float> line_y = {5.0f};
        int timestamp = 40;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create a point
        std::vector<Point2D<float>> points = {Point2D<float>{10.0f, 10.0f}};
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify no result is produced for this timestamp
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Invalid image sizes") {
        // Set invalid image sizes
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{-1, -1}); // Invalid

        // Create a line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 50;
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), line_x, line_y);

        // Create a point
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->addAtTime(TimeFrameIndex(timestamp), points);

        // Calculate the minimum distance - should use original point coordinates
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is calculated without scaling
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(5.0f, 0.001f));
    }

    SECTION("Transform operation with null point data in parameters") {
        // Create a line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(60), line_x, line_y);

        // Setup the operation and parameters
        LineMinPointDistOperation operation;
        DataTypeVariant line_variant = line_data;
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = nullptr; // Null point data

        // Execute the operation
        DataTypeVariant result = operation.execute(line_variant, params.get());

        // Verify the operation returns an empty variant when point_data is null
        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }

    SECTION("Transform operation with incorrect input type") {
        // Setup the operation with wrong input type (point data instead of line data)
        LineMinPointDistOperation operation;
        DataTypeVariant point_variant = point_data; // Wrong input type
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = point_data;

        // Execute the operation
        DataTypeVariant result = operation.execute(point_variant, params.get());

        // Verify the operation returns an empty variant for wrong input
        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }
}

TEST_CASE("Data Transform: Line Min Point Dist - JSON pipeline", "[transforms][line_min_point_dist][json]") {
    // Create DataManager and populate it with LineData and PointData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data in code
    auto line_data = std::make_shared<LineData>();
    line_data->setTimeFrame(time_frame);
    
    // Create a horizontal line from (0,0) to (10,0) at timestamp 100
    std::vector<float> line_x = {0.0f, 10.0f};
    std::vector<float> line_y = {0.0f, 0.0f};
    line_data->emplaceAtTime(TimeFrameIndex(100), line_x, line_y);
    
    // Create a vertical line from (5,0) to (5,10) at timestamp 200
    std::vector<float> line_x2 = {5.0f, 5.0f};
    std::vector<float> line_y2 = {0.0f, 10.0f};
    line_data->emplaceAtTime(TimeFrameIndex(200), line_x2, line_y2);
    
    // Store the line data in DataManager with a known key
    dm.setData("test_line", line_data, TimeKey("default"));
    
    // Create test point data in code
    auto point_data = std::make_shared<PointData>();
    point_data->setTimeFrame(time_frame);
    
    // Create points at timestamp 100: (5,5) - should be 5 units away from horizontal line
    std::vector<Point2D<float>> points1 = {Point2D<float>{5.0f, 5.0f}};
    point_data->addAtTime(TimeFrameIndex(100), points1);
    
    // Create points at timestamp 200: (8,5) - should be 3 units away from vertical line
    std::vector<Point2D<float>> points2 = {Point2D<float>{8.0f, 5.0f}};
    point_data->addAtTime(TimeFrameIndex(200), points2);
    
    // Store the point data in DataManager with a known key
    dm.setData("test_points", point_data, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line to Point Distance Pipeline\",\n"
        "            \"description\": \"Test line to point minimum distance calculation\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line to Point Distance\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_point_distances\",\n"
        "                \"parameters\": {\n"
        "                    \"point_data\": \"test_points\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_dist_pipeline_test";
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
    auto result_distances = dm.getData<AnalogTimeSeries>("line_point_distances");
    REQUIRE(result_distances != nullptr);
    
    // Verify the distance calculation results
    REQUIRE(result_distances->getNumSamples() == 2); // Two timestamps
    
    // Find the indices for both timestamps
    size_t idx100 = 0;
    size_t idx200 = 1;
    if (result_distances->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(200)) {
        idx100 = 1;
        idx200 = 0;
    }
    
    // Check timestamp 100: point (5,5) to horizontal line (0,0)-(10,0) = 5.0 units
    REQUIRE(result_distances->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx100)) == TimeFrameIndex(100));
    REQUIRE_THAT(result_distances->getDataAtDataArrayIndex(DataArrayIndex(idx100)), Catch::Matchers::WithinAbs(5.0f, 0.001f));
    
    // Check timestamp 200: point (8,5) to vertical line (5,0)-(5,10) = 3.0 units
    REQUIRE(result_distances->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx200)) == TimeFrameIndex(200));
    REQUIRE_THAT(result_distances->getDataAtDataArrayIndex(DataArrayIndex(idx200)), Catch::Matchers::WithinAbs(3.0f, 0.001f));
    
    // Test another pipeline with different line and point data (scaling test)
    auto line_data_scaled = std::make_shared<LineData>();
    line_data_scaled->setTimeFrame(time_frame);
    line_data_scaled->setImageSize(ImageSize{100, 100}); // Higher resolution
    
    // Create a line from (0,0) to (100,0) in the line's coordinate space
    std::vector<float> line_x_scaled = {0.0f, 100.0f};
    std::vector<float> line_y_scaled = {0.0f, 0.0f};
    line_data_scaled->emplaceAtTime(TimeFrameIndex(300), line_x_scaled, line_y_scaled);
    
    dm.setData("test_line_scaled", line_data_scaled, TimeKey("default"));
    
    auto point_data_scaled = std::make_shared<PointData>();
    point_data_scaled->setTimeFrame(time_frame);
    point_data_scaled->setImageSize(ImageSize{50, 50}); // Half the resolution
    
    // Create a point at (25, 10) in the point's coordinate space
    // After scaling to line coordinates, this should be at (50, 20)
    // Distance to the line should be 20 units
    std::vector<Point2D<float>> points_scaled = {Point2D<float>{25.0f, 10.0f}};
    point_data_scaled->addAtTime(TimeFrameIndex(300), points_scaled);
    
    dm.setData("test_points_scaled", point_data_scaled, TimeKey("default"));
    
    const char* json_config_scaled = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line to Point Distance with Scaling\",\n"
        "            \"description\": \"Test line to point distance with coordinate scaling\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line to Point Distance\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line_scaled\",\n"
        "                \"output_key\": \"line_point_distances_scaled\",\n"
        "                \"parameters\": {\n"
        "                    \"point_data\": \"test_points_scaled\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_scaled = test_dir / "pipeline_config_scaled.json";
    {
        std::ofstream json_file(json_filepath_scaled);
        REQUIRE(json_file.is_open());
        json_file << json_config_scaled;
        json_file.close();
    }
    
    // Execute the scaling pipeline
    auto data_info_list_scaled = load_data_from_json_config(&dm, json_filepath_scaled.string());
    
    // Verify the scaling results
    auto result_distances_scaled = dm.getData<AnalogTimeSeries>("line_point_distances_scaled");
    REQUIRE(result_distances_scaled != nullptr);
    
    REQUIRE(result_distances_scaled->getNumSamples() == 1);
    REQUIRE(result_distances_scaled->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(300));
    REQUIRE_THAT(result_distances_scaled->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(20.0f, 0.001f));
    
    // Test edge case: point directly on the line
    auto line_data_on_line = std::make_shared<LineData>();
    line_data_on_line->setTimeFrame(time_frame);
    
    // Create a diagonal line from (0,0) to (10,10)
    std::vector<float> line_x_on = {0.0f, 10.0f};
    std::vector<float> line_y_on = {0.0f, 10.0f};
    line_data_on_line->emplaceAtTime(TimeFrameIndex(400), line_x_on, line_y_on);
    
    dm.setData("test_line_on", line_data_on_line, TimeKey("default"));
    
    auto point_data_on_line = std::make_shared<PointData>();
    point_data_on_line->setTimeFrame(time_frame);
    
    // Create a point at (5, 5) - exactly on the line
    std::vector<Point2D<float>> points_on = {Point2D<float>{5.0f, 5.0f}};
    point_data_on_line->addAtTime(TimeFrameIndex(400), points_on);
    
    dm.setData("test_points_on", point_data_on_line, TimeKey("default"));
    
    const char* json_config_on_line = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line to Point Distance - Point on Line\",\n"
        "            \"description\": \"Test line to point distance when point is on the line\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line to Point Distance\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line_on\",\n"
        "                \"output_key\": \"line_point_distances_on_line\",\n"
        "                \"parameters\": {\n"
        "                    \"point_data\": \"test_points_on\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_on_line = test_dir / "pipeline_config_on_line.json";
    {
        std::ofstream json_file(json_filepath_on_line);
        REQUIRE(json_file.is_open());
        json_file << json_config_on_line;
        json_file.close();
    }
    
    // Execute the on-line pipeline
    auto data_info_list_on_line = load_data_from_json_config(&dm, json_filepath_on_line.string());
    
    // Verify the on-line results
    auto result_distances_on_line = dm.getData<AnalogTimeSeries>("line_point_distances_on_line");
    REQUIRE(result_distances_on_line != nullptr);
    
    REQUIRE(result_distances_on_line->getNumSamples() == 1);
    REQUIRE(result_distances_on_line->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(400));
    REQUIRE_THAT(result_distances_on_line->getDataAtDataArrayIndex(DataArrayIndex(0)), Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
