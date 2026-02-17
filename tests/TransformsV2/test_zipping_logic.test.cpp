#include <catch2/catch_test_macros.hpp>
#include "transforms/v2/core/FlatZipView.hpp"
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

TEST_CASE("FlatZipView - 1:1 Matching", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}}}, {1, {{3.0f, 3.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}}}, {1, {{30.0f, 30.0f}}}});

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    FlatZipView zip(
        std::span{storage1.getTimes()},
        std::span{storage1.getData()},
        std::span{storage2.getTimes()},
        std::span{storage2.getData()}
    );
    
    auto it = zip.begin();
    REQUIRE(it != zip.end());
    
    auto [t1, v1, v2] = *it;
    REQUIRE(t1 == TimeFrameIndex(0));
    REQUIRE(v1.x == 1.0f);
    REQUIRE(v2.x == 10.0f);
    
    ++it;
    auto [t2, v1_2, v2_2] = *it;
    REQUIRE(t2 == TimeFrameIndex(0));
    REQUIRE(v1_2.x == 2.0f);
    REQUIRE(v2_2.x == 20.0f);

    ++it;
    auto [t3, v1_3, v2_3] = *it;
    REQUIRE(t3 == TimeFrameIndex(1));
    REQUIRE(v1_3.x == 3.0f);
    REQUIRE(v2_3.x == 30.0f);
    
    ++it;
    REQUIRE(it == zip.end());
}

TEST_CASE("FlatZipView - Broadcast Right", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}}}}); // Size 1, should broadcast

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    FlatZipView zip(
        std::span{storage1.getTimes()},
        std::span{storage1.getData()},
        std::span{storage2.getTimes()},
        std::span{storage2.getData()}
    );
    
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        REQUIRE(time == TimeFrameIndex(0));
        REQUIRE(val2.x == 10.0f); // Always 10
        if (count == 0) REQUIRE(val1.x == 1.0f);
        if (count == 1) REQUIRE(val1.x == 2.0f);
        if (count == 2) REQUIRE(val1.x == 3.0f);
        count++;
    }
    REQUIRE(count == 3);
}

TEST_CASE("FlatZipView - Broadcast Left", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{5.0f, 5.0f}}}}); // Size 1, should broadcast
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}}}});

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    FlatZipView zip(
        std::span{storage1.getTimes()},
        std::span{storage1.getData()},
        std::span{storage2.getTimes()},
        std::span{storage2.getData()}
    );
    
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        REQUIRE(time == TimeFrameIndex(0));
        REQUIRE(val1.x == 5.0f); // Always 5
        if (count == 0) REQUIRE(val2.x == 10.0f);
        if (count == 1) REQUIRE(val2.x == 20.0f);
        count++;
    }
    REQUIRE(count == 2);
}

TEST_CASE("FlatZipView - Shape Mismatch", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}, {2.0f, 2.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}, {20.0f, 20.0f}, {30.0f, 30.0f}}}}); // Size mismatch (2 vs 3)

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    FlatZipView zip(
        std::span{storage1.getTimes()},
        std::span{storage1.getData()},
        std::span{storage2.getTimes()},
        std::span{storage2.getData()}
    );
    
    REQUIRE_THROWS_AS([&](){
        auto it = zip.begin(); // Iterator setup triggers setupCurrentTime which throws
        (void)*it; // Dereference to ensure it's evaluated
    }(), std::runtime_error);
}

TEST_CASE("FlatZipView - Time Alignment", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}}}, {2, {{2.0f, 2.0f}}}}); // Missing T=1
    auto d2 = createPointData({{0, {{10.0f, 10.0f}}}, {1, {{20.0f, 20.0f}}}, {2, {{30.0f, 30.0f}}}});

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    FlatZipView zip(
        std::span{storage1.getTimes()},
        std::span{storage1.getData()},
        std::span{storage2.getTimes()},
        std::span{storage2.getData()}
    );
    
    // Should only yield T=0 and T=2
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        if (count == 0) {
            REQUIRE(time == TimeFrameIndex(0));
            REQUIRE(val1.x == 1.0f);
            REQUIRE(val2.x == 10.0f);
        }
        if (count == 1) {
            REQUIRE(time == TimeFrameIndex(2));
            REQUIRE(val1.x == 2.0f);
            REQUIRE(val2.x == 30.0f);
        }
        count++;
    }
    REQUIRE(count == 2);
}

TEST_CASE("FlatZipView - makeZipView helper", "[transforms][v2][zipping]") {
    auto d1 = createPointData({{0, {{1.0f, 1.0f}}}, {1, {{2.0f, 2.0f}}}});
    auto d2 = createPointData({{0, {{10.0f, 10.0f}}}, {1, {{20.0f, 20.0f}}}});

    auto const& storage1 = d1->getStorage();
    auto const& storage2 = d2->getStorage();

    // Use the helper function
    auto zip = makeZipView(storage1, storage2);
    
    int count = 0;
    for (auto [time, val1, val2] : zip) {
        if (count == 0) {
            REQUIRE(time == TimeFrameIndex(0));
            REQUIRE(val1.x == 1.0f);
            REQUIRE(val2.x == 10.0f);
        }
        if (count == 1) {
            REQUIRE(time == TimeFrameIndex(1));
            REQUIRE(val1.x == 2.0f);
            REQUIRE(val2.x == 20.0f);
        }
        count++;
    }
    REQUIRE(count == 2);
}
