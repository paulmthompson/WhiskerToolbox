#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "catch2/catch_approx.hpp"

#include "Lines/Line_Data.hpp"
#include "transforms/Lines/Line_Clip/line_clip.hpp"
#include "transforms/data_transforms.hpp" // For ProgressCallback

#include "fixtures/scenarios/line/clip_scenarios.hpp"

#include <vector>
#include <memory> // std::make_shared
#include <functional> // std::function
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Clip Line by Reference Line - Happy Path", "[transforms][line_clip]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> reference_line_data;
    std::shared_ptr<LineData> result_lines;
    LineClipParameters params;
    volatile int progress_val = -1; // Volatile to prevent optimization issues in test
    volatile int call_count = 0;    // Volatile for the same reason
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("KeepBase - Horizontal line clipped by vertical reference") {
        // Create main line data using scenario
        line_data = line_clip_scenarios::horizontal_line();

        // Create reference line data using scenario
        reference_line_data = line_clip_scenarios::vertical_reference_at_2_5();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        
        // Should keep from start to intersection (x=0 to x=2.5)
        auto const & clipped_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped_lines.size() == 1);
        
        auto const & clipped_line = clipped_lines[0];
        REQUIRE(clipped_line.size() >= 3); // Should have points up to intersection
        
        // Check that the line is clipped at x=2.5
        REQUIRE(clipped_line.back().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped_line.back().y == Catch::Approx(2.0f).margin(0.001f));

        progress_val = -1;
        call_count = 0;
        result_lines = clip_lines(line_data.get(), &params, cb);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("KeepDistal - Horizontal line clipped by vertical reference") {
        // Create main line data using scenario
        line_data = line_clip_scenarios::horizontal_line();

        // Create reference line data using scenario
        reference_line_data = line_clip_scenarios::vertical_reference_at_2_5();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepDistal;

        result_lines = clip_lines(line_data.get(), &params);
        
        // Should keep from intersection to end (x=2.5 to x=4.0)
        auto const & clipped_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped_lines.size() == 1);
        
        auto const & clipped_line = clipped_lines[0];
        REQUIRE(clipped_line.size() >= 3); // Should have points from intersection to end
        
        // Check that the line starts at intersection
        REQUIRE(clipped_line.front().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped_line.front().y == Catch::Approx(2.0f).margin(0.001f));
        
        // Check that it ends at the original end
        REQUIRE(clipped_line.back().x == Catch::Approx(4.0f).margin(0.001f));
        REQUIRE(clipped_line.back().y == Catch::Approx(2.0f).margin(0.001f));
    }

    SECTION("Multiple time frames") {
        // Create main line data with multiple time frames using scenario
        line_data = line_clip_scenarios::multiple_time_frames();

        // Create reference line data using scenario
        reference_line_data = line_clip_scenarios::vertical_reference_at_2_0();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        
        // Check both time frames are processed
        auto times_with_data = result_lines->getTimesWithData();
        REQUIRE(times_with_data.size() == 2);
        
        // Check time frame 100
        auto const & lines_100 = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(lines_100.size() == 1);
        REQUIRE(lines_100[0].back().x == Catch::Approx(2.0f).margin(0.001f));
        
        // Check time frame 200
        auto const & lines_200 = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(lines_200.size() == 1);
        REQUIRE(lines_200[0].back().x == Catch::Approx(2.0f).margin(0.001f));
        REQUIRE(lines_200[0].back().y == Catch::Approx(2.0f).margin(0.001f));
    }

    SECTION("No intersection - line should remain unchanged") {
        // Create main line data using scenario
        line_data = line_clip_scenarios::horizontal_line_short();

        // Create reference line data using scenario (no intersection)
        reference_line_data = line_clip_scenarios::vertical_reference_no_intersection();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        
        // Line should remain unchanged
        auto const & clipped_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped_lines.size() == 1);
        
        auto const & clipped_line = clipped_lines[0];
        REQUIRE(clipped_line.size() == 4); // Same as original
        
        // Check that all points are the same (horizontal_line_short has points at (0,2), (1,2), (2,2), (3,2))
        std::vector<float> expected_x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> expected_y = {2.0f, 2.0f, 2.0f, 2.0f};
        for (size_t i = 0; i < clipped_line.size(); ++i) {
            REQUIRE(clipped_line[i].x == Catch::Approx(expected_x[i]).margin(0.001f));
            REQUIRE(clipped_line[i].y == Catch::Approx(expected_y[i]).margin(0.001f));
        }
    }
}

TEST_CASE("Data Transform: Clip Line by Reference Line - Error and Edge Cases", "[transforms][line_clip]") {
    std::shared_ptr<LineData> line_data;
    std::shared_ptr<LineData> reference_line_data;
    std::shared_ptr<LineData> result_lines;
    LineClipParameters params;
    volatile int progress_val = -1;
    volatile int call_count = 0;
    ProgressCallback cb = [&](int p) {
        progress_val = p;
        call_count = call_count + 1;
    };

    SECTION("Null input LineData") {
        line_data = nullptr;
        reference_line_data = line_clip_scenarios::vertical_reference_at_1_0();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_lines = clip_lines(line_data.get(), &params, cb);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1);
    }

    SECTION("Null reference LineData") {
        line_data = line_clip_scenarios::horizontal_line_short();

        params.reference_line_data = nullptr;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());
    }

    SECTION("Empty LineData") {
        line_data = line_clip_scenarios::empty_line_data();
        reference_line_data = line_clip_scenarios::vertical_reference_at_1_0();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());

        progress_val = -1;
        call_count = 0;
        result_lines = clip_lines(line_data.get(), &params, cb);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());
        REQUIRE(progress_val == 100);
        REQUIRE(call_count == 1);
    }

    SECTION("Empty reference LineData") {
        line_data = line_clip_scenarios::horizontal_line_short();

        reference_line_data = line_clip_scenarios::empty_line_data();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());
    }

    SECTION("Reference frame out of bounds") {
        line_data = line_clip_scenarios::horizontal_line_short();

        reference_line_data = line_clip_scenarios::vertical_reference_at_1_0();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 999; // Out of bounds
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty());
    }

    SECTION("Single point line (too short)") {
        line_data = line_clip_scenarios::single_point_line();

        reference_line_data = line_clip_scenarios::vertical_reference_at_1_0();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        REQUIRE(result_lines != nullptr);
        REQUIRE(result_lines->getTimesWithData().empty()); // Single point lines are skipped
    }

    SECTION("Parallel lines (no intersection)") {
        line_data = line_clip_scenarios::horizontal_line_short();

        reference_line_data = line_clip_scenarios::horizontal_reference_parallel();

        params.reference_line_data = reference_line_data;
        params.reference_frame = 0;
        params.clip_side = ClipSide::KeepBase;

        result_lines = clip_lines(line_data.get(), &params);
        
        // Line should remain unchanged (parallel lines don't intersect)
        auto const & clipped_lines = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped_lines.size() == 1);
        
        auto const & clipped_line = clipped_lines[0];
        REQUIRE(clipped_line.size() == 4); // Same as original
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Clip Line by Reference Line - JSON pipeline", "[transforms][line_clip][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "line_clip_step_1"},
            {"transform_name", "Clip Line by Reference Line"},
            {"input_key", "TestLine.line1"},
            {"output_key", "ClippedLines"},
            {"parameters", {
                {"reference_line_data", "ReferenceLine"},
                {"reference_frame", 0},
                {"clip_side", "KeepBase"}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data using scenario
    auto line_data = line_clip_scenarios::horizontal_line();
    line_data->setTimeFrame(time_frame);
    dm.setData("TestLine.line1", line_data, TimeKey("default"));

    // Create reference line data using scenario
    auto reference_line_data = line_clip_scenarios::vertical_reference_at_2_5();
    reference_line_data->setTimeFrame(time_frame);
    dm.setData("ReferenceLine", reference_line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto clipped_lines = dm.getData<LineData>("ClippedLines");
    REQUIRE(clipped_lines != nullptr);
    
    auto const & result_lines = clipped_lines->getAtTime(TimeFrameIndex(100));
    REQUIRE(result_lines.size() == 1);
    
    auto const & clipped_line = result_lines[0];
    REQUIRE(clipped_line.size() >= 3);
    
    // Check that the line is clipped at x=2.5
    REQUIRE(clipped_line.back().x == Catch::Approx(2.5f).margin(0.001f));
    REQUIRE(clipped_line.back().y == Catch::Approx(2.0f).margin(0.001f));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Clip Line by Reference Line - Parameter Factory", "[transforms][line_clip][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineClipParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"reference_frame", 5},
        {"clip_side", "KeepDistal"}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Clip Line by Reference Line", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LineClipParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->reference_frame == 5);
    REQUIRE(params->clip_side == ClipSide::KeepDistal);
}

TEST_CASE("Data Transform: Clip Line by Reference Line - load_data_from_json_config", "[transforms][line_clip][json_config]") {
    // Create DataManager and populate it with LineData using scenarios
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data using scenario
    auto test_line = line_clip_scenarios::horizontal_line();
    test_line->setTimeFrame(time_frame);
    
    // Create reference line data using scenario
    auto reference_line = line_clip_scenarios::vertical_reference_at_2_5();
    reference_line->setTimeFrame(time_frame);
    
    // Store the line data in DataManager with known keys
    dm.setData("test_line", test_line, TimeKey("default"));
    dm.setData("reference_line", reference_line, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Clip Pipeline\",\n"
        "            \"description\": \"Test line clipping on line data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Clip Line by Reference Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"clipped_lines\",\n"
        "                \"parameters\": {\n"
        "                    \"reference_line_data\": \"reference_line\",\n"
        "                    \"reference_frame\": 0,\n"
        "                    \"clip_side\": \"KeepBase\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_clip_pipeline_test";
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
    auto result_lines = dm.getData<LineData>("clipped_lines");
    REQUIRE(result_lines != nullptr);
    
    // Verify the line clipping results
    auto const & clipped_lines = result_lines->getAtTime(TimeFrameIndex(100));
    REQUIRE(clipped_lines.size() == 1);
    
    auto const & clipped_line = clipped_lines[0];
    REQUIRE(clipped_line.size() >= 3);
    
    // Check that the line is clipped at x=2.5
    REQUIRE(clipped_line.back().x == Catch::Approx(2.5f).margin(0.001f));
    REQUIRE(clipped_line.back().y == Catch::Approx(2.0f).margin(0.001f));
    
    // Test another pipeline with different parameters (KeepDistal)
    const char* json_config_distal = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Clip with KeepDistal\",\n"
        "            \"description\": \"Test line clipping with KeepDistal side\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Clip Line by Reference Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"clipped_lines_distal\",\n"
        "                \"parameters\": {\n"
        "                    \"reference_line_data\": \"reference_line\",\n"
        "                    \"reference_frame\": 0,\n"
        "                    \"clip_side\": \"KeepDistal\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_distal = test_dir / "pipeline_config_distal.json";
    {
        std::ofstream json_file(json_filepath_distal);
        REQUIRE(json_file.is_open());
        json_file << json_config_distal;
        json_file.close();
    }
    
    // Execute the KeepDistal pipeline
    auto data_info_list_distal = load_data_from_json_config(&dm, json_filepath_distal.string());
    
    // Verify the KeepDistal results
    auto result_lines_distal = dm.getData<LineData>("clipped_lines_distal");
    REQUIRE(result_lines_distal != nullptr);
    
    auto const & clipped_lines_distal = result_lines_distal->getAtTime(TimeFrameIndex(100));
    REQUIRE(clipped_lines_distal.size() == 1);
    
    auto const & clipped_line_distal = clipped_lines_distal[0];
    REQUIRE(clipped_line_distal.size() >= 3);
    
    // Check that the line starts at intersection and ends at original end
    REQUIRE(clipped_line_distal.front().x == Catch::Approx(2.5f).margin(0.001f));
    REQUIRE(clipped_line_distal.front().y == Catch::Approx(2.0f).margin(0.001f));
    REQUIRE(clipped_line_distal.back().x == Catch::Approx(4.0f).margin(0.001f));
    REQUIRE(clipped_line_distal.back().y == Catch::Approx(2.0f).margin(0.001f));
    
    // Test multiple time frames
    const char* json_config_multiframe = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Clip Multiple Frames\",\n"
        "            \"description\": \"Test line clipping with multiple time frames\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Clip Line by Reference Line\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line_multi\",\n"
        "                \"output_key\": \"clipped_lines_multi\",\n"
        "                \"parameters\": {\n"
        "                    \"reference_line_data\": \"reference_line\",\n"
        "                    \"reference_frame\": 0,\n"
        "                    \"clip_side\": \"KeepBase\"\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create multi-frame test data using builder
    auto test_line_multi = LineDataBuilder()
        .withCoords(100, {0.0f, 1.0f, 2.0f, 3.0f, 4.0f}, {2.0f, 2.0f, 2.0f, 2.0f, 2.0f})
        .withCoords(200, {0.0f, 1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 2.0f, 3.0f})
        .build();
    test_line_multi->setTimeFrame(time_frame);
    
    dm.setData("test_line_multi", test_line_multi, TimeKey("default"));
    
    std::filesystem::path json_filepath_multi = test_dir / "pipeline_config_multi.json";
    {
        std::ofstream json_file(json_filepath_multi);
        REQUIRE(json_file.is_open());
        json_file << json_config_multiframe;
        json_file.close();
    }
    
    // Execute the multi-frame pipeline
    auto data_info_list_multi = load_data_from_json_config(&dm, json_filepath_multi.string());
    
    // Verify the multi-frame results
    auto result_lines_multi = dm.getData<LineData>("clipped_lines_multi");
    REQUIRE(result_lines_multi != nullptr);
    
    auto times_with_data = result_lines_multi->getTimesWithData();
    REQUIRE(times_with_data.size() == 2);
    
    // Check both time frames are processed correctly
    auto const & lines_100 = result_lines_multi->getAtTime(TimeFrameIndex(100));
    REQUIRE(lines_100.size() == 1);
    REQUIRE(lines_100[0].back().x == Catch::Approx(2.5f).margin(0.001f));
    
    auto const & lines_200 = result_lines_multi->getAtTime(TimeFrameIndex(200));
    REQUIRE(lines_200.size() == 1);
    REQUIRE(lines_200[0].back().x == Catch::Approx(2.5f).margin(0.001f));
    REQUIRE(lines_200[0].back().y == Catch::Approx(2.5f).margin(0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
