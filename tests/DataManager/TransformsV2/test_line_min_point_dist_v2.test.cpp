// Unit tests for LineMinPointDistTransform

#include "transforms/v2/algorithms/LineMinPointDist/LineMinPointDist.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/FlatZipView.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Helper Functions Tests
// ============================================================================

TEST_CASE("pointToLineSegmentDistance2 calculates correct distance", "[line_min_point_dist][helpers]") {
    Point2D<float> point{5.0f, 5.0f};
    Point2D<float> line_start{0.0f, 0.0f};
    Point2D<float> line_end{10.0f, 0.0f};
    
    // Point is above the horizontal line segment
    float distance_squared = pointToLineSegmentDistance2(point, line_start, line_end);
    
    // Distance should be 5.0 (vertical distance to horizontal line)
    REQUIRE_THAT(std::sqrt(distance_squared), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("pointToLineSegmentDistance2 handles point on line", "[line_min_point_dist][helpers]") {
    Point2D<float> point{5.0f, 0.0f};
    Point2D<float> line_start{0.0f, 0.0f};
    Point2D<float> line_end{10.0f, 0.0f};
    
    float distance_squared = pointToLineSegmentDistance2(point, line_start, line_end);
    
    // Point is on the line
    REQUIRE_THAT(distance_squared, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("pointToLineSegmentDistance2 handles point beyond segment end", "[line_min_point_dist][helpers]") {
    Point2D<float> point{15.0f, 0.0f};
    Point2D<float> line_start{0.0f, 0.0f};
    Point2D<float> line_end{10.0f, 0.0f};
    
    float distance_squared = pointToLineSegmentDistance2(point, line_start, line_end);
    
    // Distance should be to end point: 5.0
    REQUIRE_THAT(std::sqrt(distance_squared), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("pointToLineMinDistance2 finds minimum over multiple segments", "[line_min_point_dist][helpers]") {
    Point2D<float> point{5.0f, 2.0f};
    
    // Create an L-shaped line
    Line2D line = {
        {0.0f, 0.0f},
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    };
    
    float distance_squared = pointToLineMinDistance2(point, line);
    
    // Minimum distance should be to the horizontal segment: 2.0
    REQUIRE_THAT(std::sqrt(distance_squared), Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("pointToLineMinDistance2 handles invalid line", "[line_min_point_dist][helpers]") {
    Point2D<float> point{5.0f, 5.0f};
    Line2D empty_line;
    
    float distance_squared = pointToLineMinDistance2(point, empty_line);
    
    // Should return max float
    REQUIRE(distance_squared == std::numeric_limits<float>::max());
}

// ============================================================================
// Transform Tests
// ============================================================================

TEST_CASE("calculateLineMinPointDistance computes correct distance", "[line_min_point_dist][transform]") {
    Line2D line = {
        {0.0f, 0.0f},
        {10.0f, 0.0f}
    };
    
    Point2D<float> point{2.0f, 1.0f};
    
    LineMinPointDistParams params;
    
    float distance = calculateLineMinPointDistance(line, point, params);
    
    // Point {2.0, 1.0} is distance 1.0 from horizontal line
    REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("calculateLineMinPointDistance returns squared distance when configured", "[line_min_point_dist][transform]") {
    Line2D line = {
        {0.0f, 0.0f},
        {10.0f, 0.0f}
    };
    
    Point2D<float> point{5.0f, 3.0f};
    
    LineMinPointDistParams params;
    params.return_squared_distance = true;
    
    float distance_squared = calculateLineMinPointDistance(line, point, params);
    
    // Distance squared should be 3.0^2 = 9.0
    REQUIRE_THAT(distance_squared, Catch::Matchers::WithinAbs(9.0f, 0.001f));
}

TEST_CASE("calculateLineMinPointDistance handles invalid line", "[line_min_point_dist][transform]") {
    Line2D line;  // Empty line
    
    Point2D<float> point{5.0f, 5.0f};
    
    LineMinPointDistParams params;
    
    float distance = calculateLineMinPointDistance(line, point, params);
    
    // Should return infinity
    REQUIRE(std::isinf(distance));
}

// ============================================================================
// Parameter Tests
// ============================================================================

TEST_CASE("LineMinPointDistParams has correct defaults", "[line_min_point_dist][params]") {
    LineMinPointDistParams params;
    
    REQUIRE(params.getUseFirstLineOnly() == true);
    REQUIRE(params.getReturnSquaredDistance() == false);
}

TEST_CASE("LineMinPointDistParams can override defaults", "[line_min_point_dist][params]") {
    LineMinPointDistParams params;
    params.use_first_line_only = false;
    params.return_squared_distance = true;
    
    REQUIRE(params.getUseFirstLineOnly() == false);
    REQUIRE(params.getReturnSquaredDistance() == true);
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_CASE("LineMinPointDistParams can be serialized to JSON", "[line_min_point_dist][json]") {
    LineMinPointDistParams params;
    params.use_first_line_only = false;
    params.return_squared_distance = true;
    
    auto json = rfl::json::write(params);
    
    REQUIRE(!json.empty());
    REQUIRE(json.find("use_first_line_only") != std::string::npos);
    REQUIRE(json.find("return_squared_distance") != std::string::npos);
}

TEST_CASE("LineMinPointDistParams can be deserialized from JSON", "[line_min_point_dist][json]") {
    std::string json = R"({
        "use_first_line_only": false,
        "return_squared_distance": true
    })";
    
    auto result = rfl::json::read<LineMinPointDistParams>(json);
    
    REQUIRE(result);
    REQUIRE(result.value().getUseFirstLineOnly() == false);
    REQUIRE(result.value().getReturnSquaredDistance() == true);
}

TEST_CASE("LineMinPointDistParams handles missing fields with defaults", "[line_min_point_dist][json]") {
    std::string json = "{}";
    
    auto result = rfl::json::read<LineMinPointDistParams>(json);
    
    REQUIRE(result);
    REQUIRE(result.value().getUseFirstLineOnly() == true);
    REQUIRE(result.value().getReturnSquaredDistance() == false);
}

// ============================================================================
// Pipeline Integration Tests
// ============================================================================

namespace {

// Helper to create PointData
std::shared_ptr<PointData> createPointData(std::vector<std::pair<int, std::vector<Point2D<float>>>> const& data) {
    auto pd = std::make_shared<PointData>();
    for (auto const& [time, points] : data) {
        for (auto const& p : points) {
            Point2D<float> p_copy = p;
            pd->addAtTime(TimeFrameIndex(time), std::move(p_copy), NotifyObservers::No);
        }
    }
    return pd;
}

// Helper to create LineData
std::shared_ptr<LineData> createLineData(std::vector<std::pair<int, std::vector<Line2D>>> const& data) {
    auto ld = std::make_shared<LineData>();
    for (auto const& [time, lines] : data) {
        for (auto const& l : lines) {
            Line2D l_copy = l;
            ld->addAtTime(TimeFrameIndex(time), std::move(l_copy), NotifyObservers::No);
        }
    }
    return ld;
}

} // namespace

TEST_CASE("Pipeline Execution with FlatZipView and LineMinPointDist", "[line_min_point_dist][pipeline]") {
    // 1. Create Input Data
    // T=0: Line(y=0) + Point(y=1) -> Dist=1
    // T=1: Line(y=0) + Point(y=2) -> Dist=2
    auto lines = createLineData({
        {0, {Line2D(Point2D<float>(0.0f, 0.0f), Point2D<float>(10.0f, 0.0f))}},
        {1, {Line2D(Point2D<float>(0.0f, 0.0f), Point2D<float>(10.0f, 0.0f))}}
    });
    
    auto points = createPointData({
        {0, {{5.0f, 1.0f}}},
        {1, {{5.0f, 2.0f}}}
    });

    // 2. Create Zipped View using FlatZipView with elements()
    FlatZipView zip_view(lines->elements(), points->elements());

    // 3. Adapt View to pair<Time, tuple> format expected by pipeline
    auto pipeline_input_view = zip_view | std::views::transform([](auto const& triplet) {
        auto const& [time, line, point] = triplet;
        // Data is already raw (Line2D, Point2D<float>), extracted from DataEntry by FlatZipView
        return std::make_pair(time, std::make_tuple(line, point));
    });

    // 4. Create Pipeline
    TransformPipeline pipeline;
    pipeline.addStep<LineMinPointDistParams>("CalculateLineMinPointDistance", LineMinPointDistParams{});

    // 5. Execute Pipeline
    // We must specify the InputElement type explicitly as tuple<Line2D, Point2D<float>>
    using InputTuple = std::tuple<Line2D, Point2D<float>>;
    auto result_view = pipeline.executeFromView<InputTuple>(pipeline_input_view);

    // 6. Verify Results
    int count = 0;
    for (auto const& [time, result_variant] : result_view) {
        // Result should be float
        REQUIRE(std::holds_alternative<float>(result_variant));
        float dist = std::get<float>(result_variant);
        
        if (time.getValue() == 0) {
            REQUIRE_THAT(dist, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        } else if (time.getValue() == 1) {
            REQUIRE_THAT(dist, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        }
        count++;
    }
    REQUIRE(count == 2);
}
