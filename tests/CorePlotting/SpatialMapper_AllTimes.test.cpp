#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Mappers/SpatialMapper_AllTimes.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

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

} // namespace

// ============================================================================
// SpatialMapper::mapAllPoints Tests
// ============================================================================

TEST_CASE("SpatialMapper::mapAllPoints - empty data", "[Mappers][SpatialMapper][AllTimes]") {
    PointData points;
    auto tf = createLinearTimeFrame(10);
    points.setTimeFrame(tf);

    auto result = SpatialMapper::mapAllPoints(points);
    REQUIRE(result.empty());
}

TEST_CASE("SpatialMapper::mapAllPoints - single time frame", "[Mappers][SpatialMapper][AllTimes]") {
    PointData points;
    auto tf = createLinearTimeFrame(10);
    points.setTimeFrame(tf);

    std::vector<Point2D<float>> pts = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    points.addAtTime(TimeFrameIndex{0}, pts, NotifyObservers::No);

    auto result = SpatialMapper::mapAllPoints(points);
    REQUIRE(result.size() == 2);
    REQUIRE_THAT(result[0].x, WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(result[0].y, WithinAbs(2.0, 1e-5));
    REQUIRE_THAT(result[1].x, WithinAbs(3.0, 1e-5));
    REQUIRE_THAT(result[1].y, WithinAbs(4.0, 1e-5));
}

TEST_CASE("SpatialMapper::mapAllPoints - multiple time frames", "[Mappers][SpatialMapper][AllTimes]") {
    PointData points;
    auto tf = createLinearTimeFrame(10);
    points.setTimeFrame(tf);

    std::vector<Point2D<float>> pts_t0 = {{1.0f, 2.0f}};
    std::vector<Point2D<float>> pts_t3 = {{5.0f, 6.0f}, {7.0f, 8.0f}};
    std::vector<Point2D<float>> pts_t7 = {{9.0f, 10.0f}};

    points.addAtTime(TimeFrameIndex{0}, pts_t0, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{3}, pts_t3, NotifyObservers::No);
    points.addAtTime(TimeFrameIndex{7}, pts_t7, NotifyObservers::No);

    auto result = SpatialMapper::mapAllPoints(points);
    REQUIRE(result.size() == 4);

    // All points from all times should be present
    // Order: t0 points, then t3 points, then t7 points
    REQUIRE_THAT(result[0].x, WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(result[1].x, WithinAbs(5.0, 1e-5));
    REQUIRE_THAT(result[2].x, WithinAbs(7.0, 1e-5));
    REQUIRE_THAT(result[3].x, WithinAbs(9.0, 1e-5));
}

TEST_CASE("SpatialMapper::mapAllPoints - with scaling and offset", "[Mappers][SpatialMapper][AllTimes]") {
    PointData points;
    auto tf = createLinearTimeFrame(10);
    points.setTimeFrame(tf);

    std::vector<Point2D<float>> pts = {{10.0f, 20.0f}};
    points.addAtTime(TimeFrameIndex{0}, pts, NotifyObservers::No);

    auto result = SpatialMapper::mapAllPoints(points, 2.0f, 0.5f, 5.0f, -3.0f);
    REQUIRE(result.size() == 1);
    REQUIRE_THAT(result[0].x, WithinAbs(25.0, 1e-5));  // 10 * 2 + 5
    REQUIRE_THAT(result[0].y, WithinAbs(7.0, 1e-5));   // 20 * 0.5 + (-3)
}

// ============================================================================
// SpatialMapper::mapAllLines Tests
// ============================================================================

TEST_CASE("SpatialMapper::mapAllLines - empty data", "[Mappers][SpatialMapper][AllTimes]") {
    LineData lines;
    auto tf = createLinearTimeFrame(10);
    lines.setTimeFrame(tf);

    auto result = SpatialMapper::mapAllLines(lines);
    REQUIRE(result.empty());
}

TEST_CASE("SpatialMapper::mapAllLines - single time frame", "[Mappers][SpatialMapper][AllTimes]") {
    LineData lines;
    auto tf = createLinearTimeFrame(10);
    lines.setTimeFrame(tf);

    Line2D line;
    line.push_back(Point2D<float>{0.0f, 0.0f});
    line.push_back(Point2D<float>{10.0f, 20.0f});

    std::vector<Line2D> line_vec = {line};
    lines.addAtTime(TimeFrameIndex{0}, line_vec, NotifyObservers::No);

    auto result = SpatialMapper::mapAllLines(lines);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].vertices().size() == 2);
    REQUIRE_THAT(result[0].vertices()[0].x, WithinAbs(0.0, 1e-5));
    REQUIRE_THAT(result[0].vertices()[1].x, WithinAbs(10.0, 1e-5));
    REQUIRE_THAT(result[0].vertices()[1].y, WithinAbs(20.0, 1e-5));
}

TEST_CASE("SpatialMapper::mapAllLines - multiple time frames", "[Mappers][SpatialMapper][AllTimes]") {
    LineData lines;
    auto tf = createLinearTimeFrame(10);
    lines.setTimeFrame(tf);

    Line2D line1;
    line1.push_back(Point2D<float>{1.0f, 2.0f});
    line1.push_back(Point2D<float>{3.0f, 4.0f});

    Line2D line2;
    line2.push_back(Point2D<float>{5.0f, 6.0f});
    line2.push_back(Point2D<float>{7.0f, 8.0f});

    Line2D line3;
    line3.push_back(Point2D<float>{9.0f, 10.0f});

    std::vector<Line2D> lines_t0 = {line1};
    std::vector<Line2D> lines_t5 = {line2, line3};

    lines.addAtTime(TimeFrameIndex{0}, lines_t0, NotifyObservers::No);
    lines.addAtTime(TimeFrameIndex{5}, lines_t5, NotifyObservers::No);

    auto result = SpatialMapper::mapAllLines(lines);
    REQUIRE(result.size() == 3);  // 1 from t0, 2 from t5

    // First line from t0
    REQUIRE(result[0].vertices().size() == 2);
    REQUIRE_THAT(result[0].vertices()[0].x, WithinAbs(1.0, 1e-5));

    // Second line from t5
    REQUIRE(result[1].vertices().size() == 2);
    REQUIRE_THAT(result[1].vertices()[0].x, WithinAbs(5.0, 1e-5));

    // Third line from t5
    REQUIRE(result[2].vertices().size() == 1);
    REQUIRE_THAT(result[2].vertices()[0].x, WithinAbs(9.0, 1e-5));
}

TEST_CASE("SpatialMapper::mapAllLines - with scaling", "[Mappers][SpatialMapper][AllTimes]") {
    LineData lines;
    auto tf = createLinearTimeFrame(10);
    lines.setTimeFrame(tf);

    Line2D line;
    line.push_back(Point2D<float>{10.0f, 20.0f});

    std::vector<Line2D> line_vec = {line};
    lines.addAtTime(TimeFrameIndex{0}, line_vec, NotifyObservers::No);

    auto result = SpatialMapper::mapAllLines(lines, 2.0f, 0.5f, 10.0f, 5.0f);
    REQUIRE(result.size() == 1);
    REQUIRE_THAT(result[0].vertices()[0].x, WithinAbs(30.0, 1e-5));  // 10*2 + 10
    REQUIRE_THAT(result[0].vertices()[0].y, WithinAbs(15.0, 1e-5));  // 20*0.5 + 5
}
