#include "transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "fixtures/scenarios/line/distance_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iostream>

// Helper function to get value at a specific index using the public getAllSamples() interface
static float getValueAtIndex(const AnalogTimeSeries* series, size_t index) {
    size_t i = 0;
    for (auto const& sample : series->getAllSamples()) {
        if (i == index) {
            return sample.value;
        }
        ++i;
    }
    throw std::out_of_range("Index out of range");
}

// Helper function to get TimeFrameIndex at a specific index using the public getAllSamples() interface
static TimeFrameIndex getTimeAtIndex(const AnalogTimeSeries* series, size_t index) {
    size_t i = 0;
    for (auto const& sample : series->getAllSamples()) {
        if (i == index) {
            return sample.time_frame_index;
        }
        ++i;
    }
    throw std::out_of_range("Index out of range");
}

// ============================================================================
// Tests: Core Functionality (using fixture)
// ============================================================================

TEST_CASE("V1 Transform: Line Min Point Dist - Core Functionality",
          "[transforms][v1][line_min_point_dist]") {

    SECTION("Basic distance calculation - horizontal line with point above") {
        auto [line_data, point_data] = line_distance_scenarios::horizontal_line_point_above();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(5.0f, 0.001f));
        REQUIRE(getTimeAtIndex(result.get(), 0) == TimeFrameIndex(10));
    }

    SECTION("Multiple points with different distances - finds minimum") {
        auto [line_data, point_data] = line_distance_scenarios::vertical_line_multiple_points();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        // Minimum distance is 1.0 (from point at (6,8) to line at x=5)
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE(getTimeAtIndex(result.get(), 0) == TimeFrameIndex(20));
    }

    SECTION("Multiple timesteps with lines and points") {
        auto [line_data, point_data] = line_distance_scenarios::multiple_timesteps();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Should have 2 results (t=30 and t=40, not t=50 which has no line)
        REQUIRE(result->getNumSamples() == 2);

        // Find the indices for both timestamps
        size_t idx30 = 0;
        size_t idx40 = 1;
        if (getTimeAtIndex(result.get(), 0) == TimeFrameIndex(40)) {
            idx30 = 1;
            idx40 = 0;
        }

        REQUIRE(getTimeAtIndex(result.get(), idx30) == TimeFrameIndex(30));
        REQUIRE_THAT(getValueAtIndex(result.get(), idx30), 
            Catch::Matchers::WithinAbs(2.0f, 0.001f));

        REQUIRE(getTimeAtIndex(result.get(), idx40) == TimeFrameIndex(40));
        REQUIRE_THAT(getValueAtIndex(result.get(), idx40), 
            Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Scaling points with different image sizes") {
        auto [line_data, point_data] = line_distance_scenarios::coordinate_scaling();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        // Point (25,10) in 50x50 space -> (50,20) in 100x100 space
        // Distance from (50,20) to horizontal line at y=0 is 20.0
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(20.0f, 0.001f));
    }

    SECTION("Point directly on the line has zero distance") {
        auto [line_data, point_data] = line_distance_scenarios::point_on_line();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Edge Cases (using fixture)
// ============================================================================

TEST_CASE("V1 Transform: Line Min Point Dist - Edge Cases",
          "[transforms][v1][line_min_point_dist][edge]") {

    SECTION("Empty line data results in empty output") {
        auto [line_data, point_data] = line_distance_scenarios::empty_line_data();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Empty point data results in empty output") {
        auto [line_data, point_data] = line_distance_scenarios::empty_point_data();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("No matching timestamps between line and point data") {
        auto [line_data, point_data] = line_distance_scenarios::no_matching_timestamps();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Line with only one point (invalid)") {
        auto [line_data, point_data] = line_distance_scenarios::invalid_line_one_point();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Invalid line should produce no results
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Invalid image sizes fallback") {
        auto [line_data, point_data] = line_distance_scenarios::invalid_image_sizes();
        
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        // With invalid image sizes, should use original coordinates (5.0 distance)
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(5.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Transform Operation (non-fixture tests)
// ============================================================================

TEST_CASE("V1 Transform: Line Min Point Dist - Operation Edge Cases",
          "[transforms][v1][line_min_point_dist][operation]") {

    SECTION("Transform operation with null point data in parameters") {
        // Create minimal test data inline
        auto line_data = std::make_shared<LineData>();
        auto time_frame = std::make_shared<TimeFrame>();
        line_data->setTimeFrame(time_frame);
        line_data->emplaceAtTime(TimeFrameIndex(10), 
            std::vector<float>{0.0f, 10.0f}, 
            std::vector<float>{0.0f, 0.0f});
        
        LineMinPointDistOperation operation;
        DataTypeVariant line_variant = line_data;
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = nullptr; // Null point data

        DataTypeVariant result = operation.execute(line_variant, params.get());

        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }

    SECTION("Transform operation with incorrect input type") {
        // Create minimal test data inline
        auto point_data = std::make_shared<PointData>();
        auto time_frame = std::make_shared<TimeFrame>();
        point_data->setTimeFrame(time_frame);
        point_data->addAtTime(TimeFrameIndex(10), 
            std::vector<Point2D<float>>{Point2D<float>{5.0f, 5.0f}}, 
            NotifyObservers::No);
        
        LineMinPointDistOperation operation;
        DataTypeVariant point_variant = point_data; // Wrong input type
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = point_data;

        DataTypeVariant result = operation.execute(point_variant, params.get());

        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }
}

// ============================================================================
// Tests: JSON Pipeline (using fixture)
// ============================================================================

TEST_CASE("V1 Transform: Line Min Point Dist - JSON Pipeline",
          "[transforms][v1][line_min_point_dist][json]") {
    
    DataManager dm;
    
    // Setup test data using scenarios
    auto [line_data_two_ts, point_data_two_ts] = line_distance_scenarios::json_pipeline_two_timesteps();
    dm.setData("json_pipeline_two_timesteps_line", line_data_two_ts, TimeKey("default"));
    dm.setData("json_pipeline_two_timesteps_point", point_data_two_ts, TimeKey("default"));
    
    auto [line_data_scaling, point_data_scaling] = line_distance_scenarios::json_pipeline_scaling();
    dm.setData("json_pipeline_scaling_line", line_data_scaling, TimeKey("default"));
    dm.setData("json_pipeline_scaling_point", point_data_scaling, TimeKey("default"));
    
    auto [line_data_on_line, point_data_on_line] = line_distance_scenarios::json_pipeline_point_on_line();
    dm.setData("json_pipeline_point_on_line_line", line_data_on_line, TimeKey("default"));
    dm.setData("json_pipeline_point_on_line_point", point_data_on_line, TimeKey("default"));
    
    SECTION("Two timesteps pipeline") {
        // Use data from scenarios (already in DataManager)
        
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
            "                \"input_key\": \"json_pipeline_two_timesteps_line\",\n"
            "                \"output_key\": \"line_point_distances\",\n"
            "                \"parameters\": {\n"
            "                    \"point_data\": \"json_pipeline_two_timesteps_point\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        // Write JSON to file
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_dist_pipeline_test";
        std::filesystem::create_directories(test_dir);
        std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Execute pipeline
        auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
        
        // Verify results
        auto result_distances = dm.getData<AnalogTimeSeries>("line_point_distances");
        REQUIRE(result_distances != nullptr);
        REQUIRE(result_distances->getNumSamples() == 2);
        
        // Find indices for both timestamps
        size_t idx100 = 0;
        size_t idx200 = 1;
        if (getTimeAtIndex(result_distances.get(), 0) == TimeFrameIndex(200)) {
            idx100 = 1;
            idx200 = 0;
        }
        
        REQUIRE(getTimeAtIndex(result_distances.get(), idx100) == TimeFrameIndex(100));
        REQUIRE_THAT(getValueAtIndex(result_distances.get(), idx100), 
            Catch::Matchers::WithinAbs(5.0f, 0.001f));
        
        REQUIRE(getTimeAtIndex(result_distances.get(), idx200) == TimeFrameIndex(200));
        REQUIRE_THAT(getValueAtIndex(result_distances.get(), idx200), 
            Catch::Matchers::WithinAbs(3.0f, 0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Pipeline with coordinate scaling") {
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
            "                \"input_key\": \"json_pipeline_scaling_line\",\n"
            "                \"output_key\": \"line_point_distances_scaled\",\n"
            "                \"parameters\": {\n"
            "                    \"point_data\": \"json_pipeline_scaling_point\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_dist_pipeline_test";
        std::filesystem::create_directories(test_dir);
        std::filesystem::path json_filepath_scaled = test_dir / "pipeline_config_scaled.json";
        {
            std::ofstream json_file(json_filepath_scaled);
            REQUIRE(json_file.is_open());
            json_file << json_config_scaled;
        }
        
        auto data_info_list_scaled = load_data_from_json_config(&dm, json_filepath_scaled.string());
        
        auto result_distances_scaled = dm.getData<AnalogTimeSeries>("line_point_distances_scaled");
        REQUIRE(result_distances_scaled != nullptr);
        REQUIRE(result_distances_scaled->getNumSamples() == 1);
        REQUIRE(getTimeAtIndex(result_distances_scaled.get(), 0) == TimeFrameIndex(300));
        REQUIRE_THAT(getValueAtIndex(result_distances_scaled.get(), 0), 
            Catch::Matchers::WithinAbs(20.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Point on line edge case") {
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
            "                \"input_key\": \"json_pipeline_point_on_line_line\",\n"
            "                \"output_key\": \"line_point_distances_on_line\",\n"
            "                \"parameters\": {\n"
            "                    \"point_data\": \"json_pipeline_point_on_line_point\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";
        
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_dist_pipeline_test";
        std::filesystem::create_directories(test_dir);
        std::filesystem::path json_filepath_on_line = test_dir / "pipeline_config_on_line.json";
        {
            std::ofstream json_file(json_filepath_on_line);
            REQUIRE(json_file.is_open());
            json_file << json_config_on_line;
        }
        
        auto data_info_list_on_line = load_data_from_json_config(&dm, json_filepath_on_line.string());
        
        auto result_distances_on_line = dm.getData<AnalogTimeSeries>("line_point_distances_on_line");
        REQUIRE(result_distances_on_line != nullptr);
        REQUIRE(result_distances_on_line->getNumSamples() == 1);
        REQUIRE(getTimeAtIndex(result_distances_on_line.get(), 0) == TimeFrameIndex(400));
        REQUIRE_THAT(getValueAtIndex(result_distances_on_line.get(), 0), 
            Catch::Matchers::WithinAbs(0.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
