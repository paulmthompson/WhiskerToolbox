#include "LineBaseFlip.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/point_geometry.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/base_flip_scenarios.hpp"

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
// Tests: LineBaseFlipParams JSON Loading
// ============================================================================

TEST_CASE("V2 LineBaseFlipParams - Load valid JSON with all fields", 
          "[transforms][v2][params][json][line_base_flip]") {
    std::string json = R"({
        "reference_x": 12.0,
        "reference_y": 5.0
    })";
    
    auto result = loadParametersFromJson<LineBaseFlipParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getReferenceX(), Catch::Matchers::WithinRel(12.0f, 0.001f));
    REQUIRE_THAT(params.getReferenceY(), Catch::Matchers::WithinRel(5.0f, 0.001f));
}

TEST_CASE("V2 LineBaseFlipParams - Load JSON with partial fields (uses defaults)", 
          "[transforms][v2][params][json][line_base_flip]") {
    std::string json = R"({
        "reference_x": 10.0
    })";
    
    auto result = loadParametersFromJson<LineBaseFlipParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getReferenceX(), Catch::Matchers::WithinRel(10.0f, 0.001f));
    REQUIRE_THAT(params.getReferenceY(), Catch::Matchers::WithinRel(0.0f, 0.001f)); // default
}

TEST_CASE("V2 LineBaseFlipParams - Load empty JSON (uses all defaults)", 
          "[transforms][v2][params][json][line_base_flip]") {
    std::string json = "{}";
    
    auto result = loadParametersFromJson<LineBaseFlipParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getReferenceX(), Catch::Matchers::WithinRel(0.0f, 0.001f));
    REQUIRE_THAT(params.getReferenceY(), Catch::Matchers::WithinRel(0.0f, 0.001f));
}

TEST_CASE("V2 LineBaseFlipParams - getReferencePoint helper", 
          "[transforms][v2][params][line_base_flip]") {
    LineBaseFlipParams params;
    params.reference_x = 5.0f;
    params.reference_y = 10.0f;
    
    auto ref_point = params.getReferencePoint();
    
    REQUIRE_THAT(ref_point.x, Catch::Matchers::WithinRel(5.0f, 0.001f));
    REQUIRE_THAT(ref_point.y, Catch::Matchers::WithinRel(10.0f, 0.001f));
}

// ============================================================================
// Tests: Helper Functions
// ============================================================================

TEST_CASE("V2 LineBaseFlip - calc_distance2 function", 
          "[transforms][v2][element][line_base_flip]") {
    
    SECTION("Same point - zero distance") {
        Point2D<float> p1{5.0f, 5.0f};
        Point2D<float> p2{5.0f, 5.0f};
        
        float dist_sq = calc_distance2(p1, p2);
        
        REQUIRE_THAT(dist_sq, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Horizontal distance") {
        Point2D<float> p1{0.0f, 0.0f};
        Point2D<float> p2{3.0f, 0.0f};
        
        float dist_sq = calc_distance2(p1, p2);
        
        REQUIRE_THAT(dist_sq, WithinAbs(9.0f, 0.001f));  // 3^2 = 9
    }
    
    SECTION("Vertical distance") {
        Point2D<float> p1{0.0f, 0.0f};
        Point2D<float> p2{0.0f, 4.0f};
        
        float dist_sq = calc_distance2(p1, p2);
        
        REQUIRE_THAT(dist_sq, WithinAbs(16.0f, 0.001f));  // 4^2 = 16
    }
    
    SECTION("Diagonal distance (3-4-5 triangle)") {
        Point2D<float> p1{0.0f, 0.0f};
        Point2D<float> p2{3.0f, 4.0f};
        
        float dist_sq = calc_distance2(p1, p2);
        
        REQUIRE_THAT(dist_sq, WithinAbs(25.0f, 0.001f));  // 3^2 + 4^2 = 25
    }
}

TEST_CASE("V2 LineBaseFlip - is_distal_end_closer function", 
          "[transforms][v2][element][line_base_flip]") {
    
    SECTION("Empty line - should not flip") {
        Line2D empty_line;
        Point2D<float> ref{0.0f, 0.0f};
        
        REQUIRE_FALSE(is_distal_end_closer(empty_line, ref));
    }
    
    SECTION("Single point line - should not flip") {
        Line2D single_point{{Point2D<float>{5.0f, 5.0f}}};
        Point2D<float> ref{0.0f, 0.0f};
        
        REQUIRE_FALSE(is_distal_end_closer(single_point, ref));
    }
    
    SECTION("Base closer to reference - should not flip") {
        // Line from (0,0) to (10,0), reference at (-2,0)
        // Base (0,0) is 2 units from reference
        // End (10,0) is 12 units from reference
        Line2D line{{Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 0.0f}}};
        Point2D<float> ref{-2.0f, 0.0f};
        
        REQUIRE_FALSE(is_distal_end_closer(line, ref));
    }
    
    SECTION("End closer to reference - should flip") {
        // Line from (0,0) to (10,0), reference at (12,0)
        // Base (0,0) is 12 units from reference
        // End (10,0) is 2 units from reference
        Line2D line{{Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 0.0f}}};
        Point2D<float> ref{12.0f, 0.0f};
        
        REQUIRE(is_distal_end_closer(line, ref));
    }
    
    SECTION("Equal distances - should not flip (keeps original)") {
        // Line from (0,0) to (10,0), reference at (5,0)
        // Both endpoints are 5 units from reference
        Line2D line{{Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 0.0f}}};
        Point2D<float> ref{5.0f, 0.0f};
        
        REQUIRE_FALSE(is_distal_end_closer(line, ref));
    }
}

TEST_CASE("V2 LineBaseFlip - reverse_line function", 
          "[transforms][v2][element][line_base_flip]") {
    
    SECTION("Empty line") {
        Line2D empty_line;
        auto flipped = reverse_line(empty_line);
        
        REQUIRE(flipped.empty());
    }
    
    SECTION("Two point line") {
        Line2D line{{Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 0.0f}}};
        auto flipped = reverse_line(line);
        
        REQUIRE(flipped.size() == 2);
        REQUIRE_THAT(flipped.front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(flipped.front().y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(flipped.back().x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(flipped.back().y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Multi-point line") {
        Line2D line{{
            Point2D<float>{0.0f, 0.0f}, 
            Point2D<float>{5.0f, 0.0f}, 
            Point2D<float>{10.0f, 0.0f}
        }};
        auto flipped = reverse_line(line);
        
        REQUIRE(flipped.size() == 3);
        REQUIRE_THAT(flipped[0].x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(flipped[1].x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(flipped[2].x, WithinAbs(0.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Main Transform Function Using Scenarios
// ============================================================================

TEST_CASE("V2 Element Transform: LineBaseFlip - Core Functionality", 
          "[transforms][v2][element][line_base_flip]") {
    
    LineBaseFlipParams params;
    
    SECTION("Flip line based on reference point (using scenario)") {
        // Create a simple line from (0,0) to (10,0) using scenario
        auto line_data = line_base_flip_scenarios::simple_horizontal_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        // Set reference point closer to the end (10,0) than the base (0,0)
        params.reference_x = 12.0f;
        params.reference_y = 0.0f;
        
        auto flipped = flipLineBase(line, params);
        
        REQUIRE(flipped.size() == 3);
        // Check that the line has been flipped (base should now be at (10,0))
        REQUIRE_THAT(flipped.front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(flipped.front().y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(flipped.back().x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(flipped.back().y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Do not flip when base is closer (using scenario)") {
        auto line_data = line_base_flip_scenarios::simple_horizontal_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        // Set reference point closer to the base (0,0) than the end (10,0)
        params.reference_x = -2.0f;
        params.reference_y = 0.0f;
        
        auto result = flipLineBase(line, params);
        
        REQUIRE(result.size() == 3);
        // Check that the line has NOT been flipped (base should still be at (0,0))
        REQUIRE_THAT(result.front().x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(result.front().y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(result.back().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.back().y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Handle single point line (using scenario)") {
        auto line_data = line_base_flip_scenarios::single_point_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        params.reference_x = 0.0f;
        params.reference_y = 0.0f;
        
        auto result = flipLineBase(line, params);
        
        // Should not flip single point line
        REQUIRE(result.size() == 1);
        REQUIRE_THAT(result.front().x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(result.front().y, WithinAbs(5.0f, 0.001f));
    }
    
    SECTION("Process vertical line (using scenario)") {
        auto line_data = line_base_flip_scenarios::vertical_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        // Reference point closer to end (5,10)
        params.reference_x = 5.0f;
        params.reference_y = 15.0f;
        
        auto result = flipLineBase(line, params);
        
        REQUIRE(result.size() == 3);
        // Should be flipped (base now at y=10)
        REQUIRE_THAT(result.front().y, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.back().y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Process diagonal line (using scenario)") {
        auto line_data = line_base_flip_scenarios::diagonal_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        // Reference point closer to end (10,10)
        params.reference_x = 15.0f;
        params.reference_y = 15.0f;
        
        auto result = flipLineBase(line, params);
        
        REQUIRE(result.size() == 3);
        // Should be flipped (base now at (10,10))
        REQUIRE_THAT(result.front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.front().y, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.back().x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(result.back().y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Handle empty line data (using scenario)") {
        auto line_data = line_base_flip_scenarios::empty_line_data();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        
        params.reference_x = 0.0f;
        params.reference_y = 0.0f;
        
        auto result = flipLineBase(line, params);
        
        // Should return empty line unchanged
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Multiple Frames Processing (using scenario)
// ============================================================================

TEST_CASE("V2 Element Transform: LineBaseFlip - Multiple Frames", 
          "[transforms][v2][element][line_base_flip]") {
    
    auto line_data = line_base_flip_scenarios::multiple_frames();
    LineBaseFlipParams params;
    
    // Reference point closer to end points
    params.reference_x = 12.0f;
    params.reference_y = 5.0f;
    
    SECTION("Frame 0") {
        auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
        auto result = flipLineBase(line, params);
        
        // Frame 0: (0,0) -> (10,0), should flip since reference is closer to (10,0)
        REQUIRE_THAT(result.front().x, WithinAbs(10.0f, 0.001f));
    }
    
    SECTION("Frame 1") {
        auto line = getLineAt(line_data.get(), TimeFrameIndex(1));
        auto result = flipLineBase(line, params);
        
        // Frame 1: (0,10) -> (10,10), should flip since reference is closer to (10,10)
        REQUIRE_THAT(result.front().x, WithinAbs(10.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Context-aware version
// ============================================================================

TEST_CASE("V2 Element Transform: LineBaseFlip - With Context", 
          "[transforms][v2][element][line_base_flip][context]") {
    
    auto line_data = line_base_flip_scenarios::simple_horizontal_line();
    auto line = getLineAt(line_data.get(), TimeFrameIndex(0));
    
    LineBaseFlipParams params;
    params.reference_x = 12.0f;
    params.reference_y = 0.0f;
    
    SECTION("Normal execution with context") {
        ComputeContext ctx;
        bool progress_reported = false;
        ctx.progress = [&progress_reported](float) { progress_reported = true; };
        ctx.is_cancelled = []() { return false; };
        
        auto result = flipLineBaseWithContext(line, params, ctx);
        
        REQUIRE(result.size() == 3);
        REQUIRE_THAT(result.front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE(progress_reported);
    }
    
    SECTION("Cancelled execution returns original line") {
        ComputeContext ctx;
        ctx.progress = [](float) {};
        ctx.is_cancelled = []() { return true; };
        
        auto result = flipLineBaseWithContext(line, params, ctx);
        
        // Should return original line when cancelled
        REQUIRE(result.size() == 3);
        REQUIRE_THAT(result.front().x, WithinAbs(0.0f, 0.001f));  // Original base
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineBaseFlip Registry Integration",
          "[transforms][v2][registry][element][line_base_flip]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("FlipLineBase"));
    }
    
    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasElementTransform("FlipLineBaseWithContext"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("FlipLineBase");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "FlipLineBase");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: Pipeline Interface for LineData Containers
// ============================================================================

TEST_CASE("V2 Pipeline: LineBaseFlip on LineData Container",
          "[transforms][v2][pipeline][line_base_flip]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Execute via pipeline on single-frame LineData") {
        auto line_data = line_base_flip_scenarios::simple_horizontal_line();
        auto time_frame = std::make_shared<TimeFrame>();
        line_data->setTimeFrame(time_frame);
        
        LineBaseFlipParams params;
        params.reference_x = 12.0f;  // Reference closer to end (10,0)
        params.reference_y = 0.0f;
        
        // Create pipeline using non-templated syntax
        TransformPipeline pipeline;
        pipeline.addStep("FlipLineBase", params);
        
        // Execute and get view typed to Line2D output
        auto view = pipeline.executeAsViewTyped<LineData, Line2D>(*line_data);
        
        // Collect results 
        std::vector<std::pair<TimeFrameIndex, Line2D>> results;
        for (auto pair : view) {
            results.emplace_back(pair.first, pair.second);
        }
        
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].second.size() == 3);
        REQUIRE_THAT(results[0].second.front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(results[0].second.back().x, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Execute via pipeline on multi-frame LineData") {
        auto line_data = line_base_flip_scenarios::multiple_frames();
        auto time_frame = std::make_shared<TimeFrame>();
        line_data->setTimeFrame(time_frame);
        
        LineBaseFlipParams params;
        params.reference_x = 12.0f;
        params.reference_y = 5.0f;
        
        TransformPipeline pipeline;
        pipeline.addStep("FlipLineBase", params);
        
        auto view = pipeline.executeAsViewTyped<LineData, Line2D>(*line_data);
        
        std::vector<std::pair<TimeFrameIndex, Line2D>> results;
        for (auto pair : view) {
            results.emplace_back(pair.first, pair.second);
        }
        
        REQUIRE(results.size() == 2);
        
        // Check frame 0
        REQUIRE(results[0].first == TimeFrameIndex(0));
        REQUIRE_THAT(results[0].second.front().x, WithinAbs(10.0f, 0.001f));
        
        // Check frame 1
        REQUIRE(results[1].first == TimeFrameIndex(1));
        REQUIRE_THAT(results[1].second.front().x, WithinAbs(10.0f, 0.001f));
    }
    
    SECTION("Materialize pipeline result into LineData") {
        auto line_data = line_base_flip_scenarios::simple_horizontal_line();
        auto time_frame = std::make_shared<TimeFrame>();
        line_data->setTimeFrame(time_frame);
        
        LineBaseFlipParams params;
        params.reference_x = 12.0f;
        params.reference_y = 0.0f;
        
        TransformPipeline pipeline;
        pipeline.addStep("FlipLineBase", params);
        
        auto view = pipeline.executeAsViewTyped<LineData, Line2D>(*line_data);
        
        // Materialize into new LineData
        auto result = std::make_shared<LineData>(view);
        result->setTimeFrame(time_frame);
        
        auto result_lines = result->getAtTime(TimeFrameIndex(0));
        REQUIRE(result_lines.size() == 1);
        REQUIRE_THAT(result_lines[0].front().x, WithinAbs(10.0f, 0.001f));
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineBaseFlip via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_base_flip]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate with scenario data
    auto two_timesteps_data = line_base_flip_scenarios::json_pipeline_two_timesteps();
    two_timesteps_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_two_timesteps_line", two_timesteps_data, TimeKey("default"));
    
    auto mixed_outcomes_data = line_base_flip_scenarios::json_pipeline_mixed_outcomes();
    mixed_outcomes_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_mixed_outcomes_line", mixed_outcomes_data, TimeKey("default"));
    
    auto edge_cases_data = line_base_flip_scenarios::json_pipeline_edge_cases();
    edge_cases_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_edge_cases_line", edge_cases_data, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_base_flip_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Two timesteps pipeline - both should flip") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Base Flip Pipeline",
                    "description": "Test line base flip transform",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "FlipLineBase",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_flipped_lines",
                        "parameters": {
                            "reference_x": 12.0,
                            "reference_y": 0.0
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
        auto result_lines = dm.getData<LineData>("v2_flipped_lines");
        REQUIRE(result_lines != nullptr);
        
        // Check we have 2 time points (t=100 and t=200)
        REQUIRE(result_lines->getTimesWithData().size() == 2);
        
        // Verify t=100: line (0,0)->(10,0) should flip to (10,0)->(0,0)
        auto lines_t100 = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines_t100.size() == 1);
        REQUIRE_THAT(lines_t100[0].front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(lines_t100[0].back().x, WithinAbs(0.0f, 0.001f));
        
        // Verify t=200: diagonal line should also flip
        auto lines_t200 = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_t200.size() == 1);
        REQUIRE_THAT(lines_t200[0].front().x, WithinAbs(10.0f, 0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Mixed outcomes pipeline - some flip, some don't") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Mixed Outcomes Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "FlipLineBase",
                        "input_key": "json_pipeline_mixed_outcomes_line",
                        "output_key": "v2_mixed_flipped",
                        "parameters": {
                            "reference_x": 12.0,
                            "reference_y": 5.0
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "mixed_outcomes_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_mixed_flipped");
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().size() == 3);
        
        // t=100: (0,0)->(10,0) should flip - end (10,0) closer to ref (12,5)
        auto lines_t100 = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines_t100.size() == 1);
        REQUIRE_THAT(lines_t100[0].front().x, WithinAbs(10.0f, 0.001f));
        
        // t=200: (10,0)->(0,0) should NOT flip - base (10,0) already closer to ref (12,5)
        auto lines_t200 = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_t200.size() == 1);
        REQUIRE_THAT(lines_t200[0].front().x, WithinAbs(10.0f, 0.001f));
        
        // t=300: (5,0)->(5,8) should flip - end (5,8) closer to ref (12,5)
        auto lines_t300 = result_lines->getAtTime(TimeFrameIndex(300));
        REQUIRE(lines_t300.size() == 1);
        REQUIRE_THAT(lines_t300[0].front().y, WithinAbs(8.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Edge cases pipeline - single point and two-point lines") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Edge Cases Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "FlipLineBase",
                        "input_key": "json_pipeline_edge_cases_line",
                        "output_key": "v2_edge_cases_flipped",
                        "parameters": {
                            "reference_x": 15.0,
                            "reference_y": 15.0
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "edge_cases_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_edge_cases_flipped");
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().size() == 2);
        
        // t=100: single point (5,5) should remain unchanged
        auto lines_t100 = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines_t100.size() == 1);
        REQUIRE(lines_t100[0].size() == 1);
        REQUIRE_THAT(lines_t100[0].front().x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(lines_t100[0].front().y, WithinAbs(5.0f, 0.001f));
        
        // t=200: (0,0)->(10,10) should flip - end closer to ref (15,15)
        auto lines_t200 = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_t200.size() == 1);
        REQUIRE_THAT(lines_t200[0].front().x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(lines_t200[0].front().y, WithinAbs(10.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Default parameters (reference at origin)") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Default Params Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "FlipLineBase",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_default_params_flipped",
                        "parameters": {}
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "default_params_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_default_params_flipped");
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().size() == 2);
        
        // With default reference at (0,0):
        // t=100: (0,0)->(10,0) should NOT flip - base already at origin
        auto lines_t100 = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines_t100.size() == 1);
        REQUIRE_THAT(lines_t100[0].front().x, WithinAbs(0.0f, 0.001f));
        
        // t=200: (0,0)->(10,10) diagonal should NOT flip - base already at origin
        auto lines_t200 = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_t200.size() == 1);
        REQUIRE_THAT(lines_t200[0].front().x, WithinAbs(0.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
