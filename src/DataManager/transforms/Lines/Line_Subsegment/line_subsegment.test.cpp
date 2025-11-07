#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "Lines/Line_Data.hpp"
#include "transforms/Lines/Line_Subsegment/line_subsegment.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function

TEST_CASE("Data Transform: Extract Line Subsegment - Happy Path", "[transforms][line_subsegment]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> result_subsegments;
    LineSubsegmentParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Direct method - middle subsegment") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;
        params.preserve_original_spacing = true;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);
        
        // Should include points from 20% to 80% of the line
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() >= 2); // At least start and end points
        
        // Check that we have points within the specified range
        // With preserve_original_spacing=true, we get original points within the range
        // plus interpolated start/end points if needed
        // For a line from (0,0) to (4,4), 20% to 80% covers distance 0.8 to 3.2
        // This includes original points at positions 1, 2, 3
        
        // First point should be the first original point within range (position 1.0)
        REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        
        // Last point should be the last original point within range (position 3.0)
        REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(3.0f, 0.001f));

        progress_val = -1;
        call_count = 0;
        result_subsegments = extract_line_subsegment(line_data.get(), params, cb);
        REQUIRE(result_subsegments != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count >= 1); // At least one call
    }

    SECTION("Direct method - full line") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(200), x_coords, y_coords);
        
        params.start_position = 0.0f;
        params.end_position = 1.0f;
        params.method = SubsegmentExtractionMethod::Direct;
        params.preserve_original_spacing = true;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(200));
        REQUIRE(subsegments.size() == 1);
        
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() == 4); // All original points
        
        // Should match original line exactly
        REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[3].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment[3].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Parametric method - middle subsegment") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(300), x_coords, y_coords);
        
        params.start_position = 0.3f;
        params.end_position = 0.7f;
        params.method = SubsegmentExtractionMethod::Parametric;
        params.polynomial_order = 3;
        params.output_points = 20;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(300));
        REQUIRE(subsegments.size() == 1);
        
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() == 20); // Exactly output_points
        
        // Check that the subsegment is within the specified range
        // Parametric method generates exactly output_points with smooth interpolation
        float total_length = 4.0f;
        float start_distance = 0.3f * total_length; // 1.2
        float end_distance = 0.7f * total_length;   // 2.8
        
        // First point should be at start_distance
        REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(1.2f, 0.1f));
        REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(1.2f, 0.1f));
        
        // Last point should be at end_distance
        REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(2.8f, 0.1f));
        REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(2.8f, 0.1f));
    }

    SECTION("Parametric method - small subsegment") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(400), x_coords, y_coords);
        
        params.start_position = 0.45f;
        params.end_position = 0.55f;
        params.method = SubsegmentExtractionMethod::Parametric;
        params.polynomial_order = 2;
        params.output_points = 10;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(400));
        REQUIRE(subsegments.size() == 1);
        
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() == 10);
        
        // Check that the subsegment is within the specified range
        // Parametric method generates exactly output_points with smooth interpolation
        float total_length = 4.0f;
        float start_distance = 0.45f * total_length; // 1.8
        float end_distance = 0.55f * total_length;   // 2.2
        
        // First point should be at start_distance
        REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(1.8f, 0.1f));
        REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(1.8f, 0.1f));
        
        // Last point should be at end_distance
        REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(2.2f, 0.1f));
        REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(2.2f, 0.1f));
    }

    SECTION("Progress callback detailed check") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(500), x_coords, y_coords);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;
        params.preserve_original_spacing = true;

        progress_val = 0;
        call_count = 0;
        std::vector<int> progress_values_seen;
        ProgressCallback detailed_cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
            progress_values_seen.push_back(p);
        };

        result_subsegments = extract_line_subsegment(line_data.get(), params, detailed_cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count >= 2); // At least 0 and 100

        // Check that we see progress values
        REQUIRE(!progress_values_seen.empty());
        REQUIRE(progress_values_seen.back() == 100);
    }
}

TEST_CASE("Data Transform: Extract Line Subsegment - Error and Edge Cases", "[transforms][line_subsegment]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> result_subsegments;
    LineSubsegmentParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input LineData") {
        line_data = nullptr; // Deliberately null
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        REQUIRE(result_subsegments->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_subsegments = extract_line_subsegment(line_data.get(), params, cb);
        REQUIRE(result_subsegments != nullptr);
        REQUIRE(result_subsegments->getTimesWithData().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1); // Called once with 100
    }

    SECTION("Empty LineData (no lines)") {
        line_data = std::make_shared<LineData>();
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        REQUIRE(result_subsegments->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_subsegments = extract_line_subsegment(line_data.get(), params, cb);
        REQUIRE(result_subsegments != nullptr);
        REQUIRE(result_subsegments->getTimesWithData().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1); // Called once with 100
    }

    SECTION("Single point line") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {1.0f};
        std::vector<float> y_coords = {2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);
        REQUIRE(subsegments[0].size() == 1); // Single point
        REQUIRE_THAT(subsegments[0][0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegments[0][0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }

    SECTION("Invalid position range (start >= end)") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = 0.8f;
        params.end_position = 0.2f; // Invalid: start > end
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.empty()); // No valid subsegment
    }

    SECTION("Position values clamped to valid range") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = -0.5f; // Invalid: negative
        params.end_position = 1.5f;    // Invalid: > 1.0
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);
        
        // Should be clamped to [0.0, 1.0] range
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() >= 2);
        REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Parametric method with insufficient points") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {0.0f, 1.0f}; // Only 2 points
        std::vector<float> y_coords = {0.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Parametric;
        params.polynomial_order = 3; // Requires at least 4 points

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);
        
        // Should fall back to direct method
        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() >= 1);
    }

    SECTION("Zero-length line") {
        line_data = std::make_shared<LineData>();
        std::vector<float> x_coords = {1.0f, 1.0f, 1.0f}; // All same point
        std::vector<float> y_coords = {2.0f, 2.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = SubsegmentExtractionMethod::Direct;

        result_subsegments = extract_line_subsegment(line_data.get(), params);
        REQUIRE(result_subsegments != nullptr);
        
        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);
        REQUIRE(subsegments[0].size() == 1); // Single point
        REQUIRE_THAT(subsegments[0][0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegments[0][0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Extract Line Subsegment - JSON pipeline", "[transforms][line_subsegment][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "subsegment_step_1"},
            {"transform_name", "Extract Line Subsegment"},
            {"input_key", "TestLine.line1"},
            {"output_key", "ExtractedSubsegments"},
            {"parameters", {
                {"start_position", 0.2},
                {"end_position", 0.8},
                {"method", "Direct"},
                {"preserve_original_spacing", true}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data
    auto line_data = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
    line_data->setTimeFrame(time_frame);
    dm.setData("TestLine.line1", line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto result_subsegments = dm.getData<LineData>("ExtractedSubsegments");
    REQUIRE(result_subsegments != nullptr);

    auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
    REQUIRE(subsegments.size() == 1);
    
    auto const & subsegment = subsegments[0];
    REQUIRE(subsegment.size() >= 2);
    
    // Check that the subsegment is within the specified range
    // With preserve_original_spacing=true, we get original points within the range
    float total_length = 4.0f;
    float start_distance = 0.2f * total_length; // 0.8
    float end_distance = 0.8f * total_length;   // 3.2
    
    // First point should be the first original point within range (position 1.0)
    REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    // Last point should be the last original point within range (position 3.0)
    REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Extract Line Subsegment - Parameter Factory", "[transforms][line_subsegment][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineSubsegmentParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"start_position", 0.3},
        {"end_position", 0.7},
        {"method", "Parametric"},
        {"polynomial_order", 4},
        {"output_points", 100},
        {"preserve_original_spacing", false}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Extract Line Subsegment", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LineSubsegmentParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->start_position == 0.3f);
    REQUIRE(params->end_position == 0.7f);
    REQUIRE(params->method == SubsegmentExtractionMethod::Parametric);
    REQUIRE(params->polynomial_order == 4);
    REQUIRE(params->output_points == 100);
    REQUIRE(params->preserve_original_spacing == false);
}

TEST_CASE("Data Transform: Extract Line Subsegment - load_data_from_json_config", "[transforms][line_subsegment][json_config]") {
    // Create DataManager and populate it with LineData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data in code
    auto test_line = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
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
        "            \"name\": \"Line Subsegment Extraction Pipeline\",\n"
        "            \"description\": \"Test line subsegment extraction\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Line Subsegment\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_subsegments\",\n"
        "                \"parameters\": {\n"
        "                    \"start_position\": 0.2,\n"
        "                    \"end_position\": 0.8,\n"
        "                    \"method\": \"Direct\",\n"
        "                    \"preserve_original_spacing\": true\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_subsegment_pipeline_test";
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
    auto result_subsegments = dm.getData<LineData>("extracted_subsegments");
    REQUIRE(result_subsegments != nullptr);
    
    // Verify the subsegment extraction results
    auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
    REQUIRE(subsegments.size() == 1);
    
    auto const & subsegment = subsegments[0];
    REQUIRE(subsegment.size() >= 2);
    
    // Check that the subsegment is within the specified range
    // With preserve_original_spacing=true, we get original points within the range
    float total_length = 4.0f;
    float start_distance = 0.2f * total_length; // 0.8
    float end_distance = 0.8f * total_length;   // 3.2
    
    // First point should be the first original point within range (position 1.0)
    REQUIRE_THAT(subsegment[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subsegment[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    // Last point should be the last original point within range (position 3.0)
    REQUIRE_THAT(subsegment.back().x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(subsegment.back().y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    
    // Test another pipeline with different parameters (parametric method)
    const char* json_config_parametric = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Subsegment Extraction with Parametric Method\",\n"
        "            \"description\": \"Test line subsegment extraction with parametric interpolation\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Line Subsegment\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_subsegments_parametric\",\n"
        "                \"parameters\": {\n"
        "                    \"start_position\": 0.3,\n"
        "                    \"end_position\": 0.7,\n"
        "                    \"method\": \"Parametric\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"output_points\": 50\n"
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
    auto result_subsegments_parametric = dm.getData<LineData>("extracted_subsegments_parametric");
    REQUIRE(result_subsegments_parametric != nullptr);
    
    auto const & subsegments_parametric = result_subsegments_parametric->getAtTime(TimeFrameIndex(100));
    REQUIRE(subsegments_parametric.size() == 1);
    
    auto const & subsegment_parametric = subsegments_parametric[0];
    REQUIRE(subsegment_parametric.size() == 50); // Exactly output_points
    
    // Check that the parametric subsegment is within the specified range
    float start_distance_parametric = 0.3f * total_length; // 1.2
    float end_distance_parametric = 0.7f * total_length;   // 2.8
    
    REQUIRE_THAT(subsegment_parametric[0].x, Catch::Matchers::WithinAbs(1.2f, 0.1f));
    REQUIRE_THAT(subsegment_parametric[0].y, Catch::Matchers::WithinAbs(1.2f, 0.1f));
    REQUIRE_THAT(subsegment_parametric.back().x, Catch::Matchers::WithinAbs(2.8f, 0.1f));
    REQUIRE_THAT(subsegment_parametric.back().y, Catch::Matchers::WithinAbs(2.8f, 0.1f));
    
    // Test full line extraction
    const char* json_config_full = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Subsegment Full Line Extraction\",\n"
        "            \"description\": \"Test full line extraction\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Extract Line Subsegment\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"extracted_subsegments_full\",\n"
        "                \"parameters\": {\n"
        "                    \"start_position\": 0.0,\n"
        "                    \"end_position\": 1.0,\n"
        "                    \"method\": \"Direct\",\n"
        "                    \"preserve_original_spacing\": true\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_full = test_dir / "pipeline_config_full.json";
    {
        std::ofstream json_file(json_filepath_full);
        REQUIRE(json_file.is_open());
        json_file << json_config_full;
        json_file.close();
    }
    
    // Execute the full line pipeline
    auto data_info_list_full = load_data_from_json_config(&dm, json_filepath_full.string());
    
    // Verify the full line results
    auto result_subsegments_full = dm.getData<LineData>("extracted_subsegments_full");
    REQUIRE(result_subsegments_full != nullptr);
    
    auto const & subsegments_full = result_subsegments_full->getAtTime(TimeFrameIndex(100));
    REQUIRE(subsegments_full.size() == 1);
    
    auto const & subsegment_full = subsegments_full[0];
    REQUIRE(subsegment_full.size() == 5); // All original points
    
    // Should match original line exactly
    REQUIRE_THAT(subsegment_full[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(subsegment_full[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(subsegment_full.back().x, Catch::Matchers::WithinAbs(4.0f, 0.001f));
    REQUIRE_THAT(subsegment_full.back().y, Catch::Matchers::WithinAbs(4.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
