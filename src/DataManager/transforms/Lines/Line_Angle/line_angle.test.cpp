#include "transforms/Lines/Line_Angle/line_angle.hpp"
#include "Lines/Line_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <cmath>
#include <nlohmann/json.hpp>

#include "fixtures/scenarios/line/geometry_scenarios.hpp"

// ============================================================================
// Helper Function
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
// Core Functionality Tests (using scenarios)
// ============================================================================

TEST_CASE("Line angle calculation - Core functionality", "[line][angle][transform]") {
    
    SECTION("Calculating angle from empty line data") {
        auto line_data = line_scenarios::empty_line_data();
        auto params = std::make_unique<LineAngleParameters>();
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Direct angle calculation - Horizontal line") {
        auto line_data = line_scenarios::horizontal_line();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.33f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(10));
        // Angle should be 0 degrees (horizontal line points right)
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Vertical line") {
        auto line_data = line_scenarios::vertical_line();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.25f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(20));
        // Angle should be 90 degrees (vertical line points up)
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(90.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Diagonal line (45 degrees)") {
        auto line_data = line_scenarios::diagonal_45_degrees();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.50f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(30));
        // Angle should be 45 degrees
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Multiple time points") {
        auto line_data = line_scenarios::multiple_timesteps();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 3);
        REQUIRE(values.size() == 3);

        // Find time indices and check angles
        auto time40_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(40)));
        auto time50_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(50)));
        auto time60_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(60)));

        // Horizontal line: 0 degrees
        REQUIRE_THAT(values[time40_idx], Catch::Matchers::WithinAbs(0.0f, 0.001f));
        // Vertical line: 90 degrees
        REQUIRE_THAT(values[time50_idx], Catch::Matchers::WithinAbs(90.0f, 0.001f));
        // 45-degree line: 45 degrees
        REQUIRE_THAT(values[time60_idx], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial angle calculation - Simple line") {
        auto line_data = line_scenarios::parabola();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.4f;
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 2;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(70));

        // For a parabola y = x², the derivative is 2x
        REQUIRE(values[0] > 75.0f);
        REQUIRE(values[0] < 85.0f);
    }

    SECTION("Different polynomial orders") {
        auto line_data = line_scenarios::smooth_curve();

        auto position = 0.5f;

        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = position;
        params1->method = AngleCalculationMethod::PolynomialFit;
        params1->polynomial_order = 1;
        auto result1 = line_angle(line_data.get(), params1.get());

        auto params3 = std::make_unique<LineAngleParameters>();
        params3->position = position;
        params3->method = AngleCalculationMethod::PolynomialFit;
        params3->polynomial_order = 3;
        auto result3 = line_angle(line_data.get(), params3.get());

        auto params5 = std::make_unique<LineAngleParameters>();
        params5->position = position;
        params5->method = AngleCalculationMethod::PolynomialFit;
        params5->polynomial_order = 5;
        auto result5 = line_angle(line_data.get(), params5.get());

        REQUIRE(result1->getTimeSeries().size() == 1);
        REQUIRE(result3->getTimeSeries().size() == 1);
        REQUIRE(result5->getTimeSeries().size() == 1);

        auto angle1 = result1->getAnalogTimeSeries()[0];
        auto angle3 = result3->getAnalogTimeSeries()[0];
        auto angle5 = result5->getAnalogTimeSeries()[0];

        REQUIRE(angle1 >= -180.0f);
        REQUIRE(angle1 <= 180.0f);
        REQUIRE(angle3 >= -180.0f);
        REQUIRE(angle3 <= 180.0f);
        REQUIRE(angle5 >= -180.0f);
        REQUIRE(angle5 <= 180.0f);

        REQUIRE((std::abs(angle1 - angle3) > 1.0f || std::abs(angle1 - angle5) > 1.0f));
    }

    SECTION("Verify returned AnalogTimeSeries structure") {
        auto line_data = line_scenarios::horizontal_at_origin();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result != nullptr);
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);

        float angle = result->getAnalogTimeSeries()[0];
        REQUIRE(calculate_mean(*result.get()) == angle);
        REQUIRE(calculate_min(*result.get()) == angle);
        REQUIRE(calculate_max(*result.get()) == angle);
    }

    SECTION("Reference vector - Horizontal reference") {
        auto line_data = line_scenarios::diagonal_for_reference();

        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->reference_x = 1.0f;
        params1->reference_y = 0.0f;
        auto result1 = line_angle(line_data.get(), params1.get());

        REQUIRE_THAT(result1->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Reference vector - Vertical reference") {
        auto line_data = line_scenarios::diagonal_for_reference();

        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;
        auto result2 = line_angle(line_data.get(), params2.get());

        REQUIRE_THAT(result2->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-45.0f, 0.001f));
    }

    SECTION("Reference vector - 45-degree reference") {
        auto line_data = line_scenarios::horizontal_for_reference();

        auto params3 = std::make_unique<LineAngleParameters>();
        params3->position = 0.5f;
        params3->reference_x = 1.0f;
        params3->reference_y = 1.0f;
        auto result3 = line_angle(line_data.get(), params3.get());

        REQUIRE_THAT(result3->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-45.0f, 0.001f));
    }

    SECTION("Reference vector with polynomial fit") {
        auto line_data = line_scenarios::parabola_for_reference();

        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->method = AngleCalculationMethod::PolynomialFit;
        params1->polynomial_order = 2;
        auto result1 = line_angle(line_data.get(), params1.get());

        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->method = AngleCalculationMethod::PolynomialFit;
        params2->polynomial_order = 2;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;
        auto result2 = line_angle(line_data.get(), params2.get());

        float angle1 = result1->getAnalogTimeSeries()[0];
        float angle2 = result2->getAnalogTimeSeries()[0];

        float angle_diff = angle1 - angle2;
        if (angle_diff > 180.0f) angle_diff -= 360.0f;
        if (angle_diff <= -180.0f) angle_diff += 360.0f;

        REQUIRE_THAT(std::abs(angle_diff), Catch::Matchers::WithinAbs(90.0f, 5.0f));
    }
}

// ============================================================================
// Edge Cases Tests (using scenarios)
// ============================================================================

TEST_CASE("Line angle calculation - Edge cases and error handling", "[line][angle][transform][edge]") {

    SECTION("Line with only one point") {
        auto line_data = line_scenarios::single_point_line();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Position out of range") {
        auto line_data = line_scenarios::two_point_diagonal();

        auto params_low = std::make_unique<LineAngleParameters>();
        params_low->position = -0.5f;
        auto result_low = line_angle(line_data.get(), params_low.get());

        auto params_high = std::make_unique<LineAngleParameters>();
        params_high->position = 1.5f;
        auto result_high = line_angle(line_data.get(), params_high.get());

        REQUIRE(result_low->getAnalogTimeSeries().size() == 1);
        REQUIRE(result_high->getAnalogTimeSeries().size() == 1);

        float low_angle = result_low->getAnalogTimeSeries()[0];
        REQUIRE((std::isnan(low_angle) || (-180.0f <= low_angle && low_angle <= 180.0f)));

        float high_angle = result_high->getAnalogTimeSeries()[0];
        REQUIRE_THAT(high_angle, Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial fit with too few points") {
        auto line_data = line_scenarios::two_point_line();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 3;
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial fit with collinear points") {
        auto line_data = line_scenarios::vertical_collinear();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 3;
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);

        float angle = result->getAnalogTimeSeries()[0];
        REQUIRE(((angle > 80.0f && angle < 100.0f) || (angle < -80.0f && angle > -100.0f)));
    }

    SECTION("Null parameters") {
        auto line_data = line_scenarios::simple_diagonal();

        auto result = line_angle(line_data.get(), nullptr);

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Large number of points") {
        auto line_data = line_scenarios::large_diagonal_line();

        auto params_direct = std::make_unique<LineAngleParameters>();
        params_direct->position = 0.5f;
        params_direct->method = AngleCalculationMethod::DirectPoints;
        auto result_direct = line_angle(line_data.get(), params_direct.get());

        auto params_poly = std::make_unique<LineAngleParameters>();
        params_poly->position = 0.5f;
        params_poly->method = AngleCalculationMethod::PolynomialFit;
        params_poly->polynomial_order = 3;
        auto result_poly = line_angle(line_data.get(), params_poly.get());

        REQUIRE(result_direct->getAnalogTimeSeries().size() == 1);
        REQUIRE(result_poly->getAnalogTimeSeries().size() == 1);

        REQUIRE_THAT(result_direct->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
        REQUIRE_THAT(result_poly->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 1.0f));
    }

    SECTION("Zero reference vector") {
        auto line_data = line_scenarios::diagonal_45_degrees();

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->reference_x = 0.0f;
        params->reference_y = 0.0f;
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Normalizing reference vector") {
        auto line_data = line_scenarios::horizontal_for_normalization();

        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->reference_x = 0.0f;
        params1->reference_y = 2.0f;
        auto result1 = line_angle(line_data.get(), params1.get());

        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;
        auto result2 = line_angle(line_data.get(), params2.get());

        REQUIRE_THAT(result1->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(result2->getAnalogTimeSeries()[0], 0.001f));
    }

    SECTION("Specific problematic 2-point lines with negative reference vector") {
        auto line_data1 = line_scenarios::problematic_line_1();
        auto line_data2 = line_scenarios::problematic_line_2();
        
        // Combine them into one LineData for testing
        auto combined_line_data = std::make_shared<LineData>();
        
        // Copy line 1 at t=200
        auto line1 = getLineAt(line_data1.get(), TimeFrameIndex(200));
        std::vector<float> x1, y1;
        for (auto const& pt : line1) {
            x1.push_back(pt.x);
            y1.push_back(pt.y);
        }
        combined_line_data->emplaceAtTime(TimeFrameIndex(200), x1, y1);
        
        // Copy line 2 at t=210
        auto line2 = getLineAt(line_data2.get(), TimeFrameIndex(210));
        std::vector<float> x2, y2;
        for (auto const& pt : line2) {
            x2.push_back(pt.x);
            y2.push_back(pt.y);
        }
        combined_line_data->emplaceAtTime(TimeFrameIndex(210), x2, y2);

        auto params_80 = std::make_unique<LineAngleParameters>();
        params_80->position = 0.8f;
        params_80->reference_x = -1.0f;
        params_80->reference_y = 0.0f;
        params_80->method = AngleCalculationMethod::DirectPoints;
        auto result_80 = line_angle(combined_line_data.get(), params_80.get());

        auto params_100 = std::make_unique<LineAngleParameters>();
        params_100->position = 1.0f;
        params_100->reference_x = -1.0f;
        params_100->reference_y = 0.0f;
        params_100->method = AngleCalculationMethod::DirectPoints;
        auto result_100 = line_angle(combined_line_data.get(), params_100.get());

        // Verify we get results for both lines
        REQUIRE(result_80->getAnalogTimeSeries().size() == 2);
        REQUIRE(result_80->getTimeSeries().size() == 2);
        REQUIRE(result_100->getAnalogTimeSeries().size() == 2);
        REQUIRE(result_100->getTimeSeries().size() == 2);

        // Check that results are not +/- 180 degrees (the problematic values)
        for (size_t i = 0; i < result_80->getAnalogTimeSeries().size(); ++i) {
            float angle_80 = result_80->getAnalogTimeSeries()[i];
            float angle_100 = result_100->getAnalogTimeSeries()[i];
            
            // Results should not be exactly 180 or -180
            REQUIRE(angle_80 != 180.0f);
            REQUIRE(angle_80 != -180.0f);
            REQUIRE(angle_100 != 180.0f);
            REQUIRE(angle_100 != -180.0f);
            
            // Results should be within valid angle range
            REQUIRE(angle_80 >= -180.0f);
            REQUIRE(angle_80 <= 180.0f);
            REQUIRE(angle_100 >= -180.0f);
            REQUIRE(angle_100 <= 180.0f);
            
            // Print the actual values for debugging
            std::cout << "Line " << (i+1) << " at 80%: " << angle_80 << " degrees" << std::endl;
            std::cout << "Line " << (i+1) << " at 100%: " << angle_100 << " degrees" << std::endl;
        }

        // Test with polynomial fit method as well
        auto params_poly_80 = std::make_unique<LineAngleParameters>();
        params_poly_80->position = 0.8f;
        params_poly_80->reference_x = -1.0f;
        params_poly_80->reference_y = 0.0f;
        params_poly_80->method = AngleCalculationMethod::PolynomialFit;
        params_poly_80->polynomial_order = 1; // Linear fit for 2 points
        auto result_poly_80 = line_angle(combined_line_data.get(), params_poly_80.get());

        // Polynomial fit should fall back to direct method for 2 points
        REQUIRE(result_poly_80->getAnalogTimeSeries().size() == 2);
        for (size_t i = 0; i < result_poly_80->getAnalogTimeSeries().size(); ++i) {
            float angle_poly = result_poly_80->getAnalogTimeSeries()[i];
            REQUIRE(angle_poly != 180.0f);
            REQUIRE(angle_poly != -180.0f);
            std::cout << "Line " << (i+1) << " polynomial at 80%: " << angle_poly << " degrees" << std::endl;
        }
    }
}

#include "DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Line Angle - JSON pipeline", "[transforms][line_angle][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "line_angle_step_1"},
            {"transform_name", "Calculate Line Angle"},
            {"input_key", "TestLine.line1"},
            {"output_key", "LineAngles"},
            {"parameters", {
                {"position", 0.5},
                {"method", "Direct Points"},
                {"polynomial_order", 3},
                {"reference_x", 1.0},
                {"reference_y", 0.0}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data - 45-degree line
    auto line_data = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
    line_data->setTimeFrame(time_frame);
    dm.setData("TestLine.line1", line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto angle_series = dm.getData<AnalogTimeSeries>("LineAngles");
    REQUIRE(angle_series != nullptr);
    REQUIRE(angle_series->getAnalogTimeSeries().size() == 1);
    REQUIRE(angle_series->getTimeSeries().size() == 1);
    REQUIRE(angle_series->getTimeSeries()[0] == TimeFrameIndex(100));
    // Should be 45 degrees for a 45-degree line
    REQUIRE_THAT(angle_series->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Line Angle - Parameter Factory", "[transforms][line_angle][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineAngleParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"position", 0.75},
        {"method", "Polynomial Fit"},
        {"polynomial_order", 5},
        {"reference_x", 0.0},
        {"reference_y", 1.0}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Calculate Line Angle", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LineAngleParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->position == 0.75f);
    REQUIRE(params->method == AngleCalculationMethod::PolynomialFit);
    REQUIRE(params->polynomial_order == 5);
    REQUIRE(params->reference_x == 0.0f);
    REQUIRE(params->reference_y == 1.0f);
}

TEST_CASE("Data Transform: Line Angle - load_data_from_json_config", "[transforms][line_angle][json_config]") {
    // Create DataManager and populate it with LineData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data in code - horizontal line
    auto test_line = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_coords = {1.0f, 1.0f, 1.0f, 1.0f}; // Horizontal line
    test_line->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
    test_line->setTimeFrame(time_frame);
    
    // Store the line data in DataManager with a known key
    dm.setData("test_line", test_line, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle Pipeline\",\n"
        "            \"description\": \"Test line angle calculation on line data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Direct Points\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"reference_x\": 1.0,\n"
        "                    \"reference_y\": 0.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_angle_pipeline_test";
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
    auto result_angles = dm.getData<AnalogTimeSeries>("line_angles");
    REQUIRE(result_angles != nullptr);
    
    // Verify the line angle results - horizontal line should have 0 degrees
    REQUIRE(result_angles->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles->getTimeSeries().size() == 1);
    REQUIRE(result_angles->getTimeSeries()[0] == TimeFrameIndex(100));
    REQUIRE_THAT(result_angles->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Test another pipeline with different parameters (polynomial fit)
    const char* json_config_poly = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle with Polynomial Fit\",\n"
        "            \"description\": \"Test line angle calculation with polynomial fitting\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles_poly\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Polynomial Fit\",\n"
        "                    \"polynomial_order\": 2,\n"
        "                    \"reference_x\": 1.0,\n"
        "                    \"reference_y\": 0.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_poly = test_dir / "pipeline_config_poly.json";
    {
        std::ofstream json_file(json_filepath_poly);
        REQUIRE(json_file.is_open());
        json_file << json_config_poly;
        json_file.close();
    }
    
    // Execute the polynomial fit pipeline
    auto data_info_list_poly = load_data_from_json_config(&dm, json_filepath_poly.string());
    
    // Verify the polynomial fit results
    auto result_angles_poly = dm.getData<AnalogTimeSeries>("line_angles_poly");
    REQUIRE(result_angles_poly != nullptr);
    
    // For a horizontal line, polynomial fit should also give 0 degrees
    REQUIRE(result_angles_poly->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles_poly->getTimeSeries().size() == 1);
    REQUIRE_THAT(result_angles_poly->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Test with different reference vector
    const char* json_config_ref = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle with Vertical Reference\",\n"
        "            \"description\": \"Test line angle calculation with vertical reference\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles_ref\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Direct Points\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"reference_x\": 0.0,\n"
        "                    \"reference_y\": 1.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_ref = test_dir / "pipeline_config_ref.json";
    {
        std::ofstream json_file(json_filepath_ref);
        REQUIRE(json_file.is_open());
        json_file << json_config_ref;
        json_file.close();
    }
    
    // Execute the reference vector pipeline
    auto data_info_list_ref = load_data_from_json_config(&dm, json_filepath_ref.string());
    
    // Verify the reference vector results
    auto result_angles_ref = dm.getData<AnalogTimeSeries>("line_angles_ref");
    REQUIRE(result_angles_ref != nullptr);
    
    // With vertical reference (0,1), horizontal line should be -90 degrees
    REQUIRE(result_angles_ref->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles_ref->getTimeSeries().size() == 1);
    REQUIRE_THAT(result_angles_ref->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-90.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
