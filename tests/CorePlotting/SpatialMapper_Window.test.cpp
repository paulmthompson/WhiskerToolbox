#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Mappers/SpatialMapper_Window.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cstdlib>
#include <memory>
#include <vector>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::shared_ptr<TimeFrame> createLinearTimeFrame(int num_frames) {
    std::vector<int> times;
    times.reserve(static_cast<size_t>(num_frames));
    for (int i = 0; i < num_frames; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

Mask2D createSquareMask() {
    std::vector<Point2D<uint32_t>> pixels;
    for (uint32_t y = 0; y < 3; ++y) {
        for (uint32_t x = 0; x < 3; ++x) {
            pixels.push_back({x, y});
        }
    }
    return Mask2D{pixels};
}

} // namespace

// ============================================================================
// TimedMappedElement Tests
// ============================================================================

TEST_CASE("TimedMappedElement - basic construction", "[Mappers][SpatialMapper][Window]") {
    TimedMappedElement elem{1.0f, 2.0f, EntityId{10}, -3};
    REQUIRE_THAT(elem.x, WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(elem.y, WithinAbs(2.0, 1e-5));
    REQUIRE(elem.entity_id == EntityId{10});
    REQUIRE(elem.temporal_distance == -3);
    REQUIRE(elem.absTemporalDistance() == 3);
}

TEST_CASE("TimedMappedElement - from MappedElement", "[Mappers][SpatialMapper][Window]") {
    MappedElement base{5.0f, 6.0f, EntityId{7}};
    TimedMappedElement timed{base, 2};
    REQUIRE_THAT(timed.x, WithinAbs(5.0, 1e-5));
    REQUIRE_THAT(timed.y, WithinAbs(6.0, 1e-5));
    REQUIRE(timed.entity_id == EntityId{7});
    REQUIRE(timed.temporal_distance == 2);
}

TEST_CASE("TimedMappedElement - zero distance", "[Mappers][SpatialMapper][Window]") {
    TimedMappedElement elem{0.0f, 0.0f, EntityId{1}, 0};
    REQUIRE(elem.temporal_distance == 0);
    REQUIRE(elem.absTemporalDistance() == 0);
}

// ============================================================================
// TimedOwningLineView Tests
// ============================================================================

TEST_CASE("TimedOwningLineView - basic construction", "[Mappers][SpatialMapper][Window]") {
    std::vector<MappedVertex> verts = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    TimedOwningLineView view{EntityId{5}, verts, -2};

    REQUIRE(view.entity_id == EntityId{5});
    REQUIRE(view.temporal_distance == -2);
    REQUIRE(view.absTemporalDistance() == 2);
    REQUIRE(view.vertices().size() == 2);
    REQUIRE_THAT(view.vertices()[0].x, WithinAbs(1.0, 1e-5));
}

// ============================================================================
// SpatialMapper::mapPointsInWindow Tests
// ============================================================================

TEST_CASE("mapPointsInWindow - empty data", "[Mappers][SpatialMapper][Window]") {
    PointData points;
    auto tf = createLinearTimeFrame(20);
    points.setTimeFrame(tf);

    auto result = SpatialMapper::mapPointsInWindow(points, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.empty());
}

TEST_CASE("mapPointsInWindow - captures data in window", "[Mappers][SpatialMapper][Window]") {
    PointData points;
    auto tf = createLinearTimeFrame(20);
    points.setTimeFrame(tf);

    // Add points at times 3, 8, 10, 12, 17
    points.addAtTime(TimeFrameIndex{3}, std::vector<Point2D<float>>{{1.0f, 1.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{8}, std::vector<Point2D<float>>{{2.0f, 2.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{10}, std::vector<Point2D<float>>{{3.0f, 3.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{12}, std::vector<Point2D<float>>{{4.0f, 4.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{17}, std::vector<Point2D<float>>{{5.0f, 5.0f}}, NotifyObservers::No);

    // Window centered at 10 with ±5 → [5, 15]
    auto result = SpatialMapper::mapPointsInWindow(points, TimeFrameIndex{10}, 5, 5);

    // Should capture times 8, 10, 12 (3 is before, 17 is after)
    REQUIRE(result.size() == 3);
}

TEST_CASE("mapPointsInWindow - temporal distance is correct", "[Mappers][SpatialMapper][Window]") {
    PointData points;
    auto tf = createLinearTimeFrame(20);
    points.setTimeFrame(tf);

    points.addAtTime(TimeFrameIndex{7}, std::vector<Point2D<float>>{{1.0f, 1.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{10}, std::vector<Point2D<float>>{{2.0f, 2.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{13}, std::vector<Point2D<float>>{{3.0f, 3.0f}}, NotifyObservers::No);

    auto result = SpatialMapper::mapPointsInWindow(points, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.size() == 3);

    // Check temporal distances
    // t=7: distance = 7-10 = -3
    // t=10: distance = 0
    // t=13: distance = 3
    REQUIRE(result[0].temporal_distance == -3);
    REQUIRE(result[1].temporal_distance == 0);
    REQUIRE(result[2].temporal_distance == 3);
}

TEST_CASE("mapPointsInWindow - asymmetric window", "[Mappers][SpatialMapper][Window]") {
    PointData points;
    auto tf = createLinearTimeFrame(20);
    points.setTimeFrame(tf);

    points.addAtTime(TimeFrameIndex{5}, std::vector<Point2D<float>>{{1.0f, 1.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{8}, std::vector<Point2D<float>>{{2.0f, 2.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{10}, std::vector<Point2D<float>>{{3.0f, 3.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{11}, std::vector<Point2D<float>>{{4.0f, 4.0f}}, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{15}, std::vector<Point2D<float>>{{5.0f, 5.0f}}, NotifyObservers::No);

    // Window: center=10, behind=2, ahead=1 → [8, 11]
    auto result = SpatialMapper::mapPointsInWindow(points, TimeFrameIndex{10}, 2, 1);

    // Should capture times 8, 10, 11
    REQUIRE(result.size() == 3);
}

TEST_CASE("mapPointsInWindow - with scaling", "[Mappers][SpatialMapper][Window]") {
    PointData points;
    auto tf = createLinearTimeFrame(20);
    points.setTimeFrame(tf);

    points.addAtTime(TimeFrameIndex{10}, std::vector<Point2D<float>>{{5.0f, 10.0f}}, NotifyObservers::No);

    auto result = SpatialMapper::mapPointsInWindow(
        points, TimeFrameIndex{10}, 3, 3, 2.0f, 0.5f, 1.0f, -1.0f);
    REQUIRE(result.size() == 1);
    REQUIRE_THAT(result[0].x, WithinAbs(11.0, 1e-5));  // 5*2 + 1
    REQUIRE_THAT(result[0].y, WithinAbs(4.0, 1e-5));   // 10*0.5 + (-1)
    REQUIRE(result[0].temporal_distance == 0);
}

// ============================================================================
// SpatialMapper::mapLinesInWindow Tests
// ============================================================================

TEST_CASE("mapLinesInWindow - empty data", "[Mappers][SpatialMapper][Window]") {
    LineData lines;
    auto tf = createLinearTimeFrame(20);
    lines.setTimeFrame(tf);

    auto result = SpatialMapper::mapLinesInWindow(lines, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.empty());
}

TEST_CASE("mapLinesInWindow - captures lines in window", "[Mappers][SpatialMapper][Window]") {
    LineData lines;
    auto tf = createLinearTimeFrame(20);
    lines.setTimeFrame(tf);

    Line2D line1;
    line1.push_back(Point2D<float>{1.0f, 2.0f});
    Line2D line2;
    line2.push_back(Point2D<float>{3.0f, 4.0f});
    Line2D line3;
    line3.push_back(Point2D<float>{5.0f, 6.0f});

    lines.addAtTime(TimeFrameIndex{3}, std::vector<Line2D>{line1}, NotifyObservers::No);
    lines.addAtTime(TimeFrameIndex{10}, std::vector<Line2D>{line2}, NotifyObservers::No);
    lines.addAtTime(TimeFrameIndex{18}, std::vector<Line2D>{line3}, NotifyObservers::No);

    // Window: center=10, ±5 → [5, 15]
    auto result = SpatialMapper::mapLinesInWindow(lines, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.size() == 1);  // Only t=10
    REQUIRE(result[0].temporal_distance == 0);
}

TEST_CASE("mapLinesInWindow - temporal distance is correct", "[Mappers][SpatialMapper][Window]") {
    LineData lines;
    auto tf = createLinearTimeFrame(20);
    lines.setTimeFrame(tf);

    Line2D line;
    line.push_back(Point2D<float>{1.0f, 1.0f});

    lines.addAtTime(TimeFrameIndex{8}, std::vector<Line2D>{line}, NotifyObservers::No);
    lines.addAtTime(TimeFrameIndex{10}, std::vector<Line2D>{line}, NotifyObservers::No);
    lines.addAtTime(TimeFrameIndex{14}, std::vector<Line2D>{line}, NotifyObservers::No);

    auto result = SpatialMapper::mapLinesInWindow(lines, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.size() == 3);
    REQUIRE(result[0].temporal_distance == -2);
    REQUIRE(result[1].temporal_distance == 0);
    REQUIRE(result[2].temporal_distance == 4);
}

// ============================================================================
// SpatialMapper::mapMaskContoursInWindow Tests
// ============================================================================

TEST_CASE("mapMaskContoursInWindow - empty data", "[Mappers][SpatialMapper][Window]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(20);
    masks.setTimeFrame(tf);

    auto result = SpatialMapper::mapMaskContoursInWindow(masks, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.empty());
}

TEST_CASE("mapMaskContoursInWindow - captures masks in window", "[Mappers][SpatialMapper][Window]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(20);
    masks.setTimeFrame(tf);

    auto square = createSquareMask();
    masks.addAtTime(TimeFrameIndex{2}, std::vector<Mask2D>{square}, NotifyObservers::No);
    masks.addAtTime(TimeFrameIndex{9}, std::vector<Mask2D>{square}, NotifyObservers::No);
    masks.addAtTime(TimeFrameIndex{15}, std::vector<Mask2D>{square}, NotifyObservers::No);
    masks.addAtTime(TimeFrameIndex{19}, std::vector<Mask2D>{square}, NotifyObservers::No);

    // Window: center=10, ±5 → [5, 15]
    auto result = SpatialMapper::mapMaskContoursInWindow(masks, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.size() == 2);  // t=9 and t=15
}

TEST_CASE("mapMaskContoursInWindow - temporal distance correct", "[Mappers][SpatialMapper][Window]") {
    MaskData masks;
    auto tf = createLinearTimeFrame(20);
    masks.setTimeFrame(tf);

    auto square = createSquareMask();
    masks.addAtTime(TimeFrameIndex{8}, std::vector<Mask2D>{square}, NotifyObservers::No);
    masks.addAtTime(TimeFrameIndex{10}, std::vector<Mask2D>{square}, NotifyObservers::No);

    auto result = SpatialMapper::mapMaskContoursInWindow(masks, TimeFrameIndex{10}, 5, 5);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].temporal_distance == -2);
    REQUIRE(result[1].temporal_distance == 0);
}
