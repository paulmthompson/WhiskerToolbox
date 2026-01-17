#include <catch2/catch_test_macros.hpp>
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/detail/FlatZipView.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include <vector>
#include <ranges>
#include <tuple>
#include <rfl.hpp>
#include <rfl/json.hpp>

using namespace WhiskerToolbox::Transforms::V2;

// Helper to create PointData
std::shared_ptr<PointData> createPointDataForPipeline(std::vector<std::pair<int, std::vector<Point2D<float>>>> const& data) {
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
std::shared_ptr<LineData> createLineDataForPipeline(std::vector<std::pair<int, std::vector<Line2D>>> const& data) {
    auto ld = std::make_shared<LineData>();
    for (auto const& [time, lines] : data) {
        for (auto const& l : lines) {
            Line2D l_copy = l;
            ld->addAtTime(TimeFrameIndex(time), std::move(l_copy), NotifyObservers::No);
        }
    }
    return ld;
}

// Dummy transform params
struct TestDistParams {};
// REFLECTCPP_STRUCT(TestDistParams); // Not needed/defined

// Dummy binary transform function
float calculateTestDistance(std::tuple<Line2D, Point2D<float>> const& input, TestDistParams const&) {
    auto const& [line, point] = input;
    // Simple "distance": line.front().x + point.x
    return line.front().x + point.x;
}

TEST_CASE("TransformPipeline - Multi-Input Execution", "[transforms][v2][pipeline][integration]") {
    // 1. Register the binary transform
    auto& registry = ElementRegistry::instance();
    
    // Only register if not already registered (to avoid double registration in repeated test runs)
    if (!registry.hasTransform("TestBinaryDist")) {
        TransformMetadata meta;
        meta.description = "Test binary transform";
        meta.input_type_name = "tuple<Line2D, Point2D>";
        meta.output_type_name = "float";
        
        registry.registerTransform<std::tuple<Line2D, Point2D<float>>, float, TestDistParams>(
            "TestBinaryDist",
            calculateTestDistance,
            meta
        );
    }

    // 2. Create Input Data
    // T=0: Line(x=10) + Point(x=1) -> 11
    // T=1: Line(x=20) + Point(x=2) -> 22
    auto lines = createLineDataForPipeline({
        {0, {Line2D(Point2D<float>(10.0f, 0.0f), Point2D<float>(10.0f, 10.0f))}},
        {1, {Line2D(Point2D<float>(20.0f, 0.0f), Point2D<float>(20.0f, 10.0f))}}
    });
    
    auto points = createPointDataForPipeline({
        {0, {{1.0f, 1.0f}}},
        {1, {{2.0f, 2.0f}}}
    });

    FlatZipView zip_view(lines->elements(), points->elements());


    // 4. Adapt View to pair<Time, tuple> format expected by pipeline
    auto pipeline_input_view = zip_view | std::views::transform([](auto const& triplet) {
        auto const& [time, line, point] = triplet;
        // Data is already raw (Line2D, Point2D<float>), no DataEntry wrapper
        return std::make_pair(time, std::make_tuple(line, point));
    });

    // 5. Create Pipeline
    TransformPipeline pipeline;
    pipeline.addStep<TestDistParams>("TestBinaryDist", TestDistParams{});

    // 6. Execute Pipeline using new executeFromView
    // We must specify the InputElement type explicitly as tuple<Line2D, Point2D<float>>
    using InputTuple = std::tuple<Line2D, Point2D<float>>;
    auto result_view = pipeline.executeFromView<InputTuple>(pipeline_input_view);

    // 7. Verify Results
    int count = 0;
    for (auto [time, result_variant] : result_view) {
        float val = std::get<float>(result_variant);
        
        if (count == 0) {
            REQUIRE(time == TimeFrameIndex(0));
            REQUIRE(val == 11.0f);
        } else if (count == 1) {
            REQUIRE(time == TimeFrameIndex(1));
            REQUIRE(val == 22.0f);
        }
        count++;
    }
    REQUIRE(count == 2);
}
