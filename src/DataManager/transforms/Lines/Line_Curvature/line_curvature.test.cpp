#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"

#include "Lines/Line_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/Lines/Line_Curvature/line_curvature.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include "fixtures/builders/LineDataBuilder.hpp"
#include "fixtures/scenarios/line/curvature_scenarios.hpp"

#include <vector>
#include <memory>
#include <functional>

TEST_CASE("Data Transform: Calculate Line Curvature - Happy Path", "[transforms][line_curvature]") {
    std::shared_ptr<AnalogTimeSeries> result_curvature;
    LineCurvatureParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Simple curved line with polynomial fit") {
        auto line_data = curvature_scenarios::parabola();
        
        params.position = 0.5f; // Middle of the line
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        progress_val = -1;
        call_count = 0;
        result_curvature = line_curvature(line_data.get(), &params, cb);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("Straight line (should have low curvature)") {
        auto line_data = curvature_scenarios::straight_line();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
        
        // For a straight line, curvature should be very low (close to 0)
        float curvature_value = result_curvature->getAnalogTimeSeries()[0];
        REQUIRE(std::abs(curvature_value) < 0.1f); // Should be very small for straight line
    }

    SECTION("Different positions along the line") {
        auto line_data = curvature_scenarios::parabola();
        
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        // Test different positions
        params.position = 0.0f; // Start of line
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.position = 0.25f; // Quarter way
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.position = 0.75f; // Three quarters way
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.position = 1.0f; // End of line
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
    }

    SECTION("Different polynomial orders") {
        auto line_data = curvature_scenarios::parabola();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.fitting_window_percentage = 0.1f;

        // Test different polynomial orders
        params.polynomial_order = 2;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.polynomial_order = 3;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.polynomial_order = 4;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
    }

    SECTION("Different fitting window percentages") {
        auto line_data = curvature_scenarios::parabola();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;

        // Test different fitting window percentages
        params.fitting_window_percentage = 0.05f;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.fitting_window_percentage = 0.1f;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());

        params.fitting_window_percentage = 0.2f;
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
    }
}

TEST_CASE("Data Transform: Calculate Line Curvature - Error and Edge Cases", "[transforms][line_curvature]") {
    std::shared_ptr<AnalogTimeSeries> result_curvature;
    LineCurvatureParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input LineData") {
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        result_curvature = line_curvature(nullptr, &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(result_curvature->getAnalogTimeSeries().empty());
    }

    SECTION("Empty LineData (no lines)") {
        auto line_data = curvature_scenarios::empty();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(result_curvature->getAnalogTimeSeries().empty());

        progress_val = -1;
        call_count = 0;
        result_curvature = line_curvature(line_data.get(), &params, cb);
        REQUIRE(result_curvature != nullptr);
        REQUIRE(result_curvature->getAnalogTimeSeries().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1); // Called once with 100
    }

    SECTION("Line with insufficient points for polynomial fit") {
        auto line_data = curvature_scenarios::two_point_line();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        // Should handle insufficient points gracefully
        REQUIRE(result_curvature->getAnalogTimeSeries().empty());
    }

    SECTION("Position outside valid range") {
        auto line_data = curvature_scenarios::parabola();
        
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.polynomial_order = 3;
        params.fitting_window_percentage = 0.1f;

        // Test positions outside valid range
        params.position = -0.1f; // Below 0.0
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        // Should handle invalid position gracefully

        params.position = 1.1f; // Above 1.0
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        // Should handle invalid position gracefully
    }

    SECTION("Invalid polynomial order") {
        auto line_data = curvature_scenarios::parabola();
        
        params.position = 0.5f;
        params.method = CurvatureCalculationMethod::PolynomialFit;
        params.fitting_window_percentage = 0.1f;

        // Test invalid polynomial orders
        params.polynomial_order = 0; // Invalid
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        // Should handle invalid polynomial order gracefully

        params.polynomial_order = 1; // Too low for curvature calculation
        result_curvature = line_curvature(line_data.get(), &params);
        REQUIRE(result_curvature != nullptr);
        // Should handle invalid polynomial order gracefully
    }
}

#include "DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Calculate Line Curvature - JSON pipeline", "[transforms][line_curvature][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "curvature_step_1"},
            {"transform_name", "Calculate Line Curvature"},
            {"input_key", "TestLine.line1"},
            {"output_key", "LineCurvature"},
            {"parameters", {
                {"position", 0.5},
                {"method", "PolynomialFit"},
                {"polynomial_order", 3},
                {"fitting_window_percentage", 0.1}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data using builder
    auto line_data = LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})  // y = x^2
        .build();
    line_data->setTimeFrame(time_frame);
    dm.setData("TestLine.line1", line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto curvature_series = dm.getData<AnalogTimeSeries>("LineCurvature");
    REQUIRE(curvature_series != nullptr);
    REQUIRE(!curvature_series->getAnalogTimeSeries().empty());
    
    // For a parabola, curvature should be non-zero (can be positive or negative due to polynomial fitting)
    float curvature_value = curvature_series->getAnalogTimeSeries()[0];
    REQUIRE(std::abs(curvature_value) > 0.001f); // Should be non-zero
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Calculate Line Curvature - Parameter Factory", "[transforms][line_curvature][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineCurvatureParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"position", 0.75},
        {"method", "PolynomialFit"},
        {"polynomial_order", 4},
        {"fitting_window_percentage", 0.15}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Calculate Line Curvature", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LineCurvatureParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->position == 0.75f);
    REQUIRE(params->method == CurvatureCalculationMethod::PolynomialFit);
    REQUIRE(params->polynomial_order == 4);
    REQUIRE(params->fitting_window_percentage == 0.15f);
}

TEST_CASE("Data Transform: Calculate Line Curvature - load_data_from_json_config", "[transforms][line_curvature][json_config]") {
    // Create DataManager and populate it with LineData using builder
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data using builder (parabola)
    auto test_line_data = LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f})  // y = x^2
        .build();
    test_line_data->setTimeFrame(time_frame);
    
    // Store the line data in DataManager with a known key
    dm.setData("test_line", test_line_data, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Curvature Pipeline\",\n"
        "            \"description\": \"Test line curvature calculation on curved line\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Curvature\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_curvature\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"PolynomialFit\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"fitting_window_percentage\": 0.1\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_curvature_pipeline_test";
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
    auto result_curvature = dm.getData<AnalogTimeSeries>("line_curvature");
    REQUIRE(result_curvature != nullptr);
    REQUIRE(!result_curvature->getAnalogTimeSeries().empty());
    
    // Verify the curvature calculation results
    float curvature_value = result_curvature->getAnalogTimeSeries()[0];
    REQUIRE(std::abs(curvature_value) > 0.001f); // For a parabola, curvature should be non-zero
    
    // Test another pipeline with different parameters (different position)
    const char* json_config_position = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Curvature at Different Position\",\n"
        "            \"description\": \"Test line curvature at different position\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Curvature\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_curvature_position\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.75,\n"
        "                    \"method\": \"PolynomialFit\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"fitting_window_percentage\": 0.1\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_position = test_dir / "pipeline_config_position.json";
    {
        std::ofstream json_file(json_filepath_position);
        REQUIRE(json_file.is_open());
        json_file << json_config_position;
        json_file.close();
    }
    
    // Execute the position pipeline
    auto data_info_list_position = load_data_from_json_config(&dm, json_filepath_position.string());
    
    // Verify the position results
    auto result_curvature_position = dm.getData<AnalogTimeSeries>("line_curvature_position");
    REQUIRE(result_curvature_position != nullptr);
    REQUIRE(!result_curvature_position->getAnalogTimeSeries().empty());
    
    float curvature_value_position = result_curvature_position->getAnalogTimeSeries()[0];
    // Curvature can be negative due to polynomial fitting and numerical derivatives
    // For a parabola, we expect non-zero curvature (either positive or negative)
    REQUIRE(std::abs(curvature_value_position) > 0.001f); // Should be non-zero
    
    // Test different polynomial order
    const char* json_config_polynomial = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Curvature with Different Polynomial Order\",\n"
        "            \"description\": \"Test line curvature with different polynomial order\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Curvature\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_curvature_polynomial\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"PolynomialFit\",\n"
        "                    \"polynomial_order\": 4,\n"
        "                    \"fitting_window_percentage\": 0.1\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_polynomial = test_dir / "pipeline_config_polynomial.json";
    {
        std::ofstream json_file(json_filepath_polynomial);
        REQUIRE(json_file.is_open());
        json_file << json_config_polynomial;
        json_file.close();
    }
    
    // Execute the polynomial order pipeline
    auto data_info_list_polynomial = load_data_from_json_config(&dm, json_filepath_polynomial.string());
    
    // Verify the polynomial order results
    auto result_curvature_polynomial = dm.getData<AnalogTimeSeries>("line_curvature_polynomial");
    REQUIRE(result_curvature_polynomial != nullptr);
    REQUIRE(!result_curvature_polynomial->getAnalogTimeSeries().empty());
    
    float curvature_value_polynomial = result_curvature_polynomial->getAnalogTimeSeries()[0];
    REQUIRE(std::abs(curvature_value_polynomial) > 0.001f); // Should be non-zero
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
