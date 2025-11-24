#include "transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "fixtures/LinePointDistanceTestFixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace WhiskerToolbox::Testing;

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


TEST_CASE("Line to point minimum distance calculation - Core functionality", "[line][point][distance][transform]") {

    SECTION("Basic distance calculation between a line and a point") {
        HorizontalLineWithPointAbove fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
        REQUIRE(getTimeAtIndex(result.get(), 0) == fixture.timestamp);
    }

    SECTION("Multiple points with different distances") {
        VerticalLineWithMultiplePoints fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
        REQUIRE(getTimeAtIndex(result.get(), 0) == fixture.timestamp);
    }

    SECTION("Multiple timesteps with lines and points") {
        MultipleTimesteps fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == fixture.expected_num_results);

        // Find the indices for both timestamps
        size_t idx30 = 0;
        size_t idx40 = 1;
        if (getTimeAtIndex(result.get(), 0) == fixture.timestamp2) {
            idx30 = 1;
            idx40 = 0;
        }

        REQUIRE(getTimeAtIndex(result.get(), idx30) == fixture.timestamp1);
        REQUIRE_THAT(getValueAtIndex(result.get(), idx30), 
            Catch::Matchers::WithinAbs(fixture.expected_distance1, 0.001f));

        REQUIRE(getTimeAtIndex(result.get(), idx40) == fixture.timestamp2);
        REQUIRE_THAT(getValueAtIndex(result.get(), idx40), 
            Catch::Matchers::WithinAbs(fixture.expected_distance2, 0.001f));
    }

    SECTION("Scaling points with different image sizes") {
        CoordinateScaling fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
    }

    SECTION("Point directly on the line has zero distance") {
        PointOnLine fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
    }
}

TEST_CASE("Line to point minimum distance calculation - Edge cases and error handling", "[line][point][distance][edge]") {

    SECTION("Empty line data results in empty output") {
        EmptyLineData fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == fixture.expected_num_results);
    }

    SECTION("Empty point data results in empty output") {
        EmptyPointData fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == fixture.expected_num_results);
    }

    SECTION("No matching timestamps between line and point data") {
        NoMatchingTimestamps fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == fixture.expected_num_results);
    }

    SECTION("Line with only one point (invalid)") {
        InvalidLineOnePoint fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == fixture.expected_num_results);
    }

    SECTION("Invalid image sizes") {
        InvalidImageSizes fixture;
        
        auto result = line_min_point_dist(fixture.line_data.get(), fixture.point_data.get());

        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(getValueAtIndex(result.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
    }

    SECTION("Transform operation with null point data in parameters") {
        HorizontalLineWithPointAbove fixture;
        
        LineMinPointDistOperation operation;
        DataTypeVariant line_variant = fixture.line_data;
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = nullptr; // Null point data

        DataTypeVariant result = operation.execute(line_variant, params.get());

        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }

    SECTION("Transform operation with incorrect input type") {
        HorizontalLineWithPointAbove fixture;
        
        LineMinPointDistOperation operation;
        DataTypeVariant point_variant = fixture.point_data; // Wrong input type
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = fixture.point_data;

        DataTypeVariant result = operation.execute(point_variant, params.get());

        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }
}

TEST_CASE("Data Transform: Line Min Point Dist - JSON pipeline", "[transforms][line_min_point_dist][json]") {
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    SECTION("Two timesteps pipeline") {
        JsonPipelineTwoTimesteps fixture;
        
        // Store data in DataManager
        dm.setData("test_line", fixture.line_data, TimeKey("default"));
        dm.setData("test_points", fixture.point_data, TimeKey("default"));
        
        // Create JSON configuration
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
        REQUIRE(result_distances->getNumSamples() == fixture.expected_num_results);
        
        // Find indices for both timestamps
        size_t idx100 = 0;
        size_t idx200 = 1;
        if (getTimeAtIndex(result_distances.get(), 0) == fixture.timestamp2) {
            idx100 = 1;
            idx200 = 0;
        }
        
        REQUIRE(getTimeAtIndex(result_distances.get(), idx100) == fixture.timestamp1);
        REQUIRE_THAT(getValueAtIndex(result_distances.get(), idx100), 
            Catch::Matchers::WithinAbs(fixture.expected_distance1, 0.001f));
        
        REQUIRE(getTimeAtIndex(result_distances.get(), idx200) == fixture.timestamp2);
        REQUIRE_THAT(getValueAtIndex(result_distances.get(), idx200), 
            Catch::Matchers::WithinAbs(fixture.expected_distance2, 0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Pipeline with coordinate scaling") {
        JsonPipelineScaling fixture;
        
        dm.setData("test_line_scaled", fixture.line_data, TimeKey("default"));
        dm.setData("test_points_scaled", fixture.point_data, TimeKey("default"));
        
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
        REQUIRE(getTimeAtIndex(result_distances_scaled.get(), 0) == fixture.timestamp);
        REQUIRE_THAT(getValueAtIndex(result_distances_scaled.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Point on line edge case") {
        JsonPipelinePointOnLine fixture;
        
        dm.setData("test_line_on", fixture.line_data, TimeKey("default"));
        dm.setData("test_points_on", fixture.point_data, TimeKey("default"));
        
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
        REQUIRE(getTimeAtIndex(result_distances_on_line.get(), 0) == fixture.timestamp);
        REQUIRE_THAT(getValueAtIndex(result_distances_on_line.get(), 0), 
            Catch::Matchers::WithinAbs(fixture.expected_distance, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
