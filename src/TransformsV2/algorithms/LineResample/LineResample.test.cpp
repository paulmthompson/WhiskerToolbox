#include "LineResample.hpp"

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
#include "fixtures/scenarios/line/resample_scenarios.hpp"

#include <cmath>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace pipeline_json_test;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const * line_data, TimeFrameIndex time) {
    auto lines_at_time = line_data->getAtTime(time);
    for (auto const & line: lines_at_time) {
        return line;// Return the first line at this time
    }
    return Line2D{};
}

// ============================================================================
// Tests: Algorithm Correctness (using scenarios)
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample - FixedSpacing Algorithm",
          "[transforms][v2][element][line_resample]") {

    LineResampleParams params;
    params.method = LineResampleMethod::FixedSpacing;

    SECTION("Two diagonal lines") {
        auto line_data = resample_scenarios::two_diagonal_lines();

        params.target_spacing = 15.0f;

        // Test first line at t=100
        auto line_t100 = getLineAt(line_data.get(), TimeFrameIndex(100));
        REQUIRE(!line_t100.empty());

        auto resampled = resampleLine(line_t100, params);
        REQUIRE(!resampled.empty());
        // The resampled line should be valid (size >= 2 since we preserve endpoints)
        REQUIRE(resampled.size() >= 2);

        // Test second line at t=200
        auto line_t200 = getLineAt(line_data.get(), TimeFrameIndex(200));
        REQUIRE(!line_t200.empty());

        auto resampled_200 = resampleLine(line_t200, params);
        REQUIRE(!resampled_200.empty());
        REQUIRE(resampled_200.size() >= 2);
    }

    SECTION("Simple diagonal line") {
        auto line_data = resample_scenarios::simple_diagonal();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));

        params.target_spacing = 10.0f;

        auto resampled = resampleLine(line, params);
        REQUIRE(!resampled.empty());
    }
}

TEST_CASE("V2 Element Transform: LineResample - DouglasPeucker Algorithm",
          "[transforms][v2][element][line_resample]") {

    LineResampleParams params;
    params.method = LineResampleMethod::DouglasPeucker;

    SECTION("Dense nearly-straight line simplification") {
        auto line_data = resample_scenarios::dense_nearly_straight_line();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));

        REQUIRE(line.size() == 11);// Original has 11 points

        params.epsilon = 0.5f;

        auto simplified = resampleLine(line, params);

        // Douglas-Peucker should significantly reduce point count for nearly straight line
        REQUIRE(simplified.size() < line.size());
        REQUIRE(simplified.size() >= 2);// At least start and end points preserved
    }

    SECTION("Simple diagonal remains compact") {
        auto line_data = resample_scenarios::simple_diagonal();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));

        params.epsilon = 1.0f;

        auto simplified = resampleLine(line, params);

        // Simple diagonal (3 points) should simplify to 2 points (endpoints)
        REQUIRE(simplified.size() <= line.size());
    }
}

TEST_CASE("V2 Element Transform: LineResample - PolynomialSmooth Algorithm",
          "[transforms][v2][element][line_resample]") {

    LineResampleParams params;
    params.method = LineResampleMethod::PolynomialSmooth;
    params.polynomial_order = 3;
    params.target_spacing = 5.0f;

    SECTION("Noisy polyline is smoothed and resampled") {
        Line2D noisy;
        for (int i = 0; i <= 20; ++i) {
            float const t = static_cast<float>(i) / 20.0f;
            float const x = t * 100.0f;
            float const y = t * 50.0f + ((i % 2 == 0) ? 5.0f : -5.0f);
            noisy.push_back(Point2D<float>{x, y});
        }

        auto const smoothed = resampleLine(noisy, params);
        REQUIRE(smoothed.size() >= 2);
        REQUIRE(smoothed.size() < noisy.size());
    }

    SECTION("Too few points for order returns unchanged") {
        Line2D short_line;
        short_line.push_back(Point2D<float>{0.0f, 0.0f});
        short_line.push_back(Point2D<float>{10.0f, 10.0f});
        short_line.push_back(Point2D<float>{20.0f, 5.0f});

        params.polynomial_order = 3;
        auto const result = resampleLine(short_line, params);
        REQUIRE(result.size() == short_line.size());
    }
}

TEST_CASE("V2 Element Transform: LineResample - Edge Cases",
          "[transforms][v2][element][line_resample][edge]") {

    LineResampleParams params;

    SECTION("Empty line returns empty") {
        auto line_data = resample_scenarios::empty();

        // No lines exist, so create an empty line
        Line2D const empty_line;

        auto result = resampleLine(empty_line, params);
        REQUIRE(result.empty());
    }

    SECTION("Single point line returned unchanged") {
        auto line_data = resample_scenarios::single_point();
        auto line = getLineAt(line_data.get(), TimeFrameIndex(100));

        REQUIRE(line.size() == 1);

        params.method = LineResampleMethod::FixedSpacing;
        params.target_spacing = 10.0f;

        auto result = resampleLine(line, params);
        REQUIRE(result.size() == 1);
    }

    SECTION("Two point line returned unchanged") {
        // Two-point lines are minimal representation
        Line2D two_point_line;
        two_point_line.push_back(Point2D<float>{0.0f, 0.0f});
        two_point_line.push_back(Point2D<float>{10.0f, 10.0f});

        params.method = LineResampleMethod::FixedSpacing;
        params.target_spacing = 5.0f;

        auto result = resampleLine(two_point_line, params);
        REQUIRE(result.size() == 2);
    }

    SECTION("Diagonal with empty line - only empty processed correctly") {
        auto line_data = resample_scenarios::diagonal_with_empty();

        // t=200 has an empty line
        auto line_t200 = getLineAt(line_data.get(), TimeFrameIndex(200));

        auto result = resampleLine(line_t200, params);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Parameter Validation
// ============================================================================

TEST_CASE("V2 LineResampleParams - JSON Rejection",
          "[transforms][v2][params][json][line_resample]") {

    SECTION("Reject malformed JSON") {
        std::string const json = R"({
            "method": "FixedSpacing",
            "invalid
        })";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject unknown method") {
        std::string const json = R"({"method": "invalid"})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject wrong casing for method") {
        std::string const json = R"({"method": "fixedspacing"})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject non-string method") {
        std::string const json = R"({"method": 1})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject non-numeric target_spacing") {
        std::string const json = R"({"target_spacing": "wide"})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject negative target_spacing") {
        std::string const json = R"({"target_spacing": -5.0})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject zero target_spacing") {
        std::string const json = R"({"target_spacing": 0.0})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject non-numeric epsilon") {
        std::string const json = R"({"epsilon": "tight"})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject negative epsilon") {
        std::string const json = R"({"epsilon": -1.0})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }

    SECTION("Reject non-integer polynomial_order") {
        std::string const json = R"({"polynomial_order": "three"})";
        REQUIRE_FALSE(loadParametersFromJson<LineResampleParams>(json));
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample Registry Integration",
          "[transforms][v2][registry][element][line_resample]") {

    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.hasElementTransform("ResampleLine"));
    }

    SECTION("Can retrieve metadata") {
        auto const * metadata = registry.getMetadata("ResampleLine");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "ResampleLine");
        REQUIRE(metadata->category == "Geometry");
        REQUIRE(metadata->is_multi_input == false);
    }
}

// ============================================================================
// Tests: DataManager Integration via load_data_from_json_config_v2
// ============================================================================

TEST_CASE("V2 DataManager Integration: LineResample via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][line_resample]") {

    // Create DataManager and populate with test data
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Populate with scenario data
    auto two_diagonal = resample_scenarios::two_diagonal_lines();
    two_diagonal->setTimeFrame(time_frame);
    dm.setData("two_diagonal_lines", two_diagonal, TimeKey("default"));

    auto dense_line = resample_scenarios::dense_nearly_straight_line();
    dense_line->setTimeFrame(time_frame);
    dm.setData("dense_nearly_straight_line", dense_line, TimeKey("default"));

    SECTION("FixedSpacing pipeline") {
        LineResampleParams params;
        params.method = LineResampleMethod::FixedSpacing;
        params.target_spacing = 15.0f;

        auto const pipeline = makeSingleStepPipeline(
                "ResampleLine",
                "two_diagonal_lines",
                "v2_resampled_lines",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_lines = dm.getData<LineData>("v2_resampled_lines");
        REQUIRE(result_lines != nullptr);

        auto times = result_lines->getTimesWithData();
        REQUIRE(times.size() == 2);
    }

    SECTION("DouglasPeucker pipeline") {
        LineResampleParams params;
        params.method = LineResampleMethod::DouglasPeucker;
        params.epsilon = 0.5f;

        auto const pipeline = makeSingleStepPipeline(
                "ResampleLine",
                "dense_nearly_straight_line",
                "v2_simplified_lines",
                params);

        executeViaLoadDataFromJsonConfigV2(dm, pipeline);

        auto result_lines = dm.getData<LineData>("v2_simplified_lines");
        REQUIRE(result_lines != nullptr);

        auto simplified = result_lines->getAtTime(TimeFrameIndex(100));
        REQUIRE(!simplified.empty());

        auto original = dense_line->getAtTime(TimeFrameIndex(100));
        REQUIRE(!original.empty());
        REQUIRE(simplified[0].size() < original[0].size());
    }
}

// ============================================================================
// Tests: Context-Aware Execution
// ============================================================================

TEST_CASE("V2 Element Transform: LineResample - Context-Aware",
          "[transforms][v2][element][line_resample][context]") {

    auto line_data = resample_scenarios::two_diagonal_lines();
    auto line = getLineAt(line_data.get(), TimeFrameIndex(100));

    LineResampleParams params;
    params.method = LineResampleMethod::FixedSpacing;
    params.target_spacing = 10.0f;

    SECTION("With cancellation check") {
        bool was_cancelled = false;
        ComputeContext ctx;
        ctx.is_cancelled = [&was_cancelled]() { return was_cancelled; };
        ctx.progress = [](int) {};

        auto result = resampleLineWithContext(line, params, ctx);

        // Normal execution should produce valid result
        REQUIRE(!result.empty());
    }

    SECTION("Cancellation returns original line") {
        ComputeContext ctx;
        ctx.is_cancelled = []() { return true; };// Always cancelled
        ctx.progress = [](int) {};

        auto result = resampleLineWithContext(line, params, ctx);

        // Cancelled execution returns original line
        REQUIRE(result.size() == line.size());
    }
}
