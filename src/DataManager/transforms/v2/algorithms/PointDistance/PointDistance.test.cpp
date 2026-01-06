#include "PointDistance.hpp"

#include "Points/Point_Data.hpp"
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::Transforms::V2::PointDistance;

// ============================================================================
// Tests: PointDistanceParams JSON Loading
// ============================================================================

TEST_CASE("PointDistanceParams - Load valid JSON with GlobalAverage", "[transforms][v2][params][json][pointdistance]") {
    std::string json = R"({
        "reference_type": "GlobalAverage"
    })";

    auto result = loadParametersFromJson<PointDistanceParams>(json);

    REQUIRE(result);
    auto params = result.value();

    REQUIRE(params.reference_type == ReferenceType::GlobalAverage);
}

TEST_CASE("PointDistanceParams - Load valid JSON with RollingAverage", "[transforms][v2][params][json][pointdistance]") {
    std::string json = R"({
        "reference_type": "RollingAverage",
        "window_size": 500
    })";

    auto result = loadParametersFromJson<PointDistanceParams>(json);

    REQUIRE(result);
    auto params = result.value();

    REQUIRE(params.reference_type == ReferenceType::RollingAverage);
    REQUIRE(params.getWindowSize() == 500);
}

TEST_CASE("PointDistanceParams - Load valid JSON with SetPoint", "[transforms][v2][params][json][pointdistance]") {
    std::string json = R"({
        "reference_type": "SetPoint",
        "reference_x": 100.5,
        "reference_y": 200.75
    })";

    auto result = loadParametersFromJson<PointDistanceParams>(json);

    REQUIRE(result);
    auto params = result.value();

    REQUIRE(params.reference_type == ReferenceType::SetPoint);
    REQUIRE_THAT(params.getReferenceX(), Catch::Matchers::WithinRel(100.5f, 0.001f));
    REQUIRE_THAT(params.getReferenceY(), Catch::Matchers::WithinRel(200.75f, 0.001f));
}

TEST_CASE("PointDistanceParams - Load empty JSON uses defaults", "[transforms][v2][params][json][pointdistance]") {
    std::string json = "{}";

    auto result = loadParametersFromJson<PointDistanceParams>(json);

    REQUIRE(result);
    auto params = result.value();

    REQUIRE(params.reference_type == ReferenceType::GlobalAverage);
    REQUIRE(params.getWindowSize() == 1000); // Default window size
}

TEST_CASE("PointDistanceParams - Reject invalid window_size", "[transforms][v2][params][json][validation][pointdistance]") {
    std::string json = R"({
        "reference_type": "RollingAverage",
        "window_size": 0
    })";

    auto result = loadParametersFromJson<PointDistanceParams>(json);

    REQUIRE(!result); // Should fail validation (must be >= 1)
}

TEST_CASE("PointDistanceParams - JSON round-trip preserves values", "[transforms][v2][params][json][pointdistance]") {
    PointDistanceParams original;
    original.reference_type = ReferenceType::SetPoint;
    original.window_size = 2000;
    original.reference_x = 150.5f;
    original.reference_y = 250.75f;

    // Serialize
    std::string json = saveParametersToJson(original);

    // Deserialize
    auto result = loadParametersFromJson<PointDistanceParams>(json);
    REQUIRE(result);

    auto params = result.value();
    REQUIRE(params.reference_type == ReferenceType::SetPoint);
    REQUIRE(params.getWindowSize() == 2000);
    REQUIRE_THAT(params.getReferenceX(), Catch::Matchers::WithinRel(150.5f, 0.001f));
    REQUIRE_THAT(params.getReferenceY(), Catch::Matchers::WithinRel(250.75f, 0.001f));
}

// ============================================================================
// Core Functionality Tests
// ============================================================================

TEST_CASE("PointDistance - Empty point data", "[transforms][v2][pointdistance]") {
    PointData point_data;
    PointDistanceParams params;
    params.reference_type = ReferenceType::GlobalAverage;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.empty());
}

TEST_CASE("PointDistance - Single point GlobalAverage", "[transforms][v2][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::GlobalAverage;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 1);
    REQUIRE(results[0].time == 0);
    // Distance from single point to itself (average) should be 0
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("PointDistance - Multiple points GlobalAverage", "[transforms][v2][pointdistance]") {
    PointData point_data;
    // Create a square: (0,0), (10,0), (10,10), (0,10)
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{10.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(2), Point2D<float>{10.0f, 10.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(3), Point2D<float>{0.0f, 10.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::GlobalAverage;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 4);

    // Average should be at (5, 5)
    // Distance from each corner to center should be sqrt(50) ≈ 7.071
    for (auto const& result : results) {
        REQUIRE_THAT(result.distance, Catch::Matchers::WithinRel(7.071f, 0.01f));
    }
}

TEST_CASE("PointDistance - SetPoint reference", "[transforms][v2][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::SetPoint;
    params.reference_x = 0.0f;
    params.reference_y = 0.0f;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 1);
    REQUIRE(results[0].time == 0);
    // Distance from (3,4) to (0,0) should be 5 (3-4-5 triangle)
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("PointDistance - RollingAverage with window", "[transforms][v2][pointdistance]") {
    PointData point_data;
    // Linear motion from (0,0) to (100,0)
    for (int i = 0; i <= 10; ++i) {
        point_data.addAtTime(TimeFrameIndex(i), Point2D<float>{float(i * 10), 0.0f}, NotifyObservers::No);
    }

    PointDistanceParams params;
    params.reference_type = ReferenceType::RollingAverage;
    params.window_size = 3; // Small window for testing

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 11);

    // At time 5 (position 50,0), rolling average should be around (50,0)
    // so distance should be small
    auto it = std::find_if(results.begin(), results.end(),
        [](auto const& r) { return r.time == 5; });
    REQUIRE(it != results.end());
    REQUIRE_THAT(it->distance, Catch::Matchers::WithinAbs(0.0f, 10.0f)); // Should be small
}

TEST_CASE("PointDistance - OtherPointData reference", "[transforms][v2][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{3.0f, 0.0f}, NotifyObservers::No);

    PointData reference_data;
    reference_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 4.0f}, NotifyObservers::No);
    reference_data.addAtTime(TimeFrameIndex(1), Point2D<float>{0.0f, 4.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::OtherPointData;

    auto results = calculatePointDistance(point_data, params, &reference_data);

    REQUIRE(results.size() == 2);

    // At time 0: distance from (0,0) to (0,4) = 4
    REQUIRE(results[0].time == 0);
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinAbs(4.0f, 0.001f));

    // At time 1: distance from (3,0) to (0,4) = 5 (3-4-5 triangle)
    REQUIRE(results[1].time == 1);
    REQUIRE_THAT(results[1].distance, Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("PointDistance - OtherPointData with missing times", "[transforms][v2][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{1.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(2), Point2D<float>{2.0f, 0.0f}, NotifyObservers::No);

    PointData reference_data;
    // Only has data at time 0 and 2, missing time 1
    reference_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 1.0f}, NotifyObservers::No);
    reference_data.addAtTime(TimeFrameIndex(2), Point2D<float>{0.0f, 1.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::OtherPointData;

    auto results = calculatePointDistance(point_data, params, &reference_data);

    // Should only have results for times 0 and 2 (time 1 is missing in reference)
    REQUIRE(results.size() == 2);
    REQUIRE(results[0].time == 0);
    REQUIRE(results[1].time == 2);
}

TEST_CASE("PointDistance - OtherPointData with null reference", "[transforms][v2][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{1.0f, 1.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::OtherPointData;

    auto results = calculatePointDistance(point_data, params, nullptr);

    // Should return empty results when reference is null
    REQUIRE(results.empty());
}

TEST_CASE("PointDistance - Multiple points at same time", "[transforms][v2][pointdistance]") {
    PointData point_data;
    // Add multiple points at the same time
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 5.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::GlobalAverage;

    auto results = calculatePointDistance(point_data, params, nullptr);

    // Should have 3 results, one for each point
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].time == 0);
    REQUIRE(results[1].time == 0);
    REQUIRE(results[2].time == 0);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("PointDistance - Negative coordinates", "[transforms][v2][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{-10.0f, -20.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::SetPoint;
    params.reference_x = 0.0f;
    params.reference_y = 0.0f;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 1);
    // Distance from (-10,-20) to (0,0) = sqrt(500) ≈ 22.36
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinRel(22.36f, 0.01f));
}

TEST_CASE("PointDistance - Very large coordinates", "[transforms][v2][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{10000.0f, 10000.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::SetPoint;
    params.reference_x = 0.0f;
    params.reference_y = 0.0f;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 1);
    // Distance should be sqrt(2 * 10000^2) ≈ 14142.14
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinRel(14142.14f, 1.0f));
}

TEST_CASE("PointDistance - Zero distance", "[transforms][v2][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 5.0f}, NotifyObservers::No);

    PointDistanceParams params;
    params.reference_type = ReferenceType::SetPoint;
    params.reference_x = 5.0f;
    params.reference_y = 5.0f;

    auto results = calculatePointDistance(point_data, params, nullptr);

    REQUIRE(results.size() == 1);
    REQUIRE_THAT(results[0].distance, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

