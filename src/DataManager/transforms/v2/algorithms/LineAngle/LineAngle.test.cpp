#include "LineAngle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/LineAngleTestFixture.hpp"

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
// CalculateLineAngle is registered at compile-time via RegisterTransform
// RAII helper in RegisteredTransforms.cpp. The ElementRegistry::instance()
// singleton already has this transform available when tests run.

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
// Tests: Algorithm Correctness (using fixture)
// ============================================================================

TEST_CASE_METHOD(LineAngleTestFixture,
                 "V2 Element Transform: LineAngle - Core Functionality",
                 "[transforms][v2][element][line_angle]") {
    
    LineAngleParams params;
    
    SECTION("Horizontal line - 0 degrees") {
        auto line_data = m_line_data["horizontal_line"];
        TimeFrameIndex timestamp(10);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.33f;
        params.method = "DirectPoints";
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Vertical line - 90 degrees") {
        auto line_data = m_line_data["vertical_line"];
        TimeFrameIndex timestamp(20);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.25f;
        params.method = "DirectPoints";
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(90.0f, 0.001f));
    }
    
    SECTION("45-degree diagonal line") {
        auto line_data = m_line_data["diagonal_45_degrees"];
        TimeFrameIndex timestamp(30);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = "DirectPoints";
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Polynomial fit on parabola") {
        auto line_data = m_line_data["parabola"];
        TimeFrameIndex timestamp(70);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.4f;
        params.method = "PolynomialFit";
        params.polynomial_order = 2;
        
        float angle = calculateLineAngle(line, params);
        
        // For a parabola y = x², the derivative is 2x. So slope at ~x=3 is ~6
        // Angle of atan(6,1) is approximately 80.537 degrees
        REQUIRE(angle > 75.0f);
        REQUIRE(angle < 85.0f);
    }
    
    SECTION("Different polynomial orders produce different results") {
        auto line_data = m_line_data["smooth_curve"];
        TimeFrameIndex timestamp(80);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params1;
        params1.position = 0.5f;
        params1.method = "PolynomialFit";
        params1.polynomial_order = 1;
        
        LineAngleParams params3;
        params3.position = 0.5f;
        params3.method = "PolynomialFit";
        params3.polynomial_order = 3;
        
        LineAngleParams params5;
        params5.position = 0.5f;
        params5.method = "PolynomialFit";
        params5.polynomial_order = 5;
        
        float angle1 = calculateLineAngle(line, params1);
        float angle3 = calculateLineAngle(line, params3);
        float angle5 = calculateLineAngle(line, params5);
        
        // All angles should be valid
        REQUIRE(angle1 >= -180.0f);
        REQUIRE(angle1 <= 180.0f);
        REQUIRE(angle3 >= -180.0f);
        REQUIRE(angle3 <= 180.0f);
        REQUIRE(angle5 >= -180.0f);
        REQUIRE(angle5 <= 180.0f);
        
        // The angles should be different due to different polynomial orders
        REQUIRE((std::abs(angle1 - angle3) > 1.0f || std::abs(angle1 - angle5) > 1.0f));
    }
}

TEST_CASE_METHOD(LineAngleTestFixture,
                 "V2 Element Transform: LineAngle - Reference Vector",
                 "[transforms][v2][element][line_angle]") {
    
    SECTION("Horizontal reference (default)") {
        auto line_data = m_line_data["diagonal_for_reference"];
        TimeFrameIndex timestamp(110);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        params.reference_x = 1.0f;
        params.reference_y = 0.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Vertical reference") {
        auto line_data = m_line_data["diagonal_for_reference"];
        TimeFrameIndex timestamp(110);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        params.reference_x = 0.0f;
        params.reference_y = 1.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(-45.0f, 0.001f));
    }
    
    SECTION("45-degree reference") {
        auto line_data = m_line_data["horizontal_for_reference"];
        TimeFrameIndex timestamp(130);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        params.reference_x = 1.0f;
        params.reference_y = 1.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(-45.0f, 0.001f));
    }
    
    SECTION("Reference vector with polynomial fit - 90 degree difference") {
        auto line_data = m_line_data["parabola_for_reference"];
        TimeFrameIndex timestamp(140);
        auto line = getLineAt(line_data.get(), timestamp);
        
        // Horizontal reference
        LineAngleParams params1;
        params1.position = 0.5f;
        params1.method = "PolynomialFit";
        params1.polynomial_order = 2;
        params1.reference_x = 1.0f;
        params1.reference_y = 0.0f;
        
        // Vertical reference
        LineAngleParams params2;
        params2.position = 0.5f;
        params2.method = "PolynomialFit";
        params2.polynomial_order = 2;
        params2.reference_x = 0.0f;
        params2.reference_y = 1.0f;
        
        float angle1 = calculateLineAngle(line, params1);
        float angle2 = calculateLineAngle(line, params2);
        
        // The difference should be approximately 90 degrees
        float angle_diff = angle1 - angle2;
        if (angle_diff > 180.0f) angle_diff -= 360.0f;
        if (angle_diff <= -180.0f) angle_diff += 360.0f;
        
        REQUIRE_THAT(std::abs(angle_diff), WithinAbs(90.0f, 5.0f));
    }
}

TEST_CASE_METHOD(LineAngleTestFixture,
                 "V2 Element Transform: LineAngle - Edge Cases",
                 "[transforms][v2][element][line_angle][edge]") {
    
    LineAngleParams params;
    params.position = 0.5f;
    
    SECTION("Single point line returns NaN") {
        auto line_data = m_line_data["single_point_line"];
        TimeFrameIndex timestamp(10);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE(std::isnan(angle));
    }
    
    SECTION("Empty line returns NaN") {
        Line2D empty_line;
        
        float angle = calculateLineAngle(empty_line, params);
        
        REQUIRE(std::isnan(angle));
    }
    
    SECTION("Two-point line works correctly") {
        auto line_data = m_line_data["two_point_diagonal"];
        TimeFrameIndex timestamp(20);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Polynomial fallback with too few points") {
        auto line_data = m_line_data["two_point_line"];
        TimeFrameIndex timestamp(40);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = "PolynomialFit";
        params.polynomial_order = 3;  // Requires at least 4 points
        
        float angle = calculateLineAngle(line, params);
        
        // Should fall back to direct method
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Vertical collinear line") {
        auto line_data = m_line_data["vertical_collinear"];
        TimeFrameIndex timestamp(50);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = "PolynomialFit";
        params.polynomial_order = 3;
        
        float angle = calculateLineAngle(line, params);
        
        // Should be close to 90 degrees (vertical line)
        REQUIRE(((angle > 80.0f && angle < 100.0f) || (angle < -80.0f && angle > -100.0f)));
    }
    
    SECTION("Zero reference vector defaults to x-axis") {
        auto line_data = m_line_data["diagonal_45_degrees"];
        TimeFrameIndex timestamp(30);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.reference_x = 0.0f;
        params.reference_y = 0.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Large line (stress test)") {
        auto line_data = m_line_data["large_diagonal_line"];
        TimeFrameIndex timestamp(70);
        auto line = getLineAt(line_data.get(), timestamp);
        
        // Direct method
        LineAngleParams params_direct;
        params_direct.position = 0.5f;
        params_direct.method = "DirectPoints";
        float angle_direct = calculateLineAngle(line, params_direct);
        
        // Polynomial method
        LineAngleParams params_poly;
        params_poly.position = 0.5f;
        params_poly.method = "PolynomialFit";
        params_poly.polynomial_order = 3;
        float angle_poly = calculateLineAngle(line, params_poly);
        
        // Both should produce valid angles close to 45 degrees
        REQUIRE_THAT(angle_direct, WithinAbs(45.0f, 0.001f));
        REQUIRE_THAT(angle_poly, WithinAbs(45.0f, 1.0f));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Element Transform: LineAngleParams - JSON Loading",
          "[transforms][v2][params][json]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "position": 0.5,
            "method": "PolynomialFit",
            "polynomial_order": 5,
            "reference_x": 0.0,
            "reference_y": 1.0
        })";
        
        auto result = loadParametersFromJson<LineAngleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.5f, 0.001f));
        REQUIRE(params.getMethod() == LineAngleMethod::PolynomialFit);
        REQUIRE(params.getPolynomialOrder() == 5);
        REQUIRE_THAT(params.getReferenceX(), WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(params.getReferenceY(), WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LineAngleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.getPosition(), WithinAbs(0.2f, 0.001f));
        REQUIRE(params.getMethod() == LineAngleMethod::DirectPoints);
        REQUIRE(params.getPolynomialOrder() == 3);
        REQUIRE_THAT(params.getReferenceX(), WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(params.getReferenceY(), WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("JSON round-trip preserves values") {
        LineAngleParams original;
        original.position = 0.75f;
        original.method = "PolynomialFit";
        original.polynomial_order = 4;
        original.reference_x = 0.707f;
        original.reference_y = 0.707f;
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LineAngleParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE_THAT(recovered.getPosition(), WithinAbs(0.75f, 0.001f));
        REQUIRE(recovered.getMethod() == LineAngleMethod::PolynomialFit);
        REQUIRE(recovered.getPolynomialOrder() == 4);
        REQUIRE_THAT(recovered.getReferenceX(), WithinAbs(0.707f, 0.001f));
        REQUIRE_THAT(recovered.getReferenceY(), WithinAbs(0.707f, 0.001f));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineAngle Registry Integration",
          "[transforms][v2][registry][element]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("CalculateLineAngle"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("CalculateLineAngle");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "CalculateLineAngle");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE_METHOD(LineAngleTestFixture,
                 "V2 DataManager Integration: LineAngle via load_data_from_json_config_v2",
                 "[transforms][v2][datamanager][line_angle]") {
    
    DataManager* dm = getDataManager();
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_angle_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("Two timesteps pipeline - horizontal and diagonal") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Angle Pipeline",
                    "description": "Test line angle calculation",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineAngle",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_line_angles",
                        "parameters": {
                            "position": 0.5,
                            "method": "DirectPoints"
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
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_angles = dm->getData<RaggedAnalogTimeSeries>("v2_line_angles");
        REQUIRE(result_angles != nullptr);
        
        // Check we have 2 results (t=100 and t=200)
        REQUIRE(result_angles->getNumTimePoints() == 2);
        
        // Verify angle values
        // t=100: horizontal line (0 degrees)
        // t=200: 45-degree line (45 degrees)
        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(0.0f, 0.001f));
        
        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(45.0f, 0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Multiple angles pipeline with three timesteps") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Multiple Angles Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineAngle",
                        "input_key": "json_pipeline_multiple_angles_line",
                        "output_key": "v2_multiple_angles",
                        "parameters": {
                            "position": 0.5,
                            "method": "DirectPoints"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "multiple_angles_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_angles = dm->getData<RaggedAnalogTimeSeries>("v2_multiple_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 3);
        
        // Verify angle values
        // t=100: horizontal (0°), t=200: vertical (90°), t=300: 45-degree (45°)
        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(0.0f, 0.001f));
        
        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(90.0f, 0.001f));
        
        auto values_t300 = result_angles->getDataAtTime(TimeFrameIndex(300));
        REQUIRE(values_t300.size() == 1);
        REQUIRE_THAT(values_t300[0], WithinAbs(45.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Polynomial fit pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Polynomial Fit Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineAngle",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_poly_angles",
                        "parameters": {
                            "position": 0.5,
                            "method": "PolynomialFit",
                            "polynomial_order": 2
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "polynomial_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_angles = dm->getData<RaggedAnalogTimeSeries>("v2_poly_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 2);
        
        // Both angles should be valid
        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(values_t100[0] >= -180.0f);
        REQUIRE(values_t100[0] <= 180.0f);
        
        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(values_t200[0] >= -180.0f);
        REQUIRE(values_t200[0] <= 180.0f);
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Custom reference vector pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Reference Vector Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "CalculateLineAngle",
                        "input_key": "json_pipeline_two_timesteps_line",
                        "output_key": "v2_vertical_ref_angles",
                        "parameters": {
                            "position": 0.5,
                            "method": "DirectPoints",
                            "reference_x": 0.0,
                            "reference_y": 1.0
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "reference_vector_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(dm, json_filepath.string());
        
        // LineData is ragged, so output is RaggedAnalogTimeSeries
        auto result_angles = dm->getData<RaggedAnalogTimeSeries>("v2_vertical_ref_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 2);
        
        // With vertical reference (0,1):
        // t=100: horizontal line - angle = -90 degrees
        // t=200: 45-degree line - angle = -45 degrees
        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(-90.0f, 0.001f));
        
        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(-45.0f, 0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
