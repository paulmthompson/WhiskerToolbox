#include "point_distance.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// ============================================================================
// Tests: pointDistance() Function
// ============================================================================

TEST_CASE("pointDistance - Empty point data", "[transforms][pointdistance]") {
    PointData point_data;

    auto result = pointDistance(&point_data, PointDistanceReferenceType::GlobalAverage);

    REQUIRE(result != nullptr);
    REQUIRE(result->empty());
}

TEST_CASE("pointDistance - Single point GlobalAverage", "[transforms][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::GlobalAverage);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("pointDistance - Multiple points GlobalAverage", "[transforms][pointdistance]") {
    PointData point_data;
    // Create a square: (0,0), (10,0), (10,10), (0,10)
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{10.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(2), Point2D<float>{10.0f, 10.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(3), Point2D<float>{0.0f, 10.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::GlobalAverage);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 4);

    // Average should be at (5, 5), distance from each corner should be sqrt(50) ≈ 7.071
    for (int i = 0; i < 4; ++i) {
        auto val = result->getDataAtTime(TimeFrameIndex(i));
        REQUIRE(val.has_value());
        REQUIRE_THAT(val.value(), Catch::Matchers::WithinRel(7.071f, 0.01f));
    }
}

TEST_CASE("pointDistance - SetPoint reference", "[transforms][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::SetPoint,
                               1000, 0.0f, 0.0f, nullptr);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    // Distance from (3,4) to (0,0) should be 5 (3-4-5 triangle)
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("pointDistance - RollingAverage with window", "[transforms][pointdistance]") {
    PointData point_data;
    // Linear motion from (0,0) to (100,0)
    for (int i = 0; i <= 10; ++i) {
        point_data.addAtTime(TimeFrameIndex(i), Point2D<float>{float(i * 10), 0.0f}, NotifyObservers::No);
    }

    auto result = pointDistance(&point_data, PointDistanceReferenceType::RollingAverage,
                               3, 0.0f, 0.0f, nullptr);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 11);

    // At time 5 (position 50,0), rolling average should be around (50,0)
    auto val = result->getDataAtTime(TimeFrameIndex(5));
    REQUIRE(val.has_value());
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(0.0f, 10.0f));
}

TEST_CASE("pointDistance - OtherPointData reference", "[transforms][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{3.0f, 0.0f}, NotifyObservers::No);

    PointData reference_data;
    reference_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 4.0f}, NotifyObservers::No);
    reference_data.addAtTime(TimeFrameIndex(1), Point2D<float>{0.0f, 4.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::OtherPointData,
                               1000, 0.0f, 0.0f, &reference_data);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 2);

    // At time 0: distance from (0,0) to (0,4) = 4
    auto val0 = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val0.has_value());
    REQUIRE_THAT(val0.value(), Catch::Matchers::WithinAbs(4.0f, 0.001f));

    // At time 1: distance from (3,0) to (0,4) = 5 (3-4-5 triangle)
    auto val1 = result->getDataAtTime(TimeFrameIndex(1));
    REQUIRE(val1.has_value());
    REQUIRE_THAT(val1.value(), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("pointDistance - OtherPointData with missing times", "[transforms][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(1), Point2D<float>{1.0f, 0.0f}, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(2), Point2D<float>{2.0f, 0.0f}, NotifyObservers::No);

    PointData reference_data;
    // Only has data at time 0 and 2, missing time 1
    reference_data.addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 1.0f}, NotifyObservers::No);
    reference_data.addAtTime(TimeFrameIndex(2), Point2D<float>{0.0f, 1.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::OtherPointData,
                               1000, 0.0f, 0.0f, &reference_data);

    REQUIRE(result != nullptr);
    // Should only have results for times 0 and 2 (time 1 is missing in reference)
    REQUIRE(result->size() == 2);
    REQUIRE(result->getDataAtTime(TimeFrameIndex(0)).has_value());
    REQUIRE(result->getDataAtTime(TimeFrameIndex(2)).has_value());
}

TEST_CASE("pointDistance - OtherPointData with null reference", "[transforms][pointdistance]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{1.0f, 1.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::OtherPointData,
                               1000, 0.0f, 0.0f, nullptr);

    REQUIRE(result != nullptr);
    // Should return empty results when reference is null
    REQUIRE(result->empty());
}

TEST_CASE("pointDistance - Null input", "[transforms][pointdistance]") {
    auto result = pointDistance(nullptr, PointDistanceReferenceType::GlobalAverage);

    REQUIRE(result == nullptr);
}

TEST_CASE("pointDistance - Negative coordinates", "[transforms][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{-10.0f, -20.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::SetPoint,
                               1000, 0.0f, 0.0f, nullptr);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    // Distance from (-10,-20) to (0,0) = sqrt(500) ≈ 22.36
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinRel(22.36f, 0.01f));
}

TEST_CASE("pointDistance - Very large coordinates", "[transforms][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{10000.0f, 10000.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::SetPoint,
                               1000, 0.0f, 0.0f, nullptr);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    // Distance should be sqrt(2 * 10000^2) ≈ 14142.14
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinRel(14142.14f, 1.0f));
}

TEST_CASE("pointDistance - Zero distance", "[transforms][pointdistance][edge]") {
    PointData point_data;
    point_data.addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 5.0f}, NotifyObservers::No);

    auto result = pointDistance(&point_data, PointDistanceReferenceType::SetPoint,
                               1000, 5.0f, 5.0f, nullptr);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

// ============================================================================
// Tests: PointDistanceOperation
// ============================================================================

TEST_CASE("PointDistanceOperation - getName", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    REQUIRE(op.getName() == "Calculate Point Distance");
}

TEST_CASE("PointDistanceOperation - getTargetInputTypeIndex", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    REQUIRE(op.getTargetInputTypeIndex() == typeid(std::shared_ptr<PointData>));
}

TEST_CASE("PointDistanceOperation - getDefaultParameters", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    auto params = op.getDefaultParameters();

    REQUIRE(params != nullptr);
    auto* point_params = dynamic_cast<PointDistanceParameters*>(params.get());
    REQUIRE(point_params != nullptr);
    REQUIRE(point_params->reference_type == PointDistanceReferenceType::GlobalAverage);
    REQUIRE(point_params->window_size == 1000);
}

TEST_CASE("PointDistanceOperation - canApply with valid PointData", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    auto point_data = std::make_shared<PointData>();
    DataTypeVariant variant = point_data;

    REQUIRE(op.canApply(variant));
}

TEST_CASE("PointDistanceOperation - canApply with null PointData", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    std::shared_ptr<PointData> null_ptr;
    DataTypeVariant variant = null_ptr;

    REQUIRE(!op.canApply(variant));
}

TEST_CASE("PointDistanceOperation - canApply with wrong type", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;
    auto analog_ts = std::make_shared<AnalogTimeSeries>();
    DataTypeVariant variant = analog_ts;

    REQUIRE(!op.canApply(variant));
}

TEST_CASE("PointDistanceOperation - execute with valid data", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;

    auto point_data = std::make_shared<PointData>();
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);

    PointDistanceParameters params;
    params.reference_type = PointDistanceReferenceType::SetPoint;
    params.reference_x = 0.0f;
    params.reference_y = 0.0f;

    DataTypeVariant variant = point_data;
    auto result_variant = op.execute(variant, &params);

    REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result_variant));
    auto result = std::get<std::shared_ptr<AnalogTimeSeries>>(result_variant);
    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

TEST_CASE("PointDistanceOperation - execute with null data", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;

    std::shared_ptr<PointData> null_ptr;
    DataTypeVariant variant = null_ptr;

    PointDistanceParameters params;
    auto result_variant = op.execute(variant, &params);

    // Should return empty variant on error
    REQUIRE(std::holds_alternative<std::monostate>(result_variant));
}

TEST_CASE("PointDistanceOperation - execute with invalid parameters", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;

    auto point_data = std::make_shared<PointData>();
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{1.0f, 1.0f}, NotifyObservers::No);

    DataTypeVariant variant = point_data;

    // Pass wrong parameter type (nullptr or wrong type)
    auto result_variant = op.execute(variant, nullptr);

    // Should return empty variant on error
    REQUIRE(std::holds_alternative<std::monostate>(result_variant));
}

TEST_CASE("PointDistanceOperation - execute with OtherPointData reference", "[transforms][pointdistance][operation]") {
    PointDistanceOperation op;

    auto point_data = std::make_shared<PointData>();
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{0.0f, 0.0f}, NotifyObservers::No);

    auto ref_data = std::make_shared<PointData>();
    ref_data->addAtTime(TimeFrameIndex(0), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);

    PointDistanceParameters params;
    params.reference_type = PointDistanceReferenceType::OtherPointData;
    params.reference_point_data = ref_data;

    DataTypeVariant variant = point_data;
    auto result_variant = op.execute(variant, &params);

    REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result_variant));
    auto result = std::get<std::shared_ptr<AnalogTimeSeries>>(result_variant);
    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 1);

    auto val = result->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(val.has_value());
    REQUIRE_THAT(val.value(), Catch::Matchers::WithinAbs(5.0f, 0.001f));
}

