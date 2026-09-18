/**
 * @file LineDrawOperations.test.cpp
 * @brief Unit tests for Media Viewer line draw commit helpers
 */

#include "Core/LineDrawOperations.hpp"

#include "CoreGeometry/lines.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace {

std::shared_ptr<TimeFrame> makeTimeFrame(int num_frames) {
    std::vector<int> times(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

LineData makeLineData(int num_frames, EntityRegistry & registry) {
    LineData line_data;
    line_data.setTimeFrame(makeTimeFrame(num_frames));
    line_data.setIdentityContext("test_lines", &registry);
    return line_data;
}

}// namespace

TEST_CASE("commitNewLineAtTime creates entity on empty frame", "[LineDrawOperations]") {
    EntityRegistry registry;
    LineData line_data = makeLineData(100, registry);

    Line2D const line({Point2D<float>{10.0f, 20.0f}});
    auto const entity_id =
            commitNewLineAtTime(line_data, TimeFrameIndex{5}, line, NotifyObservers::No);
    REQUIRE(entity_id.has_value());
    REQUIRE(line_data.getTotalEntryCount() == 1);
    REQUIRE(entityExistsAtTime(line_data, entity_id.value(), TimeFrameIndex{5}));
}

TEST_CASE("commitNewLineAtTime supports multiple lines on same frame", "[LineDrawOperations]") {
    EntityRegistry registry;
    LineData line_data = makeLineData(100, registry);

    auto const first_id =
            commitNewLineAtTime(line_data,
                                TimeFrameIndex{7},
                                Line2D({Point2D<float>{1.0f, 2.0f}}),
                                NotifyObservers::No);
    auto const second_id =
            commitNewLineAtTime(line_data,
                                TimeFrameIndex{7},
                                Line2D({Point2D<float>{3.0f, 4.0f}}),
                                NotifyObservers::No);

    REQUIRE(first_id.has_value());
    REQUIRE(second_id.has_value());
    REQUIRE(first_id.value() != second_id.value());
    REQUIRE(line_data.getTotalEntryCount() == 2);
}

TEST_CASE("entityExistsAtTime matches entity frame", "[LineDrawOperations]") {
    EntityRegistry registry;
    LineData line_data = makeLineData(100, registry);

    auto const entity_id =
            commitNewLineAtTime(line_data,
                                TimeFrameIndex{3},
                                Line2D({Point2D<float>{0.0f, 0.0f}}),
                                NotifyObservers::No);
    REQUIRE(entity_id.has_value());
    REQUIRE(entityExistsAtTime(line_data, entity_id.value(), TimeFrameIndex{3}));
    REQUIRE_FALSE(entityExistsAtTime(line_data, entity_id.value(), TimeFrameIndex{4}));
}
