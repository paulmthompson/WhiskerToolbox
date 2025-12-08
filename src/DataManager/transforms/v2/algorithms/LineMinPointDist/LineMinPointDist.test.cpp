#include "LineMinPointDist.hpp"

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Ragged_Analog_Time_Series.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/distance_scenarios.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Registration: Uses singleton from RegisteredTransforms.cpp (compile-time)
// ============================================================================
// CalculateLineMinPointDistance is registered at compile-time via
// RegisterBinaryTransform RAII helper in RegisteredTransforms.cpp.
// The ElementRegistry::instance() singleton already has this transform
// available when tests run.

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const* line_data, TimeFrameIndex time) {
    auto const& sequence = line_data->getLineTimeSeries();
    for (auto const& [t, line] : sequence) {
        if (t == time) {
            return line;
        }
    }
    return Line2D{};
}

// Helper to extract points from PointData at a given time
static std::vector<Point2D<float>> getPointsAt(PointData const* point_data, TimeFrameIndex time) {
    return point_data->getAtTime(time);
}

// ============================================================================
// Tests: Algorithm Correctness (using fixture)
// ============================================================================

TEST_CASE("V2 Binary Element Transform: LineMinPointDist - Core Functionality",
          "[transforms][v2][binary_element][line_min_point_dist]") {
    
    LineMinPointDistParams params;
    
    SECTION("Basic distance calculation - horizontal line with point above") {
        auto [line_data, point_data] = line_distance_scenarios::horizontal_line_point_above();
        
        TimeFrameIndex timestamp(10);
        auto line = getLineAt(line_data.get(), timestamp);
        auto points = getPointsAt(point_data.get(), timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        REQUIRE_THAT(distance, WithinAbs(5.0f, 0.001f));
    }
    
    SECTION("Multiple points with different distances - finds minimum") {
        auto [line_data, point_data] = line_distance_scenarios::vertical_line_multiple_points();
        
        TimeFrameIndex timestamp(20);
        auto line = getLineAt(line_data.get(), timestamp);
        auto points = getPointsAt(point_data.get(), timestamp);
        
        // Calculate minimum distance across all points
        float min_distance = std::numeric_limits<float>::max();
        for (auto const& point : points) {
            float dist = calculateLineMinPointDistance(line, point, params);
            min_distance = std::min(min_distance, dist);
        }
        
        // Minimum distance is 1.0 (from point at (6,8) to line at x=5)
        REQUIRE_THAT(min_distance, WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Point directly on the line has zero distance") {
        auto [line_data, point_data] = line_distance_scenarios::point_on_line();
        
        TimeFrameIndex timestamp(70);
        auto line = getLineAt(line_data.get(), timestamp);
        auto points = getPointsAt(point_data.get(), timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        REQUIRE_THAT(distance, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Multiple timesteps with different distances") {
        auto [line_data, point_data] = line_distance_scenarios::multiple_timesteps();
        
        // Timestamp 30: horizontal line, point at (5,2), distance = 2.0
        {
            TimeFrameIndex timestamp(30);
            auto line = getLineAt(line_data.get(), timestamp);
            auto points = getPointsAt(point_data.get(), timestamp);
            
            REQUIRE(!line.empty());
            REQUIRE(!points.empty());
            
            float distance = calculateLineMinPointDistance(line, points[0], params);
            REQUIRE_THAT(distance, WithinAbs(2.0f, 0.001f));
        }
        
        // Timestamp 40: vertical line, point at (3,5), distance = 3.0
        {
            TimeFrameIndex timestamp(40);
            auto line = getLineAt(line_data.get(), timestamp);
            auto points = getPointsAt(point_data.get(), timestamp);
            
            REQUIRE(!line.empty());
            REQUIRE(!points.empty());
            
            float distance = calculateLineMinPointDistance(line, points[0], params);
            REQUIRE_THAT(distance, WithinAbs(3.0f, 0.001f));
        }
    }
}

TEST_CASE("V2 Binary Element Transform: LineMinPointDist - Edge Cases",
          "[transforms][v2][binary_element][line_min_point_dist]") {
    
    LineMinPointDistParams params;
    
    SECTION("Line with only one point (invalid) returns infinity") {
        auto [line_data, point_data] = line_distance_scenarios::invalid_line_one_point();
        
        TimeFrameIndex timestamp(40);
        auto line = getLineAt(line_data.get(), timestamp);
        auto points = getPointsAt(point_data.get(), timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        // Should return infinity for invalid line
        REQUIRE(std::isinf(distance));
    }
    
    SECTION("Empty line returns infinity") {
        Line2D empty_line;
        Point2D<float> point{5.0f, 5.0f};
        
        float distance = calculateLineMinPointDistance(empty_line, point, params);
        
        REQUIRE(std::isinf(distance));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Binary Element Transform: LineMinPointDistParams - JSON Loading",
          "[transforms][v2][params][json]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "use_first_line_only": false,
            "return_squared_distance": true
        })";
        
        auto result = loadParametersFromJson<LineMinPointDistParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getUseFirstLineOnly() == false);
        REQUIRE(params.getReturnSquaredDistance() == true);
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LineMinPointDistParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getUseFirstLineOnly() == true);
        REQUIRE(params.getReturnSquaredDistance() == false);
    }
    
    SECTION("JSON round-trip preserves values") {
        LineMinPointDistParams original;
        original.use_first_line_only = false;
        original.return_squared_distance = true;
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LineMinPointDistParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.getUseFirstLineOnly() == false);
        REQUIRE(recovered.getReturnSquaredDistance() == true);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Binary Element Transform: Registry Integration",
          "[transforms][v2][registry][binary_element]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("CalculateLineMinPointDistance"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("CalculateLineMinPointDistance");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "CalculateLineMinPointDistance");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == true);
        REQUIRE(metadata->input_arity == 2);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineMinPointDist via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_min_point_dist]") {
    
    DataManager dm;
    
    // Create TimeFrame for DataManager
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test data using scenarios
    auto [line_data_two_ts, point_data_two_ts] = line_distance_scenarios::json_pipeline_two_timesteps();
    line_data_two_ts->setTimeFrame(time_frame);
    point_data_two_ts->setTimeFrame(time_frame);
    dm.setData("json_pipeline_two_timesteps_line", line_data_two_ts, TimeKey("default"));
    dm.setData("json_pipeline_two_timesteps_point", point_data_two_ts, TimeKey("default"));
    
    auto [line_data_on_line, point_data_on_line] = line_distance_scenarios::json_pipeline_point_on_line();
    line_data_on_line->setTimeFrame(time_frame);
    point_data_on_line->setTimeFrame(time_frame);
    dm.setData("json_pipeline_point_on_line_line", line_data_on_line, TimeKey("default"));
    dm.setData("json_pipeline_point_on_line_point", point_data_on_line, TimeKey("default"));
    
    auto [line_data_horiz, point_data_horiz] = line_distance_scenarios::horizontal_line_point_above();
    line_data_horiz->setTimeFrame(time_frame);
    point_data_horiz->setTimeFrame(time_frame);
    dm.setData("horizontal_line_point_above_line", line_data_horiz, TimeKey("default"));
    dm.setData("horizontal_line_point_above_point", point_data_horiz, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_min_point_dist_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Two timesteps pipeline") {
        // Uses additional_input_keys for multi-input (binary) transforms
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Min Point Distance Pipeline",
                    "description": "Test line to point minimum distance calculation",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineMinPointDistance",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "additional_input_keys": ["json_pipeline_two_timesteps_point"],
                        "output_key": "v2_line_point_distances",
                        "parameters": {
                            "use_first_line_only": true,
                            "return_squared_distance": false
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "two_timesteps_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        auto result_distances = dm.getData<RaggedAnalogTimeSeries>("v2_line_point_distances");
        REQUIRE(result_distances != nullptr);
        
        // Check we have 2 results (t=100 and t=200)
        REQUIRE(result_distances->getNumSamples() == 2);
        
        // Verify distance values
        // t=100: horizontal line (0,0)-(10,0) with point (5,5), distance = 5.0
        // t=200: vertical line (5,0)-(5,10) with point (8,5), distance = 3.0
        auto all_samples = result_distances->getAllSamples();
        bool found_100 = false;
        bool found_200 = false;
        
        for (auto const& sample : all_samples) {
            if (sample.time_frame_index == TimeFrameIndex(100)) {
                REQUIRE(!sample.value.empty());
                REQUIRE_THAT(sample.value[0], WithinAbs(5.0f, 0.001f));
                found_100 = true;
            } else if (sample.time_frame_index == TimeFrameIndex(200)) {
                REQUIRE(!sample.value.empty());
                REQUIRE_THAT(sample.value[0], WithinAbs(3.0f, 0.001f));
                found_200 = true;
            }
        }
        
        REQUIRE(found_100);
        REQUIRE(found_200);
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Point on line edge case") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Point On Line Test",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineMinPointDistance",
                        "input_key": "json_pipeline_point_on_line_line",
                        "additional_input_keys": ["json_pipeline_point_on_line_point"],
                        "output_key": "v2_point_on_line_distances",
                        "parameters": {}
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "point_on_line_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_distances = dm.getData<RaggedAnalogTimeSeries>("v2_point_on_line_distances");
        REQUIRE(result_distances != nullptr);
        REQUIRE(result_distances->getNumSamples() == 1);
        
        // Distance should be 0 for point exactly on line
        auto all_samples = result_distances->getAllSamples();
        REQUIRE(!all_samples.empty());
        REQUIRE(!all_samples.front().value.empty());
        REQUIRE_THAT(all_samples.front().value[0], WithinAbs(0.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Basic horizontal line test") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Horizontal Line Test",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineMinPointDistance",
                        "input_key": "horizontal_line_point_above_line",
                        "additional_input_keys": ["horizontal_line_point_above_point"],
                        "output_key": "v2_horizontal_distances",
                        "parameters": {}
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "horizontal_line_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_distances = dm.getData<RaggedAnalogTimeSeries>("v2_horizontal_distances");
        REQUIRE(result_distances != nullptr);
        REQUIRE(result_distances->getNumSamples() == 1);
        
        // Distance should be 5.0 (point at y=5, line at y=0)
        auto all_samples = result_distances->getAllSamples();
        REQUIRE(!all_samples.empty());
        REQUIRE(!all_samples.front().value.empty());
        REQUIRE_THAT(all_samples.front().value[0], WithinAbs(5.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
