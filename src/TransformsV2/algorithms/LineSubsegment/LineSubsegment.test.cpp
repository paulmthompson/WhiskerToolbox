#include "LineSubsegment.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "fixtures/builders/LineDataBuilder.hpp"
#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/line/subsegment_scenarios.hpp"

#include <cmath>
#include <memory>
#include <vector>

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
// Tests: Happy Path - Direct Method
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Direct Method",
          "[transforms][v2][element][line_subsegment]") {
    
    LineSubsegmentParams params;
    params.method = LineSubsegmentMethod::Direct;
    
    SECTION("Middle subsegment with preserve_original_spacing") {
        auto line_data = subsegment_scenarios::diagonal_5_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.preserve_original_spacing = true;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.size() >= 2);
        
        // For a line from (0,0) to (4,4), 20% to 80% covers distance 0.8 to 3.2
        // This includes original points at positions 1, 2, 3
        
        // First point should be the first original point within range (position 1.0)
        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(1.0f, 0.001f));
        
        // Last point should be the last original point within range (position 3.0)
        REQUIRE_THAT(subsegment.back().x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(3.0f, 0.001f));
    }

    SECTION("Full line extraction (0.0 to 1.0)") {
        auto line_data = subsegment_scenarios::diagonal_4_points_at_200();
        TimeFrameIndex timestamp(200);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.0f;
        params.end_position = 1.0f;
        params.preserve_original_spacing = true;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.size() == 4);  // All original points
        
        // Should match original line exactly
        REQUIRE_THAT(subsegment[0].x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[3].x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment[3].y, WithinAbs(3.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Happy Path - Parametric Method
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Parametric Method",
          "[transforms][v2][element][line_subsegment]") {
    
    LineSubsegmentParams params;
    params.method = LineSubsegmentMethod::Parametric;
    
    SECTION("Middle subsegment with polynomial interpolation") {
        auto line_data = subsegment_scenarios::diagonal_5_points_at_300();
        TimeFrameIndex timestamp(300);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.3f;
        params.end_position = 0.7f;
        params.polynomial_order = 3;
        params.output_points = 20;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.size() == 20);  // Exactly output_points
        
        // Check that the subsegment is within the specified range
        float total_length = 4.0f;
        float start_distance = 0.3f * total_length; // 1.2
        float end_distance = 0.7f * total_length;   // 2.8
        
        // First point should be at start_distance
        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.2f, 0.1f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(1.2f, 0.1f));
        
        // Last point should be at end_distance
        REQUIRE_THAT(subsegment.back().x, WithinAbs(2.8f, 0.1f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(2.8f, 0.1f));
    }

    SECTION("Small subsegment extraction") {
        auto line_data = subsegment_scenarios::diagonal_5_points_at_400();
        TimeFrameIndex timestamp(400);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.45f;
        params.end_position = 0.55f;
        params.polynomial_order = 2;
        params.output_points = 10;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.size() == 10);
        
        float total_length = 4.0f;
        float start_distance = 0.45f * total_length; // 1.8
        float end_distance = 0.55f * total_length;   // 2.2
        
        // First point should be at start_distance
        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.8f, 0.1f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(1.8f, 0.1f));
        
        // Last point should be at end_distance
        REQUIRE_THAT(subsegment.back().x, WithinAbs(2.2f, 0.1f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(2.2f, 0.1f));
    }
}

// ============================================================================
// Tests: Edge Cases
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Edge Cases",
          "[transforms][v2][element][line_subsegment]") {
    
    LineSubsegmentParams params;
    params.method = LineSubsegmentMethod::Direct;
    
    SECTION("Empty line") {
        Line2D empty_line;
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;

        Line2D subsegment = extractLineSubsegment(empty_line, params);
        
        REQUIRE(subsegment.empty());
    }

    SECTION("Single point line") {
        auto line_data = subsegment_scenarios::single_point();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        // Single point lines should be returned unchanged
        REQUIRE(subsegment.size() == 1);
        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Invalid position range (start >= end)") {
        auto line_data = subsegment_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.8f;
        params.end_position = 0.2f;  // Invalid: start > end

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.empty());  // No valid subsegment
    }

    SECTION("Position values clamped to valid range") {
        auto line_data = subsegment_scenarios::diagonal_4_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = -0.5f;  // Invalid: negative
        params.end_position = 1.5f;     // Invalid: > 1.0

        Line2D subsegment = extractLineSubsegment(line, params);
        
        REQUIRE(subsegment.size() >= 2);
        
        // Should be clamped to [0.0, 1.0] range
        REQUIRE_THAT(subsegment[0].x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(3.0f, 0.001f));
    }

    SECTION("Two-point line with Parametric method (insufficient points)") {
        auto line_data = subsegment_scenarios::two_point_line();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.method = LineSubsegmentMethod::Parametric;
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.polynomial_order = 3;  // Requires at least 4 points

        Line2D subsegment = extractLineSubsegment(line, params);
        
        // Should fall back to direct method or produce valid output
        REQUIRE(subsegment.size() >= 1);
    }

    SECTION("Zero-length line (all points at same location)") {
        auto line_data = subsegment_scenarios::zero_length_line();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        params.start_position = 0.2f;
        params.end_position = 0.8f;

        Line2D subsegment = extractLineSubsegment(line, params);
        
        // Zero-length lines should return a single point
        REQUIRE(subsegment.size() == 1);
        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(2.0f, 0.001f));
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Parameter Validation",
          "[transforms][v2][element][line_subsegment]") {
    
    SECTION("validate() clamps positions to [0, 1]") {
        LineSubsegmentParams params;
        params.start_position = -0.5f;
        params.end_position = 1.5f;
        
        params.validate();
        
        REQUIRE_THAT(params.start_position, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(params.end_position, WithinAbs(1.0f, 0.001f));
    }

    SECTION("validate() clamps polynomial order") {
        LineSubsegmentParams params;
        params.polynomial_order = 15;  // Too high
        
        params.validate();
        
        REQUIRE(params.polynomial_order <= 9);
        REQUIRE(params.polynomial_order >= 1);
    }

    SECTION("validate() clamps output points") {
        LineSubsegmentParams params;
        params.output_points = 0;  // Too low
        
        params.validate();
        
        REQUIRE(params.output_points >= 2);
    }
}

// ============================================================================
// Tests: Context-Aware Execution
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Context-Aware Execution",
          "[transforms][v2][element][line_subsegment]") {
    
    SECTION("Normal execution with context") {
        auto line_data = subsegment_scenarios::diagonal_5_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineSubsegmentParams params;
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = LineSubsegmentMethod::Direct;
        
        int progress = 0;
        ComputeContext ctx;
        ctx.progress = [&progress](int p) { progress = p; };

        Line2D subsegment = extractLineSubsegmentWithContext(line, params, ctx);
        
        REQUIRE(subsegment.size() >= 2);
        REQUIRE(progress == 100);
    }

    SECTION("Execution with cancellation") {
        auto line_data = subsegment_scenarios::diagonal_5_points();
        TimeFrameIndex timestamp(100);
        auto line = getLineAt(line_data.get(), timestamp);
        
        LineSubsegmentParams params;
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = LineSubsegmentMethod::Direct;
        
        ComputeContext ctx;
        ctx.is_cancelled = []() { return true; };  // Always cancelled

        Line2D subsegment = extractLineSubsegmentWithContext(line, params, ctx);
        
        // When cancelled, the original line is returned unchanged
        REQUIRE(subsegment.size() == line.size());
    }
}

// ============================================================================
// Tests: JSON Parameter Validation
// ============================================================================

TEST_CASE("V2 LineSubsegmentParams - JSON Rejection",
          "[transforms][v2][params][json][line_subsegment]") {

    SECTION("Reject malformed JSON") {
        std::string const json = R"({
            "start_position": 0.2,
            "invalid
        })";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-numeric start_position") {
        std::string const json = R"({"start_position": "start"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-numeric end_position") {
        std::string const json = R"({"end_position": "end"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject unknown method") {
        std::string const json = R"({"method": "invalid"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject wrong casing for method") {
        std::string const json = R"({"method": "direct"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-string method") {
        std::string const json = R"({"method": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-integer polynomial_order") {
        std::string const json = R"({"polynomial_order": "three"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-integer output_points") {
        std::string const json = R"({"output_points": "many"})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }

    SECTION("Reject non-boolean preserve_original_spacing") {
        std::string const json = R"({"preserve_original_spacing": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LineSubsegmentParams>(json));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineSubsegment - Registry Integration",
          "[transforms][v2][registry][line_subsegment]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("ExtractLineSubsegment"));
    }
    
    SECTION("Can retrieve metadata") {
        auto const* metadata = registry.getMetadata("ExtractLineSubsegment");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "ExtractLineSubsegment");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
    
    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasElementTransform("ExtractLineSubsegmentWithContext"));
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineSubsegment via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_subsegment]") {
    
    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data using builder
    auto test_line = LineDataBuilder()
        .withCoords(100, 
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
        .build();
    test_line->setTimeFrame(time_frame);
    dm.setData("test_line", test_line, TimeKey("default"));
    
    SECTION("Direct method pipeline") {
        LineSubsegmentParams params;
        params.start_position = 0.2f;
        params.end_position = 0.8f;
        params.method = LineSubsegmentMethod::Direct;
        params.preserve_original_spacing = true;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLineSubsegment",
                "test_line",
                "v2_extracted_subsegments",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_subsegments = dm.getData<LineData>("v2_extracted_subsegments");
        REQUIRE(result_subsegments != nullptr);

        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);

        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() >= 2);

        REQUIRE_THAT(subsegment[0].x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().x, WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(3.0f, 0.001f));
    }
    
    SECTION("Parametric method pipeline") {
        LineSubsegmentParams params;
        params.start_position = 0.3f;
        params.end_position = 0.7f;
        params.method = LineSubsegmentMethod::Parametric;
        params.polynomial_order = 3;
        params.output_points = 20;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLineSubsegment",
                "test_line",
                "v2_parametric_subsegments",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_subsegments = dm.getData<LineData>("v2_parametric_subsegments");
        REQUIRE(result_subsegments != nullptr);

        auto const & subsegments = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments.size() == 1);

        auto const & subsegment = subsegments[0];
        REQUIRE(subsegment.size() == 20);

        float const total_length = 4.0f;
        float const start_distance = 0.3f * total_length;
        float const end_distance = 0.7f * total_length;

        REQUIRE_THAT(subsegment[0].x, WithinAbs(start_distance, 0.1f));
        REQUIRE_THAT(subsegment[0].y, WithinAbs(start_distance, 0.1f));
        REQUIRE_THAT(subsegment.back().x, WithinAbs(end_distance, 0.1f));
        REQUIRE_THAT(subsegment.back().y, WithinAbs(end_distance, 0.1f));
    }
    
    SECTION("Multiple timesteps pipeline") {
        auto multi_line = LineDataBuilder()
            .withCoords(100, 
                {0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
                {0.0f, 1.0f, 2.0f, 3.0f, 4.0f})
            .withCoords(200, 
                {0.0f, 2.0f, 4.0f, 6.0f, 8.0f},
                {0.0f, 0.0f, 0.0f, 0.0f, 0.0f})
            .build();
        multi_line->setTimeFrame(time_frame);
        dm.setData("multi_line", multi_line, TimeKey("default"));

        LineSubsegmentParams params;
        params.start_position = 0.25f;
        params.end_position = 0.75f;
        params.method = LineSubsegmentMethod::Direct;
        params.preserve_original_spacing = true;

        auto const pipeline = makeSingleStepPipeline(
                "ExtractLineSubsegment",
                "multi_line",
                "v2_multi_subsegments",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_subsegments = dm.getData<LineData>("v2_multi_subsegments");
        REQUIRE(result_subsegments != nullptr);

        auto times = result_subsegments->getTimesWithData();
        REQUIRE(times.size() == 2);

        auto const & subsegments_t100 = result_subsegments->getAtTime(TimeFrameIndex(100));
        REQUIRE(subsegments_t100.size() == 1);
        REQUIRE(subsegments_t100[0].size() >= 2);

        auto const & subsegments_t200 = result_subsegments->getAtTime(TimeFrameIndex(200));
        REQUIRE(subsegments_t200.size() == 1);
        REQUIRE(subsegments_t200[0].size() >= 2);

        REQUIRE_THAT(subsegments_t200[0][0].y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(subsegments_t200[0].back().y, WithinAbs(0.0f, 0.001f));
    }
}
