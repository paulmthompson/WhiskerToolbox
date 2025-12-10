#include "LinePointExtraction.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/point_extraction_scenarios.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const* line_data, TimeFrameIndex time) {
    auto lines_at_time = line_data->getAtTime(time);
    for (auto const& line : lines_at_time) {
        return line;  // Return the first line at this time
    }
    return Line2D{};
}

// ============================================================================
// Tests: Algorithm Correctness (using scenarios)
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtraction - Core Functionality",
          "[transforms][v2][element][line_point_extraction]") {
    
    LinePointExtractionParams params;
    
    SECTION("Direct method - middle position") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "Direct";
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 50% position of diagonal line, should be approximately (1.5, 1.5)
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.001f));
    }
    
    SECTION("Direct method - start position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_200();
        TimeFrameIndex timestamp(200);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.0f;
        params.method = "Direct";
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 0% position, should be at the first point
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Direct method - end position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_300();
        TimeFrameIndex timestamp(300);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 1.0f;
        params.method = "Direct";
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 100% position, should be at the last point
        REQUIRE_THAT(point.x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(3.0f, 0.001f));
    }
    
    SECTION("Parametric method - middle position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_400();
        TimeFrameIndex timestamp(400);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "Parametric";
        params.polynomial_order = 3;
        
        auto point = extractLinePoint(line, params);
        
        // Parametric method should give a smooth interpolation
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.1f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.1f));
    }
    
    SECTION("Horizontal line - middle position") {
        auto line_data = point_extraction_scenarios::horizontal_3_points();
        TimeFrameIndex timestamp(600);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "Direct";
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 50% of horizontal line, should be at (1.0, 0.0)
        REQUIRE_THAT(point.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("V2 Element Transform: LinePointExtraction - Edge Cases",
          "[transforms][v2][element][line_point_extraction][edge]") {
    
    LinePointExtractionParams params;
    params.position = 0.5f;
    params.method = "Direct";
    
    SECTION("Empty line returns (0, 0)") {
        Line2D empty_line;
        
        auto point = extractLinePoint(empty_line, params);
        
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Single point line returns that point") {
        auto line_data = point_extraction_scenarios::single_point();
        TimeFrameIndex timestamp(200);
        auto line = getLineAt(line_data.get(), timestamp);
        
        auto point = extractLinePoint(line, params);
        
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(2.5f, 0.001f));
    }
    
    SECTION("Two-point line works correctly") {
        auto line_data = point_extraction_scenarios::two_point_line();
        TimeFrameIndex timestamp(500);
        auto line = getLineAt(line_data.get(), timestamp);
        
        auto point = extractLinePoint(line, params);
        
        // At 50% of diagonal line from (0,0) to (1,1), should be (0.5, 0.5)
        REQUIRE_THAT(point.x, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Parametric fallback with too few points") {
        auto line_data = point_extraction_scenarios::two_point_line();
        TimeFrameIndex timestamp(500);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = "Parametric";
        params.polynomial_order = 3;  // Requires more than 2 points
        
        auto point = extractLinePoint(line, params);
        
        // Should fall back to direct method or linear interpolation
        REQUIRE_THAT(point.x, WithinAbs(0.5f, 0.1f));
        REQUIRE_THAT(point.y, WithinAbs(0.5f, 0.1f));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtractionParams - JSON Loading",
          "[transforms][v2][params][json][line_point_extraction]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "position": 0.5,
            "method": "Parametric",
            "polynomial_order": 5,
            "use_interpolation": true
        })";
        
        auto result = loadParametersFromJson<LinePointExtractionParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.5f, 0.001f));
        REQUIRE(params.getMethod() == LinePointExtractionMethod::Parametric);
        REQUIRE(params.getPolynomialOrder() == 5);
        REQUIRE(params.getUseInterpolation() == true);
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LinePointExtractionParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.5f, 0.001f));
        REQUIRE(params.getMethod() == LinePointExtractionMethod::Direct);
        REQUIRE(params.getPolynomialOrder() == 3);
        REQUIRE(params.getUseInterpolation() == true);
    }
    
    SECTION("JSON round-trip preserves values") {
        LinePointExtractionParams original;
        original.position = 0.75f;
        original.method = "Parametric";
        original.polynomial_order = 4;
        original.use_interpolation = false;
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LinePointExtractionParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE_THAT(recovered.getPosition(), WithinAbs(0.75f, 0.001f));
        REQUIRE(recovered.getMethod() == LinePointExtractionMethod::Parametric);
        REQUIRE(recovered.getPolynomialOrder() == 4);
        REQUIRE(recovered.getUseInterpolation() == false);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtraction Registry Integration",
          "[transforms][v2][registry][element][line_point_extraction]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("ExtractLinePoint"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("ExtractLinePoint");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "ExtractLinePoint");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LinePointExtraction via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_point_extraction]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate with scenario data
    auto diagonal_data = point_extraction_scenarios::diagonal_4_points();
    diagonal_data->setTimeFrame(time_frame);
    dm.setData("diagonal_line", diagonal_data, TimeKey("default"));
    
    auto multiple_timesteps_data = point_extraction_scenarios::multiple_timesteps();
    multiple_timesteps_data->setTimeFrame(time_frame);
    dm.setData("multiple_timesteps_line", multiple_timesteps_data, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_extraction_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Single diagonal line - middle position") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Point Extraction Pipeline",
                    "description": "Test line point extraction",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ExtractLinePoint",
                        "input_key": "diagonal_line",
                        "output_key": "v2_extracted_points",
                        "parameters": {
                            "position": 0.5,
                            "method": "Direct",
                            "use_interpolation": true
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "diagonal_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        auto result_points = dm.getData<PointData>("v2_extracted_points");
        REQUIRE(result_points != nullptr);
        
        // Check we have 1 result at t=100
        REQUIRE(result_points->getTimesWithData().size() == 1);
        
        // Verify point value at 50% position of diagonal line
        auto const& points_t100 = result_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_t100.size() == 1);
        REQUIRE_THAT(points_t100[0].x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(points_t100[0].y, WithinAbs(1.5f, 0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Multiple timesteps pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Multiple Timesteps Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ExtractLinePoint",
                        "input_key": "multiple_timesteps_line",
                        "output_key": "v2_multi_points",
                        "parameters": {
                            "position": 0.5,
                            "method": "Direct"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "multiple_timesteps_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_points = dm.getData<PointData>("v2_multi_points");
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().size() == 2);
        
        // Check t=500: Diagonal line (0,0) to (4,4), 3 points
        // At 50%, should be at (2, 2)
        auto const& points_t500 = result_points->getAtTime(TimeFrameIndex(500));
        REQUIRE(points_t500.size() == 1);
        REQUIRE_THAT(points_t500[0].x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points_t500[0].y, WithinAbs(2.0f, 0.001f));
        
        // Check t=600: Horizontal line (0,0) to (2,0), 3 points
        // At 50%, should be at (1, 0)
        auto const& points_t600 = result_points->getAtTime(TimeFrameIndex(600));
        REQUIRE(points_t600.size() == 1);
        REQUIRE_THAT(points_t600[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(points_t600[0].y, WithinAbs(0.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Parametric method pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Parametric Method Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ExtractLinePoint",
                        "input_key": "diagonal_line",
                        "output_key": "v2_parametric_points",
                        "parameters": {
                            "position": 0.5,
                            "method": "Parametric",
                            "polynomial_order": 3
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "parametric_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_points = dm.getData<PointData>("v2_parametric_points");
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().size() == 1);
        
        // Verify parametric extraction produces valid result
        auto const& points_t100 = result_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_t100.size() == 1);
        REQUIRE_THAT(points_t100[0].x, WithinAbs(1.5f, 0.1f));
        REQUIRE_THAT(points_t100[0].y, WithinAbs(1.5f, 0.1f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Start and end position pipeline") {
        // Test with start position (0.0)
        const char* json_config_start = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Start Position Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ExtractLinePoint",
                        "input_key": "diagonal_line",
                        "output_key": "v2_start_point",
                        "parameters": {
                            "position": 0.0,
                            "method": "Direct"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath_start = test_dir / "start_pipeline.json";
        {
            std::ofstream json_file(json_filepath_start);
            REQUIRE(json_file.is_open());
            json_file << json_config_start;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath_start.string());
        
        auto start_points = dm.getData<PointData>("v2_start_point");
        REQUIRE(start_points != nullptr);
        
        auto const& points_start = start_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_start.size() == 1);
        REQUIRE_THAT(points_start[0].x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(points_start[0].y, WithinAbs(0.0f, 0.001f));
        
        // Test with end position (1.0)
        const char* json_config_end = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "End Position Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ExtractLinePoint",
                        "input_key": "diagonal_line",
                        "output_key": "v2_end_point",
                        "parameters": {
                            "position": 1.0,
                            "method": "Direct"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath_end = test_dir / "end_pipeline.json";
        {
            std::ofstream json_file(json_filepath_end);
            REQUIRE(json_file.is_open());
            json_file << json_config_end;
            json_file.close();
        }
        
        data_info_list = load_data_from_json_config_v2(&dm, json_filepath_end.string());
        
        auto end_points = dm.getData<PointData>("v2_end_point");
        REQUIRE(end_points != nullptr);
        
        auto const& points_end = end_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_end.size() == 1);
        REQUIRE_THAT(points_end[0].x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(points_end[0].y, WithinAbs(3.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}

// ============================================================================
// Tests: Context-Aware Transform
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtraction Context-Aware",
          "[transforms][v2][context][line_point_extraction]") {
    
    SECTION("Context-aware transform reports progress") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LinePointExtractionParams params;
        params.position = 0.5f;
        params.method = "Direct";
        
        int last_progress = -1;
        ComputeContext ctx;
        ctx.progress = [&last_progress](int p) { last_progress = p; };
        ctx.is_cancelled = []() { return false; };
        
        auto point = extractLinePointWithContext(line, params, ctx);
        
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.001f));
        REQUIRE(last_progress > 0);  // Progress was reported
    }
    
    SECTION("Context-aware transform respects cancellation") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LinePointExtractionParams params;
        params.position = 0.5f;
        
        ComputeContext ctx;
        ctx.progress = [](int) {};
        ctx.is_cancelled = []() { return true; };  // Always cancelled
        
        auto point = extractLinePointWithContext(line, params, ctx);
        
        // Should return default point when cancelled
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
}
