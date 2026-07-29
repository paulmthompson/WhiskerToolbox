#include "EventToInterval.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "fixtures/UniformIntervalTestTimeFrame.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndexReflector.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

std::shared_ptr<DigitalEventSeries> makeEventSeries(std::vector<int64_t> const & event_times) {
    std::vector<TimeFrameIndex> events;
    events.reserve(event_times.size());
    int64_t max_time = 0;
    for (int64_t const time: event_times) {
        events.emplace_back(time);
        max_time = std::max(max_time, time);
    }
    auto series = std::make_shared<DigitalEventSeries>(std::move(events));
    series->setTimeFrame(uniform_interval_test::uniformIntervalTestTimeFrame());
    return series;
}

}// namespace

TEST_CASE("V2 Container Transform: Event To Interval - Algorithm",
          "[transforms][v2][container][event_to_interval]") {
    ComputeContext const ctx;

    SECTION("Empty input returns empty overlapping series") {
        DigitalEventSeries const empty;
        auto const result = eventToInterval(
                empty,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{5},
                        .post_expansion = TimeFrameIndex{5}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
        REQUIRE(result->layout() == IntervalLayout::Overlapping);
    }

    SECTION("Single event with symmetric window") {
        auto events = makeEventSeries({100});
        auto const result = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{5},
                        .post_expansion = TimeFrameIndex{5}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 1);
        auto const interval = result->view()[0].value();
        REQUIRE(interval.start == ClockTicks(95));
        REQUIRE(interval.end == ClockTicks(105));
        REQUIRE(result->layout() == IntervalLayout::Overlapping);
    }

    SECTION("Zero expansion produces point interval") {
        auto events = makeEventSeries({50});
        auto const result = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{0},
                        .post_expansion = TimeFrameIndex{0}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 1);
        auto const interval = result->view()[0].value();
        REQUIRE(interval.start == ClockTicks(50));
        REQUIRE(interval.end == ClockTicks(50));
    }

    SECTION("Overlapping windows are preserved") {
        auto events = makeEventSeries({100, 102});
        auto const result = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{5},
                        .post_expansion = TimeFrameIndex{5}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 2);
        REQUIRE(result->layout() == IntervalLayout::Overlapping);

        auto const first = result->view()[0].value();
        auto const second = result->view()[1].value();
        REQUIRE(first.start == ClockTicks(95));
        REQUIRE(first.end == ClockTicks(105));
        REQUIRE(second.start == ClockTicks(97));
        REQUIRE(second.end == ClockTicks(107));
    }

    SECTION("Asymmetric pre/post expansion") {
        auto events = makeEventSeries({200});
        auto const result = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{10},
                        .post_expansion = TimeFrameIndex{30}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 1);
        auto const interval = result->view()[0].value();
        REQUIRE(interval.start == ClockTicks(190));
        REQUIRE(interval.end == ClockTicks(230));
    }

    SECTION("TimeFrame is preserved from input") {
        std::vector<int> const times{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
        auto time_frame = std::make_shared<TimeFrame>(times);

        auto events = makeEventSeries({3});
        events->setTimeFrame(time_frame);

        auto const result = eventToInterval(
                *events,
                EventToIntervalParams{
                        .pre_expansion = TimeFrameIndex{1},
                        .post_expansion = TimeFrameIndex{1}},
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == time_frame);
    }
}

TEST_CASE("V2 Container Transform: Event To Interval - Registry",
          "[transforms][v2][container][event_to_interval]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    REQUIRE(registry.hasTransform("EventToInterval"));

    auto events = makeEventSeries({100, 200});
    EventToIntervalParams params;
    params.pre_expansion = TimeFrameIndex{10};
    params.post_expansion = TimeFrameIndex{20};

    auto const result = registry.executeContainerTransform<
            DigitalEventSeries,
            DigitalIntervalSeries,
            EventToIntervalParams>(
            "EventToInterval",
            *events,
            params,
            ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->size() == 2);
    REQUIRE(result->view()[0].value().start == ClockTicks(90));
    REQUIRE(result->view()[0].value().end == ClockTicks(120));
    REQUIRE(result->view()[1].value().start == ClockTicks(190));
    REQUIRE(result->view()[1].value().end == ClockTicks(220));
}

TEST_CASE("V2 Container Transform: Event To Interval - JSON Parameters",
          "[transforms][v2][container][event_to_interval]") {
    auto const result = loadParametersFromJson<EventToIntervalParams>(
            R"({"pre_expansion": 10, "post_expansion": 20})");

    REQUIRE(result);
    auto const params = result.value();
    REQUIRE(params.pre_expansion.getValue() == 10);
    REQUIRE(params.post_expansion.getValue() == 20);
}
