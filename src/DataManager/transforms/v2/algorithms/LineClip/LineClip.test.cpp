#include "LineClip.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/line/clip_scenarios.hpp"

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
// ClipLineAtReference is registered at compile-time via
// RegisterBinaryTransform RAII helper in RegisteredTransforms.cpp.
// The ElementRegistry::instance() singleton already has this transform
// available when tests run.

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const* line_data, TimeFrameIndex time) {
    auto const& lines = line_data->getAtTime(time);
    if (lines.empty()) {
        return Line2D{};
    }
    return lines[0];
}

// ============================================================================
// Tests: Algorithm Correctness (using fixture)
// ============================================================================

TEST_CASE("V2 Binary Element Transform: LineClip - Core Functionality",
          "[transforms][v2][binary_element][line_clip]") {
    
    LineClipParams params;
    
    SECTION("KeepBase - Horizontal line clipped by vertical reference") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_2_5();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(!line.empty());
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Should keep from start to intersection (x=0 to x=2.5)
        REQUIRE(clipped.size() >= 3);
        
        // Check that the line is clipped at x=2.5
        REQUIRE(clipped.back().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped.back().y == Catch::Approx(2.0f).margin(0.001f));
    }
    
    SECTION("KeepDistal - Horizontal line clipped by vertical reference") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_2_5();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(!line.empty());
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepDistal";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Should keep from intersection to end (x=2.5 to x=4.0)
        REQUIRE(clipped.size() >= 3);
        
        // Check that the line starts at intersection
        REQUIRE(clipped.front().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped.front().y == Catch::Approx(2.0f).margin(0.001f));
        
        // Check that it ends at the original end
        REQUIRE(clipped.back().x == Catch::Approx(4.0f).margin(0.001f));
        REQUIRE(clipped.back().y == Catch::Approx(2.0f).margin(0.001f));
    }
    
    SECTION("No intersection - line should remain unchanged") {
        auto line_data = line_clip_scenarios::horizontal_line_short();
        auto reference_data = line_clip_scenarios::vertical_reference_no_intersection();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(!line.empty());
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Line should remain unchanged
        REQUIRE(clipped.size() == 4);
        
        // Check that all points are the same
        std::vector<float> expected_x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> expected_y = {2.0f, 2.0f, 2.0f, 2.0f};
        for (size_t i = 0; i < clipped.size(); ++i) {
            REQUIRE(clipped[i].x == Catch::Approx(expected_x[i]).margin(0.001f));
            REQUIRE(clipped[i].y == Catch::Approx(expected_y[i]).margin(0.001f));
        }
    }
    
    SECTION("Diagonal line clipped by vertical reference") {
        auto line_data = line_clip_scenarios::diagonal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_2_0();
        
        TimeFrameIndex line_time(200);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(!line.empty());
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Should keep from start to intersection at x=2.0
        REQUIRE(clipped.size() >= 2);
        REQUIRE(clipped.back().x == Catch::Approx(2.0f).margin(0.001f));
        REQUIRE(clipped.back().y == Catch::Approx(2.0f).margin(0.001f));
    }
}

TEST_CASE("V2 Binary Element Transform: LineClip - Edge Cases",
          "[transforms][v2][binary_element][line_clip]") {
    
    LineClipParams params;
    
    SECTION("Single point line (too short) returns empty or original") {
        auto line_data = line_clip_scenarios::single_point_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_1_0();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(line.size() == 1);
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Single point lines can't intersect meaningfully - should return as-is
        REQUIRE(clipped.size() == 1);
    }
    
    SECTION("Empty line returns empty") {
        Line2D empty_line{};
        auto reference_data = line_clip_scenarios::vertical_reference_at_1_0();
        
        TimeFrameIndex ref_time(0);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(empty_line, reference_line, params);
        
        REQUIRE(clipped.empty());
    }
    
    SECTION("Empty reference line returns original line") {
        auto line_data = line_clip_scenarios::horizontal_line_short();
        Line2D empty_reference{};
        
        TimeFrameIndex line_time(100);
        auto line = getLineAt(line_data.get(), line_time);
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, empty_reference, params);
        
        // No intersection possible, original line returned
        REQUIRE(clipped.size() == line.size());
    }
    
    SECTION("Parallel lines (no intersection) - line unchanged") {
        auto line_data = line_clip_scenarios::horizontal_line_short();
        auto reference_data = line_clip_scenarios::horizontal_reference_parallel();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        REQUIRE(!line.empty());
        REQUIRE(!reference_line.empty());
        
        params.clip_side = "KeepBase";
        auto clipped = clipLineAtReference(line, reference_line, params);
        
        // Parallel lines don't intersect - original returned
        REQUIRE(clipped.size() == line.size());
    }
}

TEST_CASE("V2 Binary Element Transform: LineClip - Parameter Serialization",
          "[transforms][v2][binary_element][line_clip][serialization]") {
    
    SECTION("Default parameters") {
        LineClipParams params;
        REQUIRE(params.getClipSide() == ClipSide::KeepBase);
    }
    
    SECTION("KeepBase parameter") {
        LineClipParams params;
        params.clip_side = "KeepBase";
        REQUIRE(params.getClipSide() == ClipSide::KeepBase);
    }
    
    SECTION("KeepDistal parameter") {
        LineClipParams params;
        params.clip_side = "KeepDistal";
        REQUIRE(params.getClipSide() == ClipSide::KeepDistal);
    }
    
    SECTION("JSON serialization round-trip") {
        LineClipParams original;
        original.clip_side = "KeepDistal";
        
        std::string json = rfl::json::write(original);
        auto parsed = rfl::json::read<LineClipParams>(json);
        
        REQUIRE(parsed);
        REQUIRE(parsed.value().getClipSide() == ClipSide::KeepDistal);
    }
}

TEST_CASE("V2 Binary Element Transform: LineClip - Registry Integration",
          "[transforms][v2][binary_element][line_clip][registry]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        auto names = registry.getAllTransformNames();
        bool found = false;
        for (auto const& name : names) {
            if (name == "ClipLineAtReference") {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
    
    SECTION("Metadata is available") {
        auto const* meta = registry.getMetadata("ClipLineAtReference");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->name == "ClipLineAtReference");
        REQUIRE(meta->category == "Geometry");
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
    }
}

TEST_CASE("V2 Binary Element Transform: LineClip - Context-aware version",
          "[transforms][v2][binary_element][line_clip]") {
    
    LineClipParams params;
    params.clip_side = "KeepBase";
    ComputeContext ctx;
    
    SECTION("Context-aware version produces same result") {
        auto line_data = line_clip_scenarios::horizontal_line();
        auto reference_data = line_clip_scenarios::vertical_reference_at_2_5();
        
        TimeFrameIndex line_time(100);
        TimeFrameIndex ref_time(0);
        
        auto line = getLineAt(line_data.get(), line_time);
        auto reference_line = getLineAt(reference_data.get(), ref_time);
        
        auto result_basic = clipLineAtReference(line, reference_line, params);
        auto result_ctx = clipLineAtReferenceWithContext(line, reference_line, params, ctx);
        
        REQUIRE(result_basic.size() == result_ctx.size());
        for (size_t i = 0; i < result_basic.size(); ++i) {
            REQUIRE(result_basic[i].x == Catch::Approx(result_ctx[i].x).margin(0.001f));
            REQUIRE(result_basic[i].y == Catch::Approx(result_ctx[i].y).margin(0.001f));
        }
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineClip via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_clip]") {
    
    DataManager dm;
    
    // Create TimeFrame for DataManager
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // For V2 binary transforms, both inputs must have data at matching time indices.
    // Create test data where line-to-clip and reference-line share the same time points.
    
    // Create line data at t=100: horizontal line from (0,2) to (4,2)
    auto line_data = std::make_shared<LineData>();
    line_data->setTimeFrame(time_frame);
    Line2D horizontal_line;
    horizontal_line.push_back({0.0f, 2.0f});
    horizontal_line.push_back({1.0f, 2.0f});
    horizontal_line.push_back({2.0f, 2.0f});
    horizontal_line.push_back({3.0f, 2.0f});
    horizontal_line.push_back({4.0f, 2.0f});
    line_data->addAtTime(TimeFrameIndex(100), horizontal_line, NotifyObservers::No);
    dm.setData("line_to_clip", line_data, TimeKey("default"));
    
    // Create reference line at t=100: vertical line at x=2.5
    auto reference_data = std::make_shared<LineData>();
    reference_data->setTimeFrame(time_frame);
    Line2D vertical_ref;
    vertical_ref.push_back({2.5f, 0.0f});
    vertical_ref.push_back({2.5f, 5.0f});
    reference_data->addAtTime(TimeFrameIndex(100), vertical_ref, NotifyObservers::No);
    dm.setData("reference_line", reference_data, TimeKey("default"));
    
    // Create diagonal line at t=200: from (0,0) to (4,4)
    auto diagonal_data = std::make_shared<LineData>();
    diagonal_data->setTimeFrame(time_frame);
    Line2D diagonal_line;
    diagonal_line.push_back({0.0f, 0.0f});
    diagonal_line.push_back({1.0f, 1.0f});
    diagonal_line.push_back({2.0f, 2.0f});
    diagonal_line.push_back({3.0f, 3.0f});
    diagonal_line.push_back({4.0f, 4.0f});
    diagonal_data->addAtTime(TimeFrameIndex(200), diagonal_line, NotifyObservers::No);
    dm.setData("diagonal_line", diagonal_data, TimeKey("default"));
    
    // Create reference line at t=200: vertical line at x=2.0
    auto reference_2_data = std::make_shared<LineData>();
    reference_2_data->setTimeFrame(time_frame);
    Line2D vertical_ref_2;
    vertical_ref_2.push_back({2.0f, 0.0f});
    vertical_ref_2.push_back({2.0f, 5.0f});
    reference_2_data->addAtTime(TimeFrameIndex(200), vertical_ref_2, NotifyObservers::No);
    dm.setData("reference_line_2", reference_2_data, TimeKey("default"));
    
    // Create line data for no-intersection test at t=300: horizontal line (0,2) to (3,2)
    auto no_intersect_line = std::make_shared<LineData>();
    no_intersect_line->setTimeFrame(time_frame);
    Line2D short_horizontal;
    short_horizontal.push_back({0.0f, 2.0f});
    short_horizontal.push_back({1.0f, 2.0f});
    short_horizontal.push_back({2.0f, 2.0f});
    short_horizontal.push_back({3.0f, 2.0f});
    no_intersect_line->addAtTime(TimeFrameIndex(300), short_horizontal, NotifyObservers::No);
    dm.setData("no_intersect_line", no_intersect_line, TimeKey("default"));
    
    // Create reference line at t=300: vertical line at x=5.0 (no intersection)
    auto no_intersect_ref = std::make_shared<LineData>();
    no_intersect_ref->setTimeFrame(time_frame);
    Line2D far_vertical;
    far_vertical.push_back({5.0f, 0.0f});
    far_vertical.push_back({5.0f, 5.0f});
    no_intersect_ref->addAtTime(TimeFrameIndex(300), far_vertical, NotifyObservers::No);
    dm.setData("no_intersect_ref", no_intersect_ref, TimeKey("default"));
    
    // Create temporary directory for JSON config files
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_clip_v2_test";
    std::filesystem::create_directories(test_dir);
    
    SECTION("KeepBase clipping via JSON pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Clip KeepBase Pipeline",
                    "description": "Test line clipping with KeepBase",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ClipLineAtReference",
                        "input_key": "line_to_clip",
                        "additional_input_keys": ["reference_line"],
                        "output_key": "v2_clipped_lines_keep_base",
                        "parameters": {
                            "clip_side": "KeepBase"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "keep_base_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        // Execute the V2 transformation pipeline
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        // Verify the transformation was executed and results are available
        auto result_lines = dm.getData<LineData>("v2_clipped_lines_keep_base");
        REQUIRE(result_lines != nullptr);
        
        // Check we have results at t=100
        auto clipped = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped.size() == 1);
        
        // Verify clipping - line should end at x=2.5
        REQUIRE(clipped[0].back().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped[0].back().y == Catch::Approx(2.0f).margin(0.001f));
        
        // Cleanup
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("KeepDistal clipping via JSON pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Line Clip KeepDistal Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ClipLineAtReference",
                        "input_key": "line_to_clip",
                        "additional_input_keys": ["reference_line"],
                        "output_key": "v2_clipped_lines_keep_distal",
                        "parameters": {
                            "clip_side": "KeepDistal"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "keep_distal_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_clipped_lines_keep_distal");
        REQUIRE(result_lines != nullptr);
        
        auto clipped = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(clipped.size() == 1);
        
        // Verify clipping - line should start at x=2.5 and end at x=4.0
        REQUIRE(clipped[0].front().x == Catch::Approx(2.5f).margin(0.001f));
        REQUIRE(clipped[0].front().y == Catch::Approx(2.0f).margin(0.001f));
        REQUIRE(clipped[0].back().x == Catch::Approx(4.0f).margin(0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("Diagonal line clipping via JSON pipeline") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "Diagonal Line Clip Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ClipLineAtReference",
                        "input_key": "diagonal_line",
                        "additional_input_keys": ["reference_line_2"],
                        "output_key": "v2_diagonal_clipped",
                        "parameters": {
                            "clip_side": "KeepBase"
                        }
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "diagonal_clip_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_diagonal_clipped");
        REQUIRE(result_lines != nullptr);
        
        // Diagonal line is at t=200
        auto clipped = result_lines->getAtTime(TimeFrameIndex(200));
        REQUIRE(clipped.size() == 1);
        
        // Should be clipped at x=2.0, y=2.0
        REQUIRE(clipped[0].back().x == Catch::Approx(2.0f).margin(0.001f));
        REQUIRE(clipped[0].back().y == Catch::Approx(2.0f).margin(0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    SECTION("No intersection via JSON pipeline - line unchanged") {
        const char* json_config = R"([
        {
            "transformations": {
                "metadata": {
                    "name": "No Intersection Pipeline",
                    "version": "2.0"
                },
                "steps": [
                    {
                        "step_id": "1",
                        "transform_name": "ClipLineAtReference",
                        "input_key": "no_intersect_line",
                        "additional_input_keys": ["no_intersect_ref"],
                        "output_key": "v2_no_intersection_result",
                        "parameters": {}
                    }
                ]
            }
        }
        ])";
        
        std::filesystem::path json_filepath = test_dir / "no_intersection_pipeline.json";
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
            json_file.close();
        }
        
        auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
        
        auto result_lines = dm.getData<LineData>("v2_no_intersection_result");
        REQUIRE(result_lines != nullptr);
        
        // No intersection line is at t=300
        auto clipped = result_lines->getAtTime(TimeFrameIndex(300));
        REQUIRE(clipped.size() == 1);
        
        // Line should be unchanged - 4 points from (0,2) to (3,2)
        REQUIRE(clipped[0].size() == 4);
        REQUIRE(clipped[0].front().x == Catch::Approx(0.0f).margin(0.001f));
        REQUIRE(clipped[0].back().x == Catch::Approx(3.0f).margin(0.001f));
        
        try {
            std::filesystem::remove_all(test_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
}
