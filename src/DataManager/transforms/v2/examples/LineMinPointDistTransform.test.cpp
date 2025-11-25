#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager/transforms/v2/examples/LineMinPointDistTransform.hpp"
#include "DataManager/transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/examples/RegisteredTransforms.hpp"
#include "fixtures/LinePointDistanceTestFixtures.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"

#include <memory>
#include <cmath>

using namespace WhiskerToolbox;
using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace WhiskerToolbox::Testing;
using Catch::Matchers::WithinAbs;

// Helper to extract Line2D from LineData at a given time
static Line2D getLineAt(LineData const* line_data, TimeFrameIndex time) {
    auto const& sequence = line_data->getLineTimeSeries();
    for (auto const& [t, line] : sequence) {
        if (t == time) {
            return line;
        }
    }
    return Line2D{};
}

// Helper to extract points from PointData at a given time
static std::vector<Point2D<float>> getPointsAt(PointData const* point_data, TimeFrameIndex time) {
    return point_data->getAtTime(time);
}

// ============================================================================
// Core Functionality Tests
// ============================================================================

TEST_CASE("Line to point minimum distance calculation V2 - Core functionality", "[line][point][distance][transform][v2]") {

    SECTION("Basic distance calculation between a line and a point") {
        HorizontalLineWithPointAbove fixture;
        LineMinPointDistParams params;
        
        auto line = getLineAt(fixture.line_data.get(), fixture.timestamp);
        auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        REQUIRE_THAT(distance, WithinAbs(fixture.expected_distance, 0.001f));
    }

    SECTION("Multiple points with different distances") {
        VerticalLineWithMultiplePoints fixture;
        LineMinPointDistParams params;
        
        auto line = getLineAt(fixture.line_data.get(), fixture.timestamp);
        auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp);
        
        // The fixture expects minimum distance, so we test the minimum
        float min_distance = std::numeric_limits<float>::max();
        for (auto const& point : points) {
            float dist = calculateLineMinPointDistance(line, point, params);
            min_distance = std::min(min_distance, dist);
        }
        
        REQUIRE_THAT(min_distance, WithinAbs(fixture.expected_distance, 0.001f));
    }

    SECTION("Multiple timesteps with lines and points") {
        MultipleTimesteps fixture;
        LineMinPointDistParams params;
        
        // Test first timestep
        {
            auto line = getLineAt(fixture.line_data.get(), fixture.timestamp1);
            auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp1);
            float distance = calculateLineMinPointDistance(line, points[0], params);
            REQUIRE_THAT(distance, WithinAbs(fixture.expected_distance1, 0.001f));
        }
        
        // Test second timestep
        {
            auto line = getLineAt(fixture.line_data.get(), fixture.timestamp2);
            auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp2);
            float distance = calculateLineMinPointDistance(line, points[0], params);
            REQUIRE_THAT(distance, WithinAbs(fixture.expected_distance2, 0.001f));
        }
    }

    SECTION("Scaling points with different image sizes") {
        CoordinateScaling fixture;
        LineMinPointDistParams params;
        
        auto line = getLineAt(fixture.line_data.get(), fixture.timestamp);
        auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        REQUIRE_THAT(distance, WithinAbs(fixture.expected_distance, 0.001f));
    }

    SECTION("Point directly on the line has zero distance") {
        PointOnLine fixture;
        LineMinPointDistParams params;
        
        auto line = getLineAt(fixture.line_data.get(), fixture.timestamp);
        auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        REQUIRE_THAT(distance, WithinAbs(fixture.expected_distance, 0.001f));
    }
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("Line to point minimum distance calculation V2 - Edge cases and error handling", "[line][point][distance][edge][v2]") {

    SECTION("Line with only one point (invalid)") {
        InvalidLineOnePoint fixture;
        LineMinPointDistParams params;
        
        auto line = getLineAt(fixture.line_data.get(), fixture.timestamp);
        auto points = getPointsAt(fixture.point_data.get(), fixture.timestamp);
        
        float distance = calculateLineMinPointDistance(line, points[0], params);
        
        // Should return infinity for invalid line
        REQUIRE(std::isinf(distance));
    }
}

// ============================================================================
// Parameter Validation Tests
// ============================================================================

TEST_CASE("Line to point minimum distance calculation V2 - Parameters", "[line][point][distance][parameters][v2]") {

    SECTION("JSON round-trip with reflect-cpp") {
        // Create params with non-default value
        LineMinPointDistParams original;
        original.return_squared_distance = true;
        
        // Serialize to JSON
        auto json_str = rfl::json::write(original);
        
        // Deserialize from JSON
        auto deserialized_result = rfl::json::read<LineMinPointDistParams>(json_str);
        REQUIRE(deserialized_result);
        
        auto deserialized = deserialized_result.value();
        
        // Verify values match
        REQUIRE(deserialized.getReturnSquaredDistance() == original.getReturnSquaredDistance());
    }
}
