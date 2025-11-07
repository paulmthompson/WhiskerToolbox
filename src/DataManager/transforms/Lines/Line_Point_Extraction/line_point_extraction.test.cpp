#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "transforms/Lines/Line_Point_Extraction/line_point_extraction.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Extract Point from Line - Happy Path", "[transforms][line_point_extraction]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<PointData> result_points;
    LinePointExtractionParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Direct method - middle position") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points.size() == 1);
        // At 50% position, should be between points 1 and 2 (indices 1 and 2)
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.5f, 0.001f));

        progress_val = -1;
        call_count = 0;
        result_points = extract_line_point(line_data.get(), params, cb);
        REQUIRE(result_points != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 3); // Called with 0, then 100 in loop, then final 100
    }

    SECTION("Direct method - start position") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(200), x_coords, y_coords);
        
        params.position = 0.0f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(200));
        REQUIRE(points.size() == 1);
        // At 0% position, should be at the first point
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Direct method - end position") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(300), x_coords, y_coords);
        
        params.position = 1.0f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(300));
        REQUIRE(points.size() == 1);
        // At 100% position, should be at the last point
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Parametric method - middle position") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(400), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Parametric;
        params.polynomial_order = 3;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(400));
        REQUIRE(points.size() == 1);
        // Parametric method should give a smooth interpolation
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.5f, 0.1f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.5f, 0.1f));
    }

    SECTION("Multiple time points") {
        line_data = std::make_shared<LineData>();
        
        // Line at time 500
        std::vector<float> x1 = {0.0f, 2.0f, 4.0f};
        std::vector<float> y1 = {0.0f, 2.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(500), x1, y1);
        
        // Line at time 600
        std::vector<float> x2 = {0.0f, 1.0f, 2.0f};
        std::vector<float> y2 = {0.0f, 0.0f, 0.0f}; // Horizontal line
        line_data->emplaceAtTime(TimeFrameIndex(600), x2, y2);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        // Check first time point
        auto const & points1 = result_points->getAtTime(TimeFrameIndex(500));
        REQUIRE(points1.size() == 1);
        REQUIRE_THAT(points1[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points1[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        
        // Check second time point
        auto const & points2 = result_points->getAtTime(TimeFrameIndex(600));
        REQUIRE(points2.size() == 1);
        REQUIRE_THAT(points2[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(points2[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Progress callback detailed check") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(700), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
            progress_values_seen.push_back(p);
        };

        result_points = extract_line_point(line_data.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 3); // Called with 0, then 100 in loop, then final 100

        // Check progress sequence
        std::vector<int> expected_progress_sequence = {0, 100, 100};
        REQUIRE_THAT(progress_values_seen, Catch::Matchers::Equals(expected_progress_sequence));
    }
}

TEST_CASE("Data Transform: Extract Point from Line - Error and Edge Cases", "[transforms][line_point_extraction]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<PointData> result_points;
    LinePointExtractionParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input LineData") {
        line_data = nullptr; // Deliberately null
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_points = extract_line_point(line_data.get(), params, cb);
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().empty());
        REQUIRE(progress_val == 100); // Should still call progress callback
        REQUIRE(call_count == 1); // Only one call when line_data is null
    }

    SECTION("Empty LineData (no lines)") {
        line_data = std::make_shared<LineData>();
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_points = extract_line_point(line_data.get(), params, cb);
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1); // Only one call when no times with data
    }

    SECTION("Empty line at time") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {};
        std::vector<float> y_coords = {};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().empty());
    }

    SECTION("Single point line") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {1.5f};
        std::vector<float> y_coords = {2.5f};
        line_data->emplaceAtTime(TimeFrameIndex(200), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(200));
        REQUIRE(points.size() == 1);
        // Single point should return the point regardless of position
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2.5f, 0.001f));
    }

    SECTION("Position out of bounds") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(300), x_coords, y_coords);
        
        params.position = 1.5f; // Out of bounds
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(300));
        REQUIRE(points.size() == 1);
        // Should clamp to the end of the line
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }

    SECTION("Negative position") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(400), x_coords, y_coords);
        
        params.position = -0.5f; // Negative
        params.method = PointExtractionMethod::Direct;
        params.use_interpolation = true;

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(400));
        REQUIRE(points.size() == 1);
        // Should clamp to the start of the line
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Parametric method with insufficient points") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f}; // Only 2 points
        std::vector<float> y_coords = {0.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(500), x_coords, y_coords);
        
        params.position = 0.5f;
        params.method = PointExtractionMethod::Parametric;
        params.polynomial_order = 3; // Need 4 points for order 3

        result_points = extract_line_point(line_data.get(), params);
        REQUIRE(result_points != nullptr);
        
        auto const & points = result_points->getAtTime(TimeFrameIndex(500));
        REQUIRE(points.size() == 1);
        // Should fall back to direct method
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Extract Point from Line - JSON pipeline", "[transforms][line_point_extraction][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "extract_point_step_1"},
            {"transform_name", "Extract Point from Line"},
            {"input_key", "TestLine.line1"},
            {"output_key", "ExtractedPoints"},
            {"parameters", {
                {"position", 0.5},
                {"method", "Direct"},
                {"polynomial_order", 3},
                {"use_interpolation", true}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data
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
    auto point_data = dm.getData<PointData>("ExtractedPoints");
    REQUIRE(point_data != nullptr);

    auto const & points = point_data->getAtTime(TimeFrameIndex(100));
    REQUIRE(points.size() == 1);
    // At 50% position, should be between points 1 and 2
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.5f, 0.001f));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Extract Point from Line - Parameter Factory", "[transforms][line_point_extraction][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LinePointExtractionParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"position", 0.75},
        {"method", "Parametric"},
        {"polynomial_order", 5},
        {"use_interpolation", false}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Extract Point from Line", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LinePointExtractionParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->position == 0.75f);
    REQUIRE(params->method == PointExtractionMethod::Parametric);
    REQUIRE(params->polynomial_order == 5);
    REQUIRE(params->use_interpolation == false);
}

TEST_CASE("Data Transform: Extract Point from Line - load_data_from_json_config", "[transforms][line_point_extraction][json_config]") {
    // Create DataManager and populate it with LineData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data in code
    auto test_line = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
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
        "            \"name\": \"Line Point Extraction Pipeline\",\n"
        "            \"description\": \"Test point extraction from line data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Point from Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_points\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Direct\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"use_interpolation\": true\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_point_extraction_pipeline_test";
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
    auto result_points = dm.getData<PointData>("extracted_points");
    REQUIRE(result_points != nullptr);
    
    // Verify the point extraction results
    auto const & points = result_points->getAtTime(TimeFrameIndex(100));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.5f, 0.001f));
    
    // Test another pipeline with different parameters (parametric method)
    const char* json_config_parametric = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Point Extraction with Parametric Method\",\n"
        "            \"description\": \"Test point extraction with parametric interpolation\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Point from Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_points_parametric\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.25,\n"
        "                    \"method\": \"Parametric\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"use_interpolation\": true\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_parametric = test_dir / "pipeline_config_parametric.json";
    {
        std::ofstream json_file(json_filepath_parametric);
        REQUIRE(json_file.is_open());
        json_file << json_config_parametric;
        json_file.close();
    }
    
    // Execute the parametric pipeline
    auto data_info_list_parametric = load_data_from_json_config(&dm, json_filepath_parametric.string());
    
    // Verify the parametric results
    auto result_points_parametric = dm.getData<PointData>("extracted_points_parametric");
    REQUIRE(result_points_parametric != nullptr);
    
    auto const & points_parametric = result_points_parametric->getAtTime(TimeFrameIndex(100));
    REQUIRE(points_parametric.size() == 1);
    // At 25% position, should be between points 0 and 1
    REQUIRE_THAT(points_parametric[0].x, Catch::Matchers::WithinAbs(0.75f, 0.1f));
    REQUIRE_THAT(points_parametric[0].y, Catch::Matchers::WithinAbs(0.75f, 0.1f));
    
    // Test multiple time points
    const char* json_config_multiple = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Point Extraction Multiple Times\",\n"
        "            \"description\": \"Test point extraction with multiple time points\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Point from Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_points_multiple\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 1.0,\n"
        "                    \"method\": \"Direct\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"use_interpolation\": true\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_multiple = test_dir / "pipeline_config_multiple.json";
    {
        std::ofstream json_file(json_filepath_multiple);
        REQUIRE(json_file.is_open());
        json_file << json_config_multiple;
        json_file.close();
    }
    
    // Execute the multiple time points pipeline
    auto data_info_list_multiple = load_data_from_json_config(&dm, json_filepath_multiple.string());
    
    // Verify the multiple time points results
    auto result_points_multiple = dm.getData<PointData>("extracted_points_multiple");
    REQUIRE(result_points_multiple != nullptr);
    
    auto const & points_multiple = result_points_multiple->getAtTime(TimeFrameIndex(100));
    REQUIRE(points_multiple.size() == 1);
    // At 100% position, should be at the last point
    REQUIRE_THAT(points_multiple[0].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(points_multiple[0].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
