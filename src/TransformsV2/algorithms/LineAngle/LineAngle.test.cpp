#include "LineAngle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/line/geometry_scenarios.hpp"

#include <cmath>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
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

TEST_CASE("V2 Element Transform: LineAngle - Core Functionality",
          "[transforms][v2][element][line_angle]") {
    
    LineAngleParams params;
    
    SECTION("Horizontal line - 0 degrees") {
        auto line_data = line_scenarios::horizontal_line();
        TimeFrameIndex timestamp(10);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.33f;
        params.method = LineAngleMethod::DirectPoints;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Vertical line - 90 degrees") {
        auto line_data = line_scenarios::vertical_line();
        TimeFrameIndex timestamp(20);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.25f;
        params.method = LineAngleMethod::DirectPoints;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(90.0f, 0.001f));
    }
    
    SECTION("45-degree diagonal line") {
        auto line_data = line_scenarios::diagonal_45_degrees();
        TimeFrameIndex timestamp(30);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = LineAngleMethod::DirectPoints;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Polynomial fit on parabola") {
        auto line_data = line_scenarios::parabola();
        TimeFrameIndex timestamp(70);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.4f;
        params.method = LineAngleMethod::PolynomialFit;
        params.polynomial_order = 2;
        
        float angle = calculateLineAngle(line, params);
        
        // For a parabola y = x², the derivative is 2x. So slope at ~x=3 is ~6
        // Angle of atan(6,1) is approximately 80.537 degrees
        REQUIRE(angle > 75.0f);
        REQUIRE(angle < 85.0f);
    }
    
    SECTION("Different polynomial orders produce different results") {
        auto line_data = line_scenarios::smooth_curve();
        TimeFrameIndex timestamp(80);
        auto line = getLineAt(line_data.get(), timestamp);

        float constexpr full_line_window = 1.0f;

        LineAngleParams params1;
        params1.position = 0.5f;
        params1.window = full_line_window;
        params1.method = LineAngleMethod::PolynomialFit;
        params1.polynomial_order = 1;

        LineAngleParams params3;
        params3.position = 0.5f;
        params3.window = full_line_window;
        params3.method = LineAngleMethod::PolynomialFit;
        params3.polynomial_order = 3;

        LineAngleParams params5;
        params5.position = 0.5f;
        params5.window = full_line_window;
        params5.method = LineAngleMethod::PolynomialFit;
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

TEST_CASE("V2 Element Transform: LineAngle - Coordinate frame",
          "[transforms][v2][element][line_angle]") {
    
    SECTION("Default axes (world X right, Y up)") {
        auto line_data = line_scenarios::diagonal_for_reference();
        TimeFrameIndex timestamp(110);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Measured positive X along world +Y") {
        auto line_data = line_scenarios::diagonal_for_reference();
        TimeFrameIndex timestamp(110);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        params.axis_x_x = 0.0f;
        params.axis_x_y = 1.0f;
        params.axis_y_x = -1.0f;
        params.axis_y_y = 0.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(-45.0f, 0.001f));
    }
    
    SECTION("Measured positive X along diagonal") {
        auto line_data = line_scenarios::horizontal_for_reference();
        TimeFrameIndex timestamp(130);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params;
        params.position = 0.5f;
        params.axis_x_x = 1.0f;
        params.axis_x_y = 1.0f;
        params.axis_y_x = 0.0f;
        params.axis_y_y = 1.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(-45.0f, 0.001f));
    }
    
    SECTION("Axis frame with polynomial fit - 90 degree difference") {
        auto line_data = line_scenarios::parabola_for_reference();
        TimeFrameIndex timestamp(140);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineAngleParams params1;
        params1.position = 0.5f;
        params1.method = LineAngleMethod::PolynomialFit;
        params1.polynomial_order = 2;
        
        LineAngleParams params2 = params1;
        params2.axis_x_x = 0.0f;
        params2.axis_x_y = 1.0f;
        params2.axis_y_x = -1.0f;
        params2.axis_y_y = 0.0f;
        
        float angle1 = calculateLineAngle(line, params1);
        float angle2 = calculateLineAngle(line, params2);
        
        float angle_diff = angle1 - angle2;
        if (angle_diff > 180.0f) angle_diff -= 360.0f;
        if (angle_diff <= -180.0f) angle_diff += 360.0f;
        
        REQUIRE_THAT(std::abs(angle_diff), WithinAbs(90.0f, 5.0f));
    }
}

TEST_CASE("V2 Element Transform: LineAngle - Edge Cases",
          "[transforms][v2][element][line_angle][edge]") {
    
    LineAngleParams params;
    params.position = 0.5f;
    
    SECTION("Single point line returns NaN") {
        auto line_data = line_scenarios::single_point_line();
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
        auto line_data = line_scenarios::two_point_diagonal();
        TimeFrameIndex timestamp(20);
        auto line = getLineAt(line_data.get(), timestamp);
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Polynomial fallback with too few points") {
        auto line_data = line_scenarios::two_point_line();
        TimeFrameIndex timestamp(40);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = LineAngleMethod::PolynomialFit;
        params.polynomial_order = 3;  // Requires at least 4 points
        
        float angle = calculateLineAngle(line, params);
        
        // Should fall back to direct method
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Vertical collinear line") {
        auto line_data = line_scenarios::vertical_collinear();
        TimeFrameIndex timestamp(50);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = LineAngleMethod::PolynomialFit;
        params.polynomial_order = 3;
        
        float angle = calculateLineAngle(line, params);
        
        // Should be close to 90 degrees (vertical line)
        REQUIRE(((angle > 80.0f && angle < 100.0f) || (angle < -80.0f && angle > -100.0f)));
    }
    
    SECTION("Degenerate axis X direction uses default basis") {
        auto line_data = line_scenarios::diagonal_45_degrees();
        TimeFrameIndex timestamp(30);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.axis_x_x = 0.0f;
        params.axis_x_y = 0.0f;
        params.axis_y_x = 0.0f;
        params.axis_y_y = 0.0f;
        
        float angle = calculateLineAngle(line, params);
        
        REQUIRE_THAT(angle, WithinAbs(45.0f, 0.001f));
    }
    
    SECTION("Large line (stress test)") {
        auto line_data = line_scenarios::large_diagonal_line();
        TimeFrameIndex timestamp(70);
        auto line = getLineAt(line_data.get(), timestamp);
        
        // Direct method
        LineAngleParams params_direct;
        params_direct.position = 0.5f;
        params_direct.method = LineAngleMethod::DirectPoints;
        float angle_direct = calculateLineAngle(line, params_direct);
        
        // Polynomial method
        LineAngleParams params_poly;
        params_poly.position = 0.5f;
        params_poly.method = LineAngleMethod::PolynomialFit;
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
            "window": 0.15,
            "method": "PolynomialFit",
            "polynomial_order": 5,
            "axis_x_x": 0.0,
            "axis_x_y": 1.0,
            "axis_y_x": -1.0,
            "axis_y_y": 0.0
        })";
        
        auto result = loadParametersFromJson<LineAngleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.position, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(params.window, WithinAbs(0.15f, 0.001f));
        REQUIRE(params.method == LineAngleMethod::PolynomialFit);
        REQUIRE(params.polynomial_order == 5);
        REQUIRE_THAT(params.axis_x_x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(params.axis_x_y, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(params.axis_y_x, WithinAbs(-1.0f, 0.001f));
        REQUIRE_THAT(params.axis_y_y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<LineAngleParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE_THAT(params.position, WithinAbs(0.2f, 0.001f));
        REQUIRE_THAT(params.window, WithinAbs(0.2f, 0.001f));
        REQUIRE(params.method == LineAngleMethod::DirectPoints);
        REQUIRE(params.polynomial_order == 3);
        REQUIRE_THAT(params.axis_x_x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(params.axis_x_y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(params.axis_y_x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(params.axis_y_y, WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("JSON round-trip preserves values") {
        LineAngleParams original;
        original.position = 0.75f;
        original.window = 0.1f;
        original.method = LineAngleMethod::PolynomialFit;
        original.polynomial_order = 4;
        original.axis_x_x = 0.707f;
        original.axis_x_y = 0.707f;
        original.axis_y_x = -0.707f;
        original.axis_y_y = 0.707f;
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<LineAngleParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE_THAT(recovered.position, WithinAbs(0.75f, 0.001f));
        REQUIRE_THAT(recovered.window, WithinAbs(0.1f, 0.001f));
        REQUIRE(recovered.method == LineAngleMethod::PolynomialFit);
        REQUIRE(recovered.polynomial_order == 4);
        REQUIRE_THAT(recovered.axis_x_x, WithinAbs(0.707f, 0.001f));
        REQUIRE_THAT(recovered.axis_x_y, WithinAbs(0.707f, 0.001f));
        REQUIRE_THAT(recovered.axis_y_x, WithinAbs(-0.707f, 0.001f));
        REQUIRE_THAT(recovered.axis_y_y, WithinAbs(0.707f, 0.001f));
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

TEST_CASE("V2 DataManager Integration: LineAngle via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_angle]") {

    using namespace pipeline_json_test;

    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    auto two_timesteps_data = line_scenarios::json_pipeline_two_timesteps();
    two_timesteps_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_two_timesteps_line", two_timesteps_data, TimeKey("default"));

    auto multiple_angles_data = line_scenarios::json_pipeline_multiple_angles();
    multiple_angles_data->setTimeFrame(time_frame);
    dm.setData("json_pipeline_multiple_angles_line", multiple_angles_data, TimeKey("default"));

    SECTION("Two timesteps pipeline - horizontal and diagonal") {
        LineAngleParams params;
        params.position = 0.5f;
        params.method = LineAngleMethod::DirectPoints;

        auto const pipeline = makeSingleStepPipeline(
                "CalculateLineAngle",
                "json_pipeline_two_timesteps_line",
                "v2_line_angles",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_angles = dm.getData<RaggedAnalogTimeSeries>("v2_line_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 2);

        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(0.0f, 0.001f));

        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(45.0f, 0.001f));
    }

    SECTION("Multiple angles pipeline with three timesteps") {
        LineAngleParams params;
        params.position = 0.5f;
        params.method = LineAngleMethod::DirectPoints;

        auto const pipeline = makeSingleStepPipeline(
                "CalculateLineAngle",
                "json_pipeline_multiple_angles_line",
                "v2_multiple_angles",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_angles = dm.getData<RaggedAnalogTimeSeries>("v2_multiple_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 3);

        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(0.0f, 0.001f));

        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(90.0f, 0.001f));

        auto values_t300 = result_angles->getDataAtTime(TimeFrameIndex(300));
        REQUIRE(values_t300.size() == 1);
        REQUIRE_THAT(values_t300[0], WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial fit pipeline") {
        LineAngleParams params;
        params.position = 0.5f;
        params.method = LineAngleMethod::PolynomialFit;
        params.polynomial_order = 2;

        auto const pipeline = makeSingleStepPipeline(
                "CalculateLineAngle",
                "json_pipeline_two_timesteps_line",
                "v2_poly_angles",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_angles = dm.getData<RaggedAnalogTimeSeries>("v2_poly_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 2);

        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE(values_t100[0] >= -180.0f);
        REQUIRE(values_t100[0] <= 180.0f);

        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE(values_t200[0] >= -180.0f);
        REQUIRE(values_t200[0] <= 180.0f);
    }

    SECTION("Custom coordinate frame pipeline") {
        LineAngleParams params;
        params.position = 0.5f;
        params.method = LineAngleMethod::DirectPoints;
        params.axis_x_x = 0.0f;
        params.axis_x_y = 1.0f;
        params.axis_y_x = -1.0f;
        params.axis_y_y = 0.0f;

        auto const pipeline = makeSingleStepPipeline(
                "CalculateLineAngle",
                "json_pipeline_two_timesteps_line",
                "v2_vertical_ref_angles",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_angles = dm.getData<RaggedAnalogTimeSeries>("v2_vertical_ref_angles");
        REQUIRE(result_angles != nullptr);
        REQUIRE(result_angles->getNumTimePoints() == 2);

        auto values_t100 = result_angles->getDataAtTime(TimeFrameIndex(100));
        REQUIRE(values_t100.size() == 1);
        REQUIRE_THAT(values_t100[0], WithinAbs(-90.0f, 0.001f));

        auto values_t200 = result_angles->getDataAtTime(TimeFrameIndex(200));
        REQUIRE(values_t200.size() == 1);
        REQUIRE_THAT(values_t200[0], WithinAbs(-45.0f, 0.001f));
    }
}
