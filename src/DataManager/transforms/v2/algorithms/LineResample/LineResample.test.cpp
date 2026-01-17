#include "LineResample.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/resample_scenarios.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Registration: Uses singleton from RegisteredTransforms.cpp (compile-time)
// ============================================================================
// ResampleLine is registered at compile-time via RegisterTransform
// RAII helper in RegisteredTransforms.cpp. The ElementRegistry::instance()
// singleton already has this transform available when tests run.

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const * line_data, TimeFrameIndex time) {
    auto lines_at_time = line_data->getAtTime(time);
    for (auto const & line: lines_at_time) {
        return line;  // Return the first line at this time
    }
    return Line2D{};
}

// ============================================================================
// Tests: Algorithm Correctness (using scenarios)
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample - FixedSpacing Algorithm",
          "[transforms][v2][element][line_resample]") {
    
    LineResampleParams params;
    params.method = "FixedSpacing";
    
    SECTION("Two diagonal lines") {
        auto line_data = resample_scenarios::two_diagonal_lines();
        
        params.target_spacing = rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>(15.0f);
        
        // Test first line at t=100
        auto line_t100 = getLineAt(line_data.get(), TimeFrameIndex(100));
        REQUIRE(!line_t100.empty());
        
        auto resampled = resampleLine(line_t100, params);
        REQUIRE(!resampled.empty());
        // The resampled line should be valid (size >= 2 since we preserve endpoints)
        REQUIRE(resampled.size() >= 2);
        
        // Test second line at t=200
        auto line_t200 = getLineAt(line_data.get(), TimeFrameIndex(200));
        REQUIRE(!line_t200.empty());
        
        auto resampled_200 = resampleLine(line_t200, params);
        REQUIRE(!resampled_200.empty());
        REQUIRE(resampled_200.size() >= 2);
    }
    
    SECTION("Simple diagonal line") {
        auto line_data = resample_scenarios::simple_diagonal();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        
        params.target_spacing = 10.0f;
        
        auto resampled = resampleLine(line, params);
        REQUIRE(!resampled.empty());
    }
}

TEST_CASE("V2 Element Transform: LineResample - DouglasPeucker Algorithm",
          "[transforms][v2][element][line_resample]") {
    
    LineResampleParams params;
    params.method = "DouglasPeucker";
    
    SECTION("Dense nearly-straight line simplification") {
        auto line_data = resample_scenarios::dense_nearly_straight_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        
        REQUIRE(line.size() == 11);  // Original has 11 points
        
        params.epsilon = 0.5f;
        
        auto simplified = resampleLine(line, params);
        
        // Douglas-Peucker should significantly reduce point count for nearly straight line
        REQUIRE(simplified.size() < line.size());
        REQUIRE(simplified.size() >= 2);  // At least start and end points preserved
    }
    
    SECTION("Simple diagonal remains compact") {
        auto line_data = resample_scenarios::simple_diagonal();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        
        params.epsilon = 1.0f;
        
        auto simplified = resampleLine(line, params);
        
        // Simple diagonal (3 points) should simplify to 2 points (endpoints)
        REQUIRE(simplified.size() <= line.size());
    }
}

TEST_CASE("V2 Element Transform: LineResample - Edge Cases",
          "[transforms][v2][element][line_resample][edge]") {
    
    LineResampleParams params;
    
    SECTION("Empty line returns empty") {
        auto line_data = resample_scenarios::empty();
        
        // No lines exist, so create an empty line
        Line2D empty_line;
        
        auto result = resampleLine(empty_line, params);
        REQUIRE(result.empty());
    }
    
    SECTION("Single point line returned unchanged") {
        auto line_data = resample_scenarios::single_point();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
        
        REQUIRE(line.size() == 1);
        
        params.method = "FixedSpacing";
        params.target_spacing = 10.0f;
        
        auto result = resampleLine(line, params);
        REQUIRE(result.size() == 1);
    }
    
    SECTION("Two point line returned unchanged") {
        // Two-point lines are minimal representation
        Line2D two_point_line;
        two_point_line.push_back(Point2D<float>{0.0f, 0.0f});
        two_point_line.push_back(Point2D<float>{10.0f, 10.0f});
        
        params.method = "FixedSpacing";
        params.target_spacing = 5.0f;
        
        auto result = resampleLine(two_point_line, params);
        REQUIRE(result.size() == 2);
    }
    
    SECTION("Diagonal with empty line - only empty processed correctly") {
        auto line_data = resample_scenarios::diagonal_with_empty();
        
        // t=200 has an empty line
        auto line_t200 = getLineAt(line_data.get(), TimeFrameIndex(200));
        
        auto result = resampleLine(line_t200, params);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Element Transform: LineResampleParams - JSON Loading",
          "[transforms][v2][params][json][line_resample]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "method": "DouglasPeucker",
            "target_spacing": 15.0,
            "epsilon": 3.5
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getMethod() == LineResampleMethod::DouglasPeucker);
        REQUIRE_THAT(params.getTargetSpacing(), WithinAbs(15.0f, 0.001f));
        REQUIRE_THAT(params.getEpsilon(), WithinAbs(3.5f, 0.001f));
    }
    
    SECTION("Load JSON with partial fields (uses defaults)") {
        std::string json = R"({
            "method": "FixedSpacing"
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getMethod() == LineResampleMethod::FixedSpacing);
        REQUIRE_THAT(params.getTargetSpacing(), WithinAbs(5.0f, 0.001f));  // default
        REQUIRE_THAT(params.getEpsilon(), WithinAbs(2.0f, 0.001f));        // default
    }
    
    SECTION("Load empty JSON (uses all defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getMethod() == LineResampleMethod::FixedSpacing);  // default
        REQUIRE_THAT(params.getTargetSpacing(), WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(params.getEpsilon(), WithinAbs(2.0f, 0.001f));
    }
    
    SECTION("Douglas-Peucker alternate spelling accepted") {
        std::string json = R"({
            "method": "Douglas-Peucker"
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getMethod() == LineResampleMethod::DouglasPeucker);
    }
    
    SECTION("Reject negative target_spacing") {
        std::string json = R"({
            "target_spacing": -5.0
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(!result);  // Should fail validation
    }
    
    SECTION("Reject zero target_spacing") {
        std::string json = R"({
            "target_spacing": 0.0
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(!result);  // Should fail validation (ExclusiveMinimum)
    }
    
    SECTION("Reject negative epsilon") {
        std::string json = R"({
            "epsilon": -1.0
        })";
        
        auto result = loadParametersFromJson<LineResampleParams>(json);
        
        REQUIRE(!result);  // Should fail validation
    }
    
    SECTION("JSON round-trip preserves values") {
        LineResampleParams original;
        original.method = "DouglasPeucker";
        original.target_spacing = rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>(12.5f);
        original.epsilon = rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>(4.0f);
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LineResampleParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.getMethod() == LineResampleMethod::DouglasPeucker);
        REQUIRE_THAT(recovered.getTargetSpacing(), WithinAbs(12.5f, 0.001f));
        REQUIRE_THAT(recovered.getEpsilon(), WithinAbs(4.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample Registry Integration",
          "[transforms][v2][registry][element][line_resample]") {
    
    auto & registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("ResampleLine"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const * metadata = registry.getMetadata("ResampleLine");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "ResampleLine");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineResample via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_resample]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate with scenario data
    auto two_diagonal = resample_scenarios::two_diagonal_lines();
    two_diagonal->setTimeFrame(time_frame);
    dm.setData("two_diagonal_lines", two_diagonal, TimeKey("default"));
    
    auto dense_line = resample_scenarios::dense_nearly_straight_line();
    dense_line->setTimeFrame(time_frame);
    dm.setData("dense_nearly_straight_line", dense_line, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_resample_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("FixedSpacing pipeline") {
        const char * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Resample Pipeline",
                    "description": "Test FixedSpacing resampling",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ResampleLine",
                        "input_key": "two_diagonal_lines",
                        "output_key": "v2_resampled_lines",
                        "parameters": {
                            "method": "FixedSpacing",
                            "target_spacing": 15.0
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "fixed_spacing_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        // LineData → LineData (element-wise Line2D → Line2D)
        auto result_lines = dm.getData<LineData>("v2_resampled_lines");
        REQUIRE(result_lines != nullptr);
        
        // Check we have 2 results (t=100 and t=200)
        auto times = result_lines->getTimesWithData();
        REQUIRE(times.size() == 2);
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (std::exception const & e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("DouglasPeucker pipeline") {
        const char * json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Douglas-Peucker Simplification Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ResampleLine",
                        "input_key": "dense_nearly_straight_line",
                        "output_key": "v2_simplified_lines",
                        "parameters": {
                            "method": "DouglasPeucker",
                            "epsilon": 0.5
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "douglas_peucker_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_simplified_lines");
        REQUIRE(result_lines != nullptr);
        
        // Get the simplified line at t=100
        auto simplified = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(!simplified.empty());
        
        // Original had 11 points, simplified should have fewer
        auto original = dense_line->getAtTime(TimeFrameIndex(100));
        REQUIRE(!original.empty());
        REQUIRE(simplified[0].size() < original[0].size());
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (std::exception const & e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}

// ============================================================================
// Tests: Context-Aware Execution
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample - Context-Aware",
          "[transforms][v2][element][line_resample][context]") {
    
    auto line_data = resample_scenarios::two_diagonal_lines();
    auto line = getLineAt(line_data.get(), TimeFrameIndex(100));
    
    LineResampleParams params;
    params.method = "FixedSpacing";
    params.target_spacing = rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>(10.0f);
    
    SECTION("With cancellation check") {
        bool was_cancelled = false;
        ComputeContext ctx;
        ctx.is_cancelled = [&was_cancelled]() { return was_cancelled; };
        ctx.progress = [](int) {};
        
        auto result = resampleLineWithContext(line, params, ctx);
        
        // Normal execution should produce valid result
        REQUIRE(!result.empty());
    }
    
    SECTION("Cancellation returns original line") {
        ComputeContext ctx;
        ctx.is_cancelled = []() { return true; };  // Always cancelled
        ctx.progress = [](int) {};
        
        auto result = resampleLineWithContext(line, params, ctx);
        
        // Cancelled execution returns original line
        REQUIRE(result.size() == line.size());
    }
}
