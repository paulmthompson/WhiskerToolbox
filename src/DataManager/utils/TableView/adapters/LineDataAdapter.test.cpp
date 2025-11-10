#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>
#include <vector>

TEST_CASE("DM - TV - LineDataAdapter getLinesInRange with identical start/end index", "[LineDataAdapter][Lines][Range][SingleIndex]") {
    // Arrange: timeframe and line data with a single time populated
    auto times = std::vector<int>{1,2,3,4,5,6};
    auto timeFrame = std::make_shared<TimeFrame>(times);

    auto lineData = std::make_shared<LineData>();
    std::vector<Point2D<float>> linePoints = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f}
    };
    TimeFrameIndex const t_present{3};
    TimeFrameIndex const t_absent{2};

    lineData->addAtTime(t_present, Line2D(linePoints), NotifyObservers::No);
    lineData->setTimeFrame(timeFrame);

    auto adapter = std::make_shared<LineDataAdapter>(lineData, timeFrame, "TestLines");

    SECTION("Single time present returns exactly that line and does not hang") {
        auto lines = adapter->getLinesInRange(t_present, t_present, timeFrame.get());
        REQUIRE(lines.size() == 1);
        REQUIRE(lines.front().size() == linePoints.size());
        // Spot-check first and last points
        REQUIRE(lines.front().front().x == Catch::Approx(linePoints.front().x));
        REQUIRE(lines.front().front().y == Catch::Approx(linePoints.front().y));
        REQUIRE(lines.front().back().x == Catch::Approx(linePoints.back().x));
        REQUIRE(lines.front().back().y == Catch::Approx(linePoints.back().y));
    }

    SECTION("Single time absent returns empty and does not hang") {
        auto lines = adapter->getLinesInRange(t_absent, t_absent, timeFrame.get());
        REQUIRE(lines.empty());
    }
}


