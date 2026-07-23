#include "LinePointExtraction.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/line/point_extraction_scenarios.hpp"

#include <cmath>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace pipeline_json_test;
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

TEST_CASE("V2 Element Transform: LinePointExtraction - Core Functionality",
          "[transforms][v2][element][line_point_extraction]") {
    
    LinePointExtractionParams params;
    
    SECTION("Direct method - middle position") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Direct;
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 50% position of diagonal line, should be approximately (1.5, 1.5)
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.001f));
    }
    
    SECTION("Direct method - start position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_200();
        TimeFrameIndex timestamp(200);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.0f;
        params.method = LinePointExtractionMethod::Direct;
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 0% position, should be at the first point
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Direct method - end position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_300();
        TimeFrameIndex timestamp(300);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 1.0f;
        params.method = LinePointExtractionMethod::Direct;
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 100% position, should be at the last point
        REQUIRE_THAT(point.x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(3.0f, 0.001f));
    }
    
    SECTION("Parametric method - middle position") {
        auto line_data = point_extraction_scenarios::diagonal_at_time_400();
        TimeFrameIndex timestamp(400);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Parametric;
        params.polynomial_order = 3;
        
        auto point = extractLinePoint(line, params);
        
        // Parametric method should give a smooth interpolation
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.1f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.1f));
    }
    
    SECTION("Horizontal line - middle position") {
        auto line_data = point_extraction_scenarios::horizontal_3_points();
        TimeFrameIndex timestamp(600);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Direct;
        params.use_interpolation = true;
        
        auto point = extractLinePoint(line, params);
        
        // At 50% of horizontal line, should be at (1.0, 0.0)
        REQUIRE_THAT(point.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("V2 Element Transform: LinePointExtraction - Edge Cases",
          "[transforms][v2][element][line_point_extraction][edge]") {
    
    LinePointExtractionParams params;
    params.position = 0.5f;
    params.method = LinePointExtractionMethod::Direct;
    
    SECTION("Empty line returns (0, 0)") {
        Line2D empty_line;
        
        auto point = extractLinePoint(empty_line, params);
        
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Single point line returns that point") {
        auto line_data = point_extraction_scenarios::single_point();
        TimeFrameIndex timestamp(200);
        auto line = getLineAt(line_data.get(), timestamp);
        
        auto point = extractLinePoint(line, params);
        
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(2.5f, 0.001f));
    }
    
    SECTION("Two-point line works correctly") {
        auto line_data = point_extraction_scenarios::two_point_line();
        TimeFrameIndex timestamp(500);
        auto line = getLineAt(line_data.get(), timestamp);
        
        auto point = extractLinePoint(line, params);
        
        // At 50% of diagonal line from (0,0) to (1,1), should be (0.5, 0.5)
        REQUIRE_THAT(point.x, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Parametric fallback with too few points") {
        auto line_data = point_extraction_scenarios::two_point_line();
        TimeFrameIndex timestamp(500);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = LinePointExtractionMethod::Parametric;
        params.polynomial_order = 3;  // Requires more than 2 points
        
        auto point = extractLinePoint(line, params);
        
        // Should fall back to direct method or linear interpolation
        REQUIRE_THAT(point.x, WithinAbs(0.5f, 0.1f));
        REQUIRE_THAT(point.y, WithinAbs(0.5f, 0.1f));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 LinePointExtractionParams - JSON Rejection",
          "[transforms][v2][params][json][line_point_extraction]") {

    SECTION("Reject malformed JSON") {
        std::string const json = R"({
            "position": 0.5,
            "invalid
        })";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject non-numeric position") {
        std::string const json = R"({"position": "half"})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject unknown method") {
        std::string const json = R"({"method": "invalid"})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject wrong casing for method") {
        std::string const json = R"({"method": "direct"})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject non-string method") {
        std::string const json = R"({"method": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject non-integer polynomial_order") {
        std::string const json = R"({"polynomial_order": "three"})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }

    SECTION("Reject non-boolean use_interpolation") {
        std::string const json = R"({"use_interpolation": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LinePointExtractionParams>(json));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtraction Registry Integration",
          "[transforms][v2][registry][element][line_point_extraction]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("ExtractLinePoint"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("ExtractLinePoint");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "ExtractLinePoint");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LinePointExtraction via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_point_extraction]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Populate with scenario data
    auto diagonal_data = point_extraction_scenarios::diagonal_4_points();
    diagonal_data->setTimeFrame(time_frame);
    dm.setData("diagonal_line", diagonal_data, TimeKey("default"));
    
    auto multiple_timesteps_data = point_extraction_scenarios::multiple_timesteps();
    multiple_timesteps_data->setTimeFrame(time_frame);
    dm.setData("multiple_timesteps_line", multiple_timesteps_data, TimeKey("default"));
    
    SECTION("Single diagonal line - middle position") {
        LinePointExtractionParams params;
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Direct;
        params.use_interpolation = true;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLinePoint",
                "diagonal_line",
                "v2_extracted_points",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_points = dm.getData<PointData>("v2_extracted_points");
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().size() == 1);

        auto const& points_t100 = result_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_t100.size() == 1);
        REQUIRE_THAT(points_t100[0].x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(points_t100[0].y, WithinAbs(1.5f, 0.001f));
    }

    SECTION("Multiple timesteps pipeline") {
        LinePointExtractionParams params;
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Direct;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLinePoint",
                "multiple_timesteps_line",
                "v2_multi_points",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_points = dm.getData<PointData>("v2_multi_points");
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().size() == 2);

        auto const& points_t500 = result_points->getAtTime(TimeFrameIndex(500));
        REQUIRE(points_t500.size() == 1);
        REQUIRE_THAT(points_t500[0].x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points_t500[0].y, WithinAbs(2.0f, 0.001f));

        auto const& points_t600 = result_points->getAtTime(TimeFrameIndex(600));
        REQUIRE(points_t600.size() == 1);
        REQUIRE_THAT(points_t600[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(points_t600[0].y, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Parametric method pipeline") {
        LinePointExtractionParams params;
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Parametric;
        params.polynomial_order = 3;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLinePoint",
                "diagonal_line",
                "v2_parametric_points",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_points = dm.getData<PointData>("v2_parametric_points");
        REQUIRE(result_points != nullptr);
        REQUIRE(result_points->getTimesWithData().size() == 1);

        auto const& points_t100 = result_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_t100.size() == 1);
        REQUIRE_THAT(points_t100[0].x, WithinAbs(1.5f, 0.1f));
        REQUIRE_THAT(points_t100[0].y, WithinAbs(1.5f, 0.1f));
    }

    SECTION("Start and end position pipeline") {
        LinePointExtractionParams start_params;
        start_params.position = 0.0f;
        start_params.method = LinePointExtractionMethod::Direct;

        auto const start_pipeline = makeSingleStepPipeline(
                "ExtractLinePoint",
                "diagonal_line",
                "v2_start_point",
                start_params);

        executeViaLoadDataFromJsonConfigV2(dm, start_pipeline);

        auto start_points = dm.getData<PointData>("v2_start_point");
        REQUIRE(start_points != nullptr);

        auto const& points_start = start_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_start.size() == 1);
        REQUIRE_THAT(points_start[0].x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(points_start[0].y, WithinAbs(0.0f, 0.001f));

        LinePointExtractionParams end_params;
        end_params.position = 1.0f;
        end_params.method = LinePointExtractionMethod::Direct;

        auto const end_pipeline = makeSingleStepPipeline(
                "ExtractLinePoint",
                "diagonal_line",
                "v2_end_point",
                end_params);

        executeViaLoadDataFromJsonConfigV2(dm, end_pipeline);

        auto end_points = dm.getData<PointData>("v2_end_point");
        REQUIRE(end_points != nullptr);

        auto const& points_end = end_points->getAtTime(TimeFrameIndex(100));
        REQUIRE(points_end.size() == 1);
        REQUIRE_THAT(points_end[0].x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(points_end[0].y, WithinAbs(3.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Context-Aware Transform
// ============================================================================

TEST_CASE("V2 Element Transform: LinePointExtraction Context-Aware",
          "[transforms][v2][context][line_point_extraction]") {
    
    SECTION("Context-aware transform reports progress") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LinePointExtractionParams params;
        params.position = 0.5f;
        params.method = LinePointExtractionMethod::Direct;
        
        int last_progress = -1;
        ComputeContext ctx;
        ctx.progress = [&last_progress](int p) { last_progress = p; };
        ctx.is_cancelled = []() { return false; };
        
        auto point = extractLinePointWithContext(line, params, ctx);
        
        REQUIRE_THAT(point.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(1.5f, 0.001f));
        REQUIRE(last_progress > 0);  // Progress was reported
    }
    
    SECTION("Context-aware transform respects cancellation") {
        auto line_data = point_extraction_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LinePointExtractionParams params;
        params.position = 0.5f;
        
        ComputeContext ctx;
        ctx.progress = [](int) {};
        ctx.is_cancelled = []() { return true; };  // Always cancelled
        
        auto point = extractLinePointWithContext(line, params, ctx);
        
        // Should return default point when cancelled
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.001f));
    }
}
