#include "LineCurvature.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/curvature_scenarios.hpp"

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

TEST_CASE("V2 Element Transform: LineCurvature - Core Functionality",
          "[transforms][v2][element][line_curvature]") {
    
    LineCurvatureParams params;
    
    SECTION("Parabola has non-zero curvature") {
        auto line_data = curvature_scenarios::parabola();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "PolynomialFit";
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;
        
        float curvature = calculateLineCurvature(line, params);
        
        // Curvature should be non-zero for a parabola
        REQUIRE(!std::isnan(curvature));
        REQUIRE(curvature != 0.0f);
    }
    
    SECTION("Straight line has near-zero curvature") {
        auto line_data = curvature_scenarios::straight_line();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "PolynomialFit";
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;
        
        float curvature = calculateLineCurvature(line, params);
        
        // Curvature should be very close to zero for a straight line
        REQUIRE(!std::isnan(curvature));
        REQUIRE(std::abs(curvature) < 0.1f);
    }
    
    SECTION("Different positions along the line") {
        auto line_data = curvature_scenarios::parabola();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = "PolynomialFit";
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;
        
        // Test different positions
        std::vector<float> positions = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        for (float pos : positions) {
            params.position = pos;
            float curvature = calculateLineCurvature(line, params);
            // All positions should produce valid curvature values
            REQUIRE(!std::isnan(curvature));
        }
    }
    
    SECTION("Different polynomial orders") {
        auto line_data = curvature_scenarios::parabola();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "PolynomialFit";
        params.fitting_window_percentage = 0.1f;
        
        // Test different polynomial orders
        for (int order = 2; order <= 4; ++order) {
            params.polynomial_order = order;
            float curvature = calculateLineCurvature(line, params);
            REQUIRE(!std::isnan(curvature));
        }
    }
    
    SECTION("Different fitting window percentages") {
        auto line_data = curvature_scenarios::parabola();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "PolynomialFit";
        params.polynomial_order = 3;
        
        // Test different fitting window percentages
        std::vector<float> windows = {0.05f, 0.1f, 0.2f};
        for (float window : windows) {
            params.fitting_window_percentage = window;
            float curvature = calculateLineCurvature(line, params);
            REQUIRE(!std::isnan(curvature));
        }
    }
}

// ============================================================================
// Tests: Edge Cases
// ============================================================================

TEST_CASE("V2 Element Transform: LineCurvature - Edge Cases",
          "[transforms][v2][element][line_curvature][edge]") {
    
    LineCurvatureParams params;
    params.position = 0.5f;
    params.polynomial_order = 3;
    params.fitting_window_percentage = 0.1f;
    
    SECTION("Single point line returns NaN") {
        auto line_data = curvature_scenarios::single_point();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float curvature = calculateLineCurvature(line, params);
        
        REQUIRE(std::isnan(curvature));
    }
    
    SECTION("Empty line returns NaN") {
        Line2D empty_line;
        
        float curvature = calculateLineCurvature(empty_line, params);
        
        REQUIRE(std::isnan(curvature));
    }
    
    SECTION("Two-point line") {
        auto line_data = curvature_scenarios::two_point_line();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float curvature = calculateLineCurvature(line, params);
        
        // Two-point line may return NaN or a valid value depending on polynomial fit
        // Just ensure it doesn't crash
        (void)curvature;
    }
    
    SECTION("Smooth curve") {
        auto line_data = curvature_scenarios::smooth_curve();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float curvature = calculateLineCurvature(line, params);
        
        REQUIRE(!std::isnan(curvature));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Element Transform: LineCurvatureParams - JSON Loading",
          "[transforms][v2][params][json][line_curvature]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "position": 0.5,
            "method": "PolynomialFit",
            "polynomial_order": 4,
            "fitting_window_percentage": 0.2
        })";
        
        auto result = loadParametersFromJson<LineCurvatureParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.5f, 0.001f));
        REQUIRE(params.getMethod() == LineCurvatureMethod::PolynomialFit);
        REQUIRE(params.getPolynomialOrder() == 4);
        REQUIRE_THAT(params.getFittingWindowPercentage(), WithinAbs(0.2f, 0.001f));
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LineCurvatureParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.5f, 0.001f));
        REQUIRE(params.getMethod() == LineCurvatureMethod::PolynomialFit);
        REQUIRE(params.getPolynomialOrder() == 3);
        REQUIRE_THAT(params.getFittingWindowPercentage(), WithinAbs(0.1f, 0.001f));
    }
    
    SECTION("JSON round-trip preserves values") {
        LineCurvatureParams original;
        original.position = 0.75f;
        original.method = "PolynomialFit";
        original.polynomial_order = 5;
        original.fitting_window_percentage = 0.15f;
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LineCurvatureParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE_THAT(recovered.getPosition(), WithinAbs(0.75f, 0.001f));
        REQUIRE(recovered.getMethod() == LineCurvatureMethod::PolynomialFit);
        REQUIRE(recovered.getPolynomialOrder() == 5);
        REQUIRE_THAT(recovered.getFittingWindowPercentage(), WithinAbs(0.15f, 0.001f));
    }
}

// ============================================================================
// Tests: Parameter validate() method
// ============================================================================

TEST_CASE("V2 Element Transform: LineCurvatureParams - validate()",
          "[transforms][v2][params][line_curvature]") {
    
    SECTION("Clamps position to [0, 1]") {
        LineCurvatureParams params;
        params.position = -0.5f;
        params.validate();
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.0f, 0.001f));
        
        params.position = 1.5f;
        params.validate();
        REQUIRE_THAT(params.getPosition(), WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Clamps fitting_window_percentage to [0, 1]") {
        LineCurvatureParams params;
        params.fitting_window_percentage = -0.1f;
        params.validate();
        REQUIRE_THAT(params.getFittingWindowPercentage(), WithinAbs(0.0f, 0.001f));
        
        params.fitting_window_percentage = 1.5f;
        params.validate();
        REQUIRE_THAT(params.getFittingWindowPercentage(), WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Clamps polynomial_order to [2, 9]") {
        LineCurvatureParams params;
        params.polynomial_order = 0;
        params.validate();
        REQUIRE(params.getPolynomialOrder() == 2);
        
        params.polynomial_order = 15;
        params.validate();
        REQUIRE(params.getPolynomialOrder() == 9);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineCurvature Registry Integration",
          "[transforms][v2][registry][element][line_curvature]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("CalculateLineCurvature"));
    }
    
    SECTION("Context-aware version is registered") {
        REQUIRE(registry.hasElementTransform("CalculateLineCurvatureWithContext"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("CalculateLineCurvature");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "CalculateLineCurvature");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineCurvature via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_curvature][json]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate with scenario data
    auto two_timesteps_data = curvature_scenarios::json_pipeline_two_timesteps();
    two_timesteps_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_two_timesteps_line", two_timesteps_data, TimeKey("default"));
    
    auto multiple_curvatures_data = curvature_scenarios::json_pipeline_multiple_curvatures();
    multiple_curvatures_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_multiple_curvatures_line", multiple_curvatures_data, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_curvature_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Two timesteps pipeline - parabola and straight line") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Curvature Pipeline",
                    "description": "Test line curvature calculation",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineCurvature",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_line_curvatures",
                        "parameters": {
                            "position": 0.5,
                            "method": "PolynomialFit",
                            "polynomial_order": 3,
                            "fitting_window_percentage": 0.1
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "two_timesteps_curvature_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_curvatures = dm.getData<RaggedAnalogTimeSeries>("v2_line_curvatures");
        REQUIRE(result_curvatures != nullptr);
        
        // Check we have 2 results (t=100 and t=200)
        REQUIRE(result_curvatures->getNumTimePoints() == 2);
        
        // Verify curvature values
        // t=100: parabola (should have non-zero curvature)
        // t=200: straight line (should have near-zero curvature)
        auto values_t100 = result_curvatures->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(!std::isnan(values_t100[0]));
        // Parabola should have positive curvature
        REQUIRE(values_t100[0] != 0.0f);
        
        auto values_t200 = result_curvatures->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(!std::isnan(values_t200[0]));
        // Straight line should have near-zero curvature
        REQUIRE(std::abs(values_t200[0]) < 0.1f);
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Multiple curvatures pipeline with three timesteps") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Multiple Curvatures Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineCurvature",
                        "input_key": "json_pipeline_multiple_curvatures_line",
                        "output_key": "v2_multiple_curvatures",
                        "parameters": {
                            "position": 0.5,
                            "method": "PolynomialFit",
                            "polynomial_order": 3,
                            "fitting_window_percentage": 0.1
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "multiple_curvatures_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_curvatures = dm.getData<RaggedAnalogTimeSeries>("v2_multiple_curvatures");
        REQUIRE(result_curvatures != nullptr);
        REQUIRE(result_curvatures->getNumTimePoints() == 3);
        
        // Verify all values are valid
        auto values_t100 = result_curvatures->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(!std::isnan(values_t100[0]));
        
        auto values_t200 = result_curvatures->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(!std::isnan(values_t200[0]));
        
        auto values_t300 = result_curvatures->getDataAtTime(TimeFrameIndex(300));
        REQUIRE(values_t300.size() == 1);
        REQUIRE(!std::isnan(values_t300[0]));
        
        // Straight line at t=200 should have smallest curvature
        REQUIRE(std::abs(values_t200[0]) < std::abs(values_t100[0]));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Different polynomial order via JSON") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Polynomial Order Test Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineCurvature",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_poly4_curvatures",
                        "parameters": {
                            "position": 0.5,
                            "method": "PolynomialFit",
                            "polynomial_order": 4,
                            "fitting_window_percentage": 0.15
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "polynomial_order_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_curvatures = dm.getData<RaggedAnalogTimeSeries>("v2_poly4_curvatures");
        REQUIRE(result_curvatures != nullptr);
        REQUIRE(result_curvatures->getNumTimePoints() == 2);
        
        // Both values should be valid
        auto values_t100 = result_curvatures->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(!std::isnan(values_t100[0]));
        
        auto values_t200 = result_curvatures->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(!std::isnan(values_t200[0]));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Different position via JSON") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Position Test Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineCurvature",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_position_curvatures",
                        "parameters": {
                            "position": 0.25,
                            "method": "PolynomialFit",
                            "polynomial_order": 3,
                            "fitting_window_percentage": 0.1
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "position_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_curvatures = dm.getData<RaggedAnalogTimeSeries>("v2_position_curvatures");
        REQUIRE(result_curvatures != nullptr);
        REQUIRE(result_curvatures->getNumTimePoints() == 2);
        
        // Both values should be valid
        auto values_t100 = result_curvatures->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(!std::isnan(values_t100[0]));
        
        auto values_t200 = result_curvatures->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(!std::isnan(values_t200[0]));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}

// ============================================================================
// Tests: Context-Aware Execution
// ============================================================================

TEST_CASE("V2 Element Transform: LineCurvature - Context Support",
          "[transforms][v2][element][line_curvature][context]") {
    
    auto line_data = curvature_scenarios::parabola();
    TimeFrameIndex timestamp(100);
    auto line = getLineAt(line_data.get(), timestamp);
    
    LineCurvatureParams params;
    params.position = 0.5f;
    params.polynomial_order = 3;
    params.fitting_window_percentage = 0.1f;
    
    SECTION("Normal execution with context") {
        ComputeContext ctx;
        float progress_reported = -1.0f;
        ctx.progress = [&progress_reported](float p) {
            progress_reported = p;
        };
        ctx.is_cancelled = []() { return false; };
        
        float curvature = calculateLineCurvatureWithContext(line, params, ctx);
        
        REQUIRE(!std::isnan(curvature));
        REQUIRE(progress_reported >= 0.0f);
    }
    
    SECTION("Cancelled execution returns NaN") {
        ComputeContext ctx;
        ctx.is_cancelled = []() { return true; };
        
        float curvature = calculateLineCurvatureWithContext(line, params, ctx);
        
        REQUIRE(std::isnan(curvature));
    }
}
