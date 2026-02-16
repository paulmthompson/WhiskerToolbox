/**
 * @file Line_Data.test.cpp
 * @brief LineData-specific unit tests
 *
 * Shared RaggedTimeSeries behaviour (add/get/clear, entity IDs, observers,
 * range queries, copy/move) is covered by the templated tests in
 *   tests/DataManager/ragged_shared_unit.test.cpp
 *   tests/DataManager/ragged_entity_integration.test.cpp
 *
 * This file only tests behaviour that is unique to LineData / Line2D:
 *   - emplaceAtTime (x_vec, y_vec construction)
 *   - Line2D-specific data verification (ordered coordinate access)
 */

#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

TEST_CASE("LineData - emplaceAtTime with coordinate vectors",
          "[line][data][emplace]") {
    LineData line_data;

    std::vector<float> x = {1.0f, 2.0f, 3.0f};
    std::vector<float> y = {4.0f, 5.0f, 6.0f};

    line_data.emplaceAtTime(TimeFrameIndex(10), x, y);

    auto lines = line_data.getAtTime(TimeFrameIndex(10));
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].size() == 3);
    REQUIRE(lines[0][0].x == 1.0f);
    REQUIRE(lines[0][0].y == 4.0f);
    REQUIRE(lines[0][2].x == 3.0f);
    REQUIRE(lines[0][2].y == 6.0f);
}

TEST_CASE("LineData - Multiple emplaceAtTime at same time",
          "[line][data][emplace]") {
    LineData line_data;

    std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> y1 = {1.0f, 2.0f, 1.0f};
    std::vector<float> x2 = {5.0f, 6.0f, 7.0f};
    std::vector<float> y2 = {5.0f, 6.0f, 5.0f};

    line_data.emplaceAtTime(TimeFrameIndex(10), x1, y1);
    line_data.emplaceAtTime(TimeFrameIndex(10), x2, y2);

    auto lines = line_data.getAtTime(TimeFrameIndex(10));
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0][0].x == 1.0f);
    REQUIRE(lines[1][0].x == 5.0f);
}
