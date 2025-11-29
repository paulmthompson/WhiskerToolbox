#include <catch2/catch_test_macros.hpp>
#include "transforms/v2/core/RaggedZipView.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include <vector>
#include <ranges>

using namespace WhiskerToolbox::Transforms::V2;

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

} // namespace

TEST_CASE("RaggedZipView - 1:1 Matching", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}}}, {1, {{3.0f, 3.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}}}, {1, {{30.0f, 30.0f}}}});

    auto v1 = d1->time_slices();
    auto v2 = d2->time_slices();

    RaggedZipView zip(v1, v2);
    
    auto it = zip.begin();
    REQUIRE(it != zip.end());
    
    auto [t1, v1_val, v2_val] = *it;
    REQUIRE(t1 == TimeFrameIndex(0));
    REQUIRE(v1_val.data.x == 1.0f);
    REQUIRE(v2_val.data.x == 10.0f);
    
    ++it;
    auto [t2, v1_val2, v2_val2] = *it;
    REQUIRE(t2 == TimeFrameIndex(0));
    REQUIRE(v1_val2.data.x == 2.0f);
    REQUIRE(v2_val2.data.x == 20.0f);

    ++it;
    auto [t3, v1_val3, v2_val3] = *it;
    REQUIRE(t3 == TimeFrameIndex(1));
    REQUIRE(v1_val3.data.x == 3.0f);
    REQUIRE(v2_val3.data.x == 30.0f);
    
    ++it;
    REQUIRE(it == zip.end());
}

TEST_CASE("RaggedZipView - Broadcast Right", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}}}}); // Size 1, should broadcast

    auto v1 = d1->time_slices();
    auto v2 = d2->time_slices();

    RaggedZipView zip(v1, v2);
    
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        REQUIRE(time == TimeFrameIndex(0));
        REQUIRE(val2.data.x == 10.0f); // Always 10
        if (count == 0) REQUIRE(val1.data.x == 1.0f);
        if (count == 1) REQUIRE(val1.data.x == 2.0f);
        if (count == 2) REQUIRE(val1.data.x == 3.0f);
        count++;
    }
    REQUIRE(count == 3);
}

TEST_CASE("RaggedZipView - Broadcast Left", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{5.0f, 5.0f}}}}); // Size 1, should broadcast
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}}}});

    auto v1 = d1->time_slices();
    auto v2 = d2->time_slices();

    RaggedZipView zip(v1, v2);
    
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        REQUIRE(time == TimeFrameIndex(0));
        REQUIRE(val1.data.x == 5.0f); // Always 5
        if (count == 0) REQUIRE(val2.data.x == 10.0f);
        if (count == 1) REQUIRE(val2.data.x == 20.0f);
        count++;
    }
    REQUIRE(count == 2);
}

TEST_CASE("RaggedZipView - Shape Mismatch", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}, {30.0f, 30.0f}}}}); // Size mismatch (2 vs 3)

    auto v1 = d1->time_slices();
    auto v2 = d2->time_slices();

    RaggedZipView zip(v1, v2);
    
    REQUIRE_THROWS_AS([&](){
        auto it = zip.begin(); // Constructor of iterator sets up first step
    }(), std::runtime_error);
}

TEST_CASE("RaggedZipView - Time Alignment", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}}}, {2, {{2.0f, 2.0f}}}}); // Missing T=1
    auto d2 = createPointData({{0, {{10.0f, 10.0f}}}, {1, {{20.0f, 20.0f}}}, {2, {{30.0f, 30.0f}}}});

    auto v1 = d1->time_slices();
    auto v2 = d2->time_slices();

    RaggedZipView zip(v1, v2);
    
    // Should only yield T=0 and T=2
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        if (count == 0) {
            REQUIRE(time == TimeFrameIndex(0));
            REQUIRE(val1.data.x == 1.0f);
            REQUIRE(val2.data.x == 10.0f);
        }
        if (count == 1) {
            REQUIRE(time == TimeFrameIndex(2));
            REQUIRE(val1.data.x == 2.0f);
            REQUIRE(val2.data.x == 30.0f);
        }
        count++;
    }
    REQUIRE(count == 2);
}
