#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "algorithms/PointCoordinateReduction/PointCoordinateReduction.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

TEST_CASE("PointCoordinateReduction FirstReduction") {
    // Setup
    PointData input;
    input.addAtTime(TimeFrameIndex(1), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
    input.addAtTime(TimeFrameIndex(1), Point2D<float>{15.0f, 25.0f}, NotifyObservers::No);
    input.addAtTime(TimeFrameIndex(2), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);

    PointCoordinateReductionParams params;
    params.coordinate = PointCoordinateComponent::X;
    params.reduction = PointReductionMethod::First;

    ComputeContext ctx;
    auto output = pointCoordinateReduction(input, params, ctx);

    REQUIRE(output != nullptr);
    REQUIRE(output->getNumSamples() == 2);
    
    auto ts = output->getTimeSeries();
    REQUIRE(ts[0] == TimeFrameIndex(1));
    REQUIRE(ts[1] == TimeFrameIndex(2));
    
    REQUIRE_THAT(output->getAtTime(TimeFrameIndex(1)).value(), Catch::Matchers::WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(output->getAtTime(TimeFrameIndex(2)).value(), Catch::Matchers::WithinAbs(30.0f, 1e-5f));
}

TEST_CASE("PointCoordinateReduction MeanReduction") {
    PointData input;
    input.addAtTime(TimeFrameIndex(1), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
    input.addAtTime(TimeFrameIndex(1), Point2D<float>{20.0f, 30.0f}, NotifyObservers::No);
    input.addAtTime(TimeFrameIndex(2), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);

    PointCoordinateReductionParams params;
    params.coordinate = PointCoordinateComponent::Y; // Test Y coordinate
    params.reduction = PointReductionMethod::Mean;

    ComputeContext ctx;
    auto output = pointCoordinateReduction(input, params, ctx);

    REQUIRE(output != nullptr);
    REQUIRE_THAT(output->getAtTime(TimeFrameIndex(1)).value(), Catch::Matchers::WithinAbs(25.0f, 1e-5f));
    REQUIRE_THAT(output->getAtTime(TimeFrameIndex(2)).value(), Catch::Matchers::WithinAbs(40.0f, 1e-5f));
}

TEST_CASE("PointCoordinateReduction EmptyData") {
    PointData input;
    PointCoordinateReductionParams params;
    ComputeContext ctx;
    auto output = pointCoordinateReduction(input, params, ctx);

    REQUIRE(output != nullptr);
    REQUIRE(output->getNumSamples() == 0);
}
