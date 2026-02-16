/**
 * @file Point_Data.test.cpp
 * @brief PointData-specific unit tests
 *
 * Shared RaggedTimeSeries behaviour (add/get/clear, entity IDs, observers,
 * range queries, copy/move by EntityID) is covered by the templated tests in
 *   tests/DataManager/ragged_shared_unit.test.cpp
 *   tests/DataManager/ragged_entity_integration.test.cpp
 *
 * This file only tests behaviour that is unique to PointData:
 *   - ImageSize get/set and changeImageSize scaling
 *   - Construction from std::map
 *   - Timeframe conversion (getAtTime with target TimeFrame)
 *   - View / flatten / ranges integration
 */

#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <vector>

// ============================================================================
// ImageSize
// ============================================================================
TEST_CASE("PointData - ImageSize get/set", "[point][imagesize]") {
    PointData point_data;

    ImageSize size{640, 480};
    point_data.setImageSize(size);

    auto retrieved = point_data.getImageSize();
    REQUIRE(retrieved.width == 640);
    REQUIRE(retrieved.height == 480);
}

// ============================================================================
// Image scaling via changeImageSize
// ============================================================================
TEST_CASE("PointData - changeImageSize scaling", "[point][imagesize][scaling]") {
    PointData point_data;

    std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);

    SECTION("Scaling from known size") {
        point_data.setImageSize(ImageSize{640, 480});
        point_data.changeImageSize(ImageSize{1280, 960});

        auto scaled = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(scaled[0].x == Catch::Approx(200.0f));
        REQUIRE(scaled[0].y == Catch::Approx(400.0f));
        REQUIRE(scaled[1].x == Catch::Approx(600.0f));
        REQUIRE(scaled[1].y == Catch::Approx(800.0f));

        auto sz = point_data.getImageSize();
        REQUIRE(sz.width == 1280);
        REQUIRE(sz.height == 960);
    }

    SECTION("Scaling with no initial size leaves points unchanged") {
        point_data.changeImageSize(ImageSize{1280, 960});

        auto pts = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(pts[0].x == Catch::Approx(100.0f));
        REQUIRE(pts[0].y == Catch::Approx(200.0f));

        auto sz = point_data.getImageSize();
        REQUIRE(sz.width == 1280);
        REQUIRE(sz.height == 960);
    }

    SECTION("Scaling to same size is a no-op") {
        point_data.setImageSize(ImageSize{640, 480});
        point_data.changeImageSize(ImageSize{640, 480});

        auto pts = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(pts[0].x == Catch::Approx(100.0f));
        REQUIRE(pts[0].y == Catch::Approx(200.0f));
    }
}

// ============================================================================
// Construction from std::map
// ============================================================================
TEST_CASE("PointData - Construction from map", "[point][data][construction]") {
    std::map<TimeFrameIndex, std::vector<Point2D<float>>> map_data = {
            {TimeFrameIndex(10), {{1.0f, 2.0f}, {3.0f, 4.0f}}},
            {TimeFrameIndex(20), {{5.0f, 6.0f}}}};

    PointData point_data(map_data);

    REQUIRE(point_data.getAtTime(TimeFrameIndex(10)).size() == 2);
    REQUIRE(point_data.getAtTime(TimeFrameIndex(20)).size() == 1);
    REQUIRE(point_data.getAtTime(TimeFrameIndex(10))[0].x == Catch::Approx(1.0f));
    REQUIRE(point_data.getAtTime(TimeFrameIndex(20))[0].x == Catch::Approx(5.0f));
}

// ============================================================================
// Timeframe conversion (getAtTime with target TimeFrame)
// ============================================================================
TEST_CASE("PointData - Timeframe conversion", "[point][data][timeframe]") {
    SECTION("Same timeframe returns original data") {
        PointData point_data;
        std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);

        auto timeframe = std::make_shared<TimeFrame>(std::vector<int>{5, 10, 15, 20, 25});
        point_data.setTimeFrame(timeframe);

        auto result = point_data.getAtTime(TimeFrameIndex(10), *timeframe);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
    }

    SECTION("Different timeframes with conversion") {
        auto video_tf = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40});
        auto data_tf  = std::make_shared<TimeFrame>(std::vector<int>{0, 5, 10, 15, 20, 25, 30, 35, 40});

        PointData point_data;
        std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
        point_data.addAtTime(TimeFrameIndex(2), points, NotifyObservers::No); // data idx 2 → time=10
        point_data.addAtTime(TimeFrameIndex(4), points, NotifyObservers::No); // data idx 4 → time=20
        point_data.setTimeFrame(data_tf);

        // Video frame 1 (time=10) → data idx 2
        auto result = point_data.getAtTime(TimeFrameIndex(1), *video_tf);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
    }

    SECTION("Non-matching conversion returns empty") {
        auto video_tf = std::make_shared<TimeFrame>(std::vector<int>{0, 5, 10});
        auto data_tf  = std::make_shared<TimeFrame>(std::vector<int>{0, 3, 7, 15, 25});

        PointData point_data;
        std::vector<Point2D<float>> points = {{100.0f, 200.0f}};
        point_data.addAtTime(TimeFrameIndex(3), points, NotifyObservers::No); // data idx 3 → time=15
        point_data.setTimeFrame(data_tf);

        auto result = point_data.getAtTime(TimeFrameIndex(1), *video_tf);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// View / flatten / ranges integration
// ============================================================================
TEST_CASE("PointData - View functionality", "[point][data][view][ranges]") {
    PointData point_data;

    Point2D<float> p1{1.0f, 2.0f};
    Point2D<float> p2{3.0f, 4.0f};
    Point2D<float> p3{5.0f, 6.0f};
    Point2D<float> p4{7.0f, 8.0f};
    Point2D<float> p5{9.0f, 10.0f};

    point_data.addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(30), p5, NotifyObservers::No);

    SECTION("Basic iteration counts all entries") {
        std::size_t count = 0;
        for (auto [time, entry] : point_data.view()) {
            static_cast<void>(time);
            static_cast<void>(entry);
            ++count;
        }
        REQUIRE(count == 5);
    }

    SECTION("Per-time entry counts") {
        std::map<TimeFrameIndex, std::size_t> counts;
        for (auto [time, entry] : point_data.view()) {
            static_cast<void>(entry);
            counts[time] += 1;
        }
        REQUIRE(counts[TimeFrameIndex(10)] == 2);
        REQUIRE(counts[TimeFrameIndex(20)] == 1);
        REQUIRE(counts[TimeFrameIndex(30)] == 2);
    }

    SECTION("Flatten preserves order and data") {
        auto flattened = point_data.view().flatten();

        std::vector<Point2D<float>> all_points;
        std::vector<TimeFrameIndex> all_times;
        for (auto [time, entry] : flattened) {
            all_times.push_back(time);
            all_points.push_back(entry.data);
        }

        REQUIRE(all_points.size() == 5);
        REQUIRE(all_times[0] == TimeFrameIndex(10));
        REQUIRE(all_times[4] == TimeFrameIndex(30));
        REQUIRE(all_points[0].x == Catch::Approx(1.0f));
        REQUIRE(all_points[4].x == Catch::Approx(9.0f));
    }

    SECTION("Empty data yields empty view") {
        PointData empty;
        std::size_t count = 0;
        for (auto [t, e] : empty.view()) {
            static_cast<void>(t); static_cast<void>(e);
            ++count;
        }
        REQUIRE(count == 0);
    }

    SECTION("ranges::find_if on flattened view") {
        auto flattened = point_data.view().flatten();

        auto it = std::ranges::find_if(flattened, [](auto const & elem) {
            auto [time, entry] = elem;
            return entry.data.x == Catch::Approx(5.0f) && entry.data.y == Catch::Approx(6.0f);
        });

        REQUIRE(it != flattened.end());
        auto [found_time, found_entry] = *it;
        REQUIRE(found_time == TimeFrameIndex(20));
    }

    SECTION("views::filter by time") {
        auto flattened = point_data.view().flatten();
        auto at_10 = flattened | std::views::filter([](auto const & elem) {
            return std::get<0>(elem) == TimeFrameIndex(10);
        });

        std::size_t count = 0;
        for (auto [time, entry] : at_10) {
            static_cast<void>(entry);
            REQUIRE(time == TimeFrameIndex(10));
            ++count;
        }
        REQUIRE(count == 2);
    }

    SECTION("views::transform extracts x coordinates") {
        auto x_coords = point_data.view().flatten()
            | std::views::transform([](auto const & elem) {
                  return std::get<1>(elem).data.x;
              });

        std::vector<float> xs(std::ranges::begin(x_coords), std::ranges::end(x_coords));
        REQUIRE(xs == std::vector<float>{1.0f, 3.0f, 5.0f, 7.0f, 9.0f});
    }

    SECTION("View is lazy — reflects mutations") {
        auto view = point_data.view();

        point_data.addAtTime(TimeFrameIndex(40), Point2D<float>{11.0f, 12.0f}, NotifyObservers::No);

        std::size_t count = 0;
        for (auto [t, e] : view) {
            static_cast<void>(t); static_cast<void>(e);
            ++count;
        }
        REQUIRE(count == 6);
    }

    SECTION("Flattened view exposes EntityIds") {
        std::vector<EntityId> ids;
        for (auto [time, entry] : point_data.view().flatten()) {
            static_cast<void>(time);
            ids.push_back(entry.entity_id);
        }
        REQUIRE(ids.size() == 5);

        std::unordered_set<EntityId> unique(ids.begin(), ids.end());
        REQUIRE(unique.size() <= 5);
    }
}
