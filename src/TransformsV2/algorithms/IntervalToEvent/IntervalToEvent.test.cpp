#include "IntervalToEvent.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"
#include "algorithms/EventToInterval/EventToInterval.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

std::shared_ptr<DigitalIntervalSeries> makeIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<Interval> interval_data;
    interval_data.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        interval_data.push_back(Interval{start, end});
    }
    return DigitalIntervalSeries::createOverlapping(std::move(interval_data));
}

}// namespace

TEST_CASE("V2 Container Transform: Interval To Event - Algorithm",
          "[transforms][v2][container][interval_to_event]") {
    ComputeContext const ctx;

    SECTION("Empty input returns empty event series") {
        DigitalIntervalSeries const empty;
        auto const result = intervalToEvent(
                empty,
                IntervalToEventParams{.point = IntervalEventPoint::start},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
    }

    SECTION("Single interval extracts start, end, and center") {
        auto intervals = makeIntervalSeries({{100, 200}});

        auto const start_result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::start},
                ctx);
        REQUIRE(start_result->size() == 1);
        REQUIRE(start_result->view()[0].time().getValue() == 100);

        auto const end_result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::end},
                ctx);
        REQUIRE(end_result->size() == 1);
        REQUIRE(end_result->view()[0].time().getValue() == 200);

        auto const center_result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::center},
                ctx);
        REQUIRE(center_result->size() == 1);
        REQUIRE(center_result->view()[0].time().getValue() == 150);
    }

    SECTION("Multiple intervals preserve order and count") {
        auto intervals = makeIntervalSeries({{0, 100}, {150, 250}, {300, 500}});
        auto const result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::start},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 3);
        REQUIRE(result->view()[0].time().getValue() == 0);
        REQUIRE(result->view()[1].time().getValue() == 150);
        REQUIRE(result->view()[2].time().getValue() == 300);
    }

    SECTION("Odd-length interval uses integer division for center") {
        auto intervals = makeIntervalSeries({{0, 101}});
        auto const result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::center},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 1);
        REQUIRE(result->view()[0].time().getValue() == 50);
    }

    SECTION("TimeFrame is preserved from input") {
        std::vector<int> const times{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
        auto time_frame = std::make_shared<TimeFrame>(times);

        auto intervals = makeIntervalSeries({{10, 20}});
        intervals->setTimeFrame(time_frame);

        auto const result = intervalToEvent(
                *intervals,
                IntervalToEventParams{.point = IntervalEventPoint::start},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == time_frame);
    }

    SECTION("EventToInterval round-trip with zero expansion") {
        std::vector<TimeFrameIndex> event_times;
        event_times.emplace_back(100);
        event_times.emplace_back(200);
        auto events = std::make_shared<DigitalEventSeries>(std::move(event_times));

        auto const expanded = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{0},
                        .post_expansion = TimeFrameIndex{0}},
                ctx);

        auto const round_trip = intervalToEvent(
                *expanded,
                IntervalToEventParams{.point = IntervalEventPoint::start},
                ctx);

        REQUIRE(round_trip->size() == 2);
        REQUIRE(round_trip->view()[0].time().getValue() == 100);
        REQUIRE(round_trip->view()[1].time().getValue() == 200);
    }
}

TEST_CASE("V2 Container Transform: Interval To Event - Registry",
          "[transforms][v2][container][interval_to_event]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    REQUIRE(registry.hasTransform("IntervalToEvent"));

    auto intervals = makeIntervalSeries({{0, 100}, {150, 250}});
    IntervalToEventParams params;
    params.point = IntervalEventPoint::end;

    auto const result = registry.executeContainerTransform<
            DigitalIntervalSeries,
            DigitalEventSeries,
            IntervalToEventParams>(
            "IntervalToEvent",
            *intervals,
            params,
            ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 2);
    REQUIRE(result->view()[0].time().getValue() == 100);
    REQUIRE(result->view()[1].time().getValue() == 250);
}

TEST_CASE("V2 Container Transform: Interval To Event - JSON Parameters",
          "[transforms][v2][container][interval_to_event]") {
    SECTION("Load end point from JSON") {
        auto const result = loadParametersFromJson<IntervalToEventParams>(R"({"point": "end"})");

        REQUIRE(result);
        auto const params = result.value();
        REQUIRE(params.point == IntervalEventPoint::end);
    }

    SECTION("Reject unknown point value") {
        std::string const json = R"({"point": "middle"})";
        REQUIRE_FALSE(loadParametersFromJson<IntervalToEventParams>(json));
    }

    SECTION("Reject all-caps point name") {
        std::string const json = R"({"point": "START"})";
        REQUIRE_FALSE(loadParametersFromJson<IntervalToEventParams>(json));
    }
}
