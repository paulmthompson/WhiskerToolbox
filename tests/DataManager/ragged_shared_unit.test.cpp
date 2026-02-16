/**
 * @file ragged_shared_unit.test.cpp
 * @brief Shared unit tests for all RaggedTimeSeries-derived types
 *
 * These tests exercise the common contract of RaggedTimeSeries<TData>:
 *   - add / get / clear at time
 *   - entity ID retrieval
 *   - observer notification
 *   - edge cases (empty data, non-existent times, add-clear-add)
 *   - getElementsInRange
 *
 * NO DataManager dependency — pure unit tests against the data object alone.
 * New data types (e.g. PointData) are added by:
 *   1. Adding a RaggedTestTraits specialization
 *   2. Adding the type to the TEMPLATE_TEST_CASE type list below
 */

#include "fixtures/RaggedTestTraits.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <memory>
#include <vector>

// ============================================================================
// Type list — add new RaggedTimeSeries-derived types here
// ============================================================================
TEMPLATE_TEST_CASE("RaggedTimeSeries - Add and retrieve data",
                   "[ragged][data][core][add][get]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    SECTION("Adding single element at a time") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1());

        auto result = data.getAtTime(TimeFrameIndex(0));
        REQUIRE(result.size() == 1);
    }

    SECTION("Adding multiple elements at same time") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1());
        Traits::add(data, TimeFrameIndex(0), Traits::sample2());

        auto result = data.getAtTime(TimeFrameIndex(0));
        REQUIRE(result.size() == 2);
    }

    SECTION("Adding elements at different times") {
        Traits::add(data, TimeFrameIndex(10), Traits::sample1());
        Traits::add(data, TimeFrameIndex(20), Traits::sample2());

        REQUIRE(data.getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(data.getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(data.getAtTime(TimeFrameIndex(15)).empty());
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - Clear data at time",
                   "[ragged][data][core][clear]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);
    data.setTimeFrame(timeframe);

    Traits::add(data, TimeFrameIndex(0), Traits::sample1());
    Traits::add(data, TimeFrameIndex(0), Traits::sample2());
    Traits::add(data, TimeFrameIndex(10), Traits::sample3());

    SECTION("Clear removes data at specified time only") {
        static_cast<void>(data.clearAtTime(TimeIndexAndFrame(0, timeframe.get()),
                                           NotifyObservers::No));

        REQUIRE(data.getAtTime(TimeFrameIndex(0)).empty());
        REQUIRE(data.getAtTime(TimeFrameIndex(10)).size() == 1);
    }

    SECTION("Clear non-existent time returns false") {
        REQUIRE_FALSE(data.clearAtTime(TimeIndexAndFrame(42, nullptr),
                                       NotifyObservers::No));
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - Entity ID retrieval",
                   "[ragged][data][entity][lookup]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    Traits::add(data, TimeFrameIndex(10), Traits::sample1());
    Traits::add(data, TimeFrameIndex(10), Traits::sample2());
    Traits::add(data, TimeFrameIndex(20), Traits::sample3());

    SECTION("Entity IDs at populated time") {
        auto ids = data.getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(ids.size() == 2);
    }

    SECTION("Entity IDs at empty time") {
        auto ids = data.getEntityIdsAtTime(TimeFrameIndex(99));
        REQUIRE(ids.empty());
    }

    SECTION("Entity lookup without registry returns nullopt") {
        EntityId fake_entity = EntityId(12345);
        auto result = data.getDataByEntityId(fake_entity);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Batch entity lookup without registry returns empty") {
        std::vector<EntityId> fake_entities = {EntityId(12345), EntityId(67890)};
        auto results = data.getDataByEntityIds(fake_entities);
        REQUIRE(std::ranges::distance(results) == 0);
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - Observer notification",
                   "[ragged][data][observer]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    int notification_count = 0;
    static_cast<void>(data.addObserver([&notification_count]() {
        notification_count++;
    }));

    SECTION("addAtTime with NotifyObservers::Yes triggers observer") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1(), NotifyObservers::Yes);
        REQUIRE(notification_count == 1);
    }

    SECTION("addAtTime with NotifyObservers::No does not trigger") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1(), NotifyObservers::No);
        REQUIRE(notification_count == 0);
    }

    SECTION("clearAtTime with NotifyObservers::Yes triggers observer") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1(), NotifyObservers::No);
        REQUIRE(notification_count == 0);

        REQUIRE(data.clearAtTime(TimeIndexAndFrame(0, nullptr), NotifyObservers::Yes));
        REQUIRE(notification_count == 1);
    }

    SECTION("Manual notifyObservers triggers observer once") {
        Traits::add(data, TimeFrameIndex(0), Traits::sample1(), NotifyObservers::No);
        Traits::add(data, TimeFrameIndex(1), Traits::sample2(), NotifyObservers::No);
        REQUIRE(notification_count == 0);

        data.notifyObservers();
        REQUIRE(notification_count == 1);
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - Edge cases",
                   "[ragged][data][edge]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    SECTION("Get at non-existent time returns empty") {
        REQUIRE(data.getAtTime(TimeFrameIndex(999)).empty());
    }

    SECTION("Add-clear-add preserves consistency") {
        Traits::add(data, TimeFrameIndex(5), Traits::sample1());

        REQUIRE(data.clearAtTime(TimeIndexAndFrame(5, nullptr), NotifyObservers::No));

        Traits::add(data, TimeFrameIndex(5), Traits::sample2());

        auto result = data.getAtTime(TimeFrameIndex(5));
        REQUIRE(result.size() == 1);
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - getElementsInRange",
                   "[ragged][data][range]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);
    data.setTimeFrame(timeframe);

    // Add data at multiple time points
    Traits::add(data, TimeFrameIndex(5), Traits::sample1());
    Traits::add(data, TimeFrameIndex(10), Traits::sample1());
    Traits::add(data, TimeFrameIndex(10), Traits::sample2()); // 2nd at same time
    Traits::add(data, TimeFrameIndex(15), Traits::sample3());
    Traits::add(data, TimeFrameIndex(20), Traits::sample1());
    Traits::add(data, TimeFrameIndex(25), Traits::sample2());

    SECTION("Range includes some data") {
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval)) {
            (void)entity_id;
            (void)data_ref;
            REQUIRE(time.getValue() >= 10);
            REQUIRE(time.getValue() <= 20);
            count++;
        }
        REQUIRE(count == 4); // 2 at 10, 1 at 15, 1 at 20
    }

    SECTION("Range includes all data") {
        TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval)) {
            (void)time; (void)entity_id; (void)data_ref;
            count++;
        }
        REQUIRE(count == 6);
    }

    SECTION("Range includes no data") {
        TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval)) {
            (void)time; (void)entity_id; (void)data_ref;
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Range with single time point") {
        TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval)) {
            (void)entity_id; (void)data_ref;
            REQUIRE(time.getValue() == 15);
            count++;
        }
        REQUIRE(count == 1);
    }

    SECTION("Range with start > end returns empty") {
        TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval)) {
            (void)time; (void)entity_id; (void)data_ref;
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Range with same source/target timeframe") {
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
        size_t count = 0;
        for (auto [time, entity_id, data_ref] : data.getElementsInRange(interval, *timeframe)) {
            (void)entity_id; (void)data_ref;
            REQUIRE(time.getValue() >= 10);
            REQUIRE(time.getValue() <= 20);
            count++;
        }
        REQUIRE(count == 4);
    }
}

TEMPLATE_TEST_CASE("RaggedTimeSeries - getElementsInRange with timeframe conversion",
                   "[ragged][data][range][timeframe]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    // Source timeframe (video frames)
    std::vector<int> video_times = {0, 10, 20, 30, 40};
    auto video_timeframe = std::make_shared<TimeFrame>(video_times);

    // Target timeframe (data sampling — 2× resolution)
    std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40};
    auto data_timeframe = std::make_shared<TimeFrame>(data_times);

    data.setTimeFrame(data_timeframe);

    // Data at data-timeframe indices 2 (time=10), 3 (time=15), 4 (time=20)
    Traits::add(data, TimeFrameIndex(2), Traits::sample1());
    Traits::add(data, TimeFrameIndex(3), Traits::sample2());
    Traits::add(data, TimeFrameIndex(4), Traits::sample3());

    // Query video frames 1–2 (times 10–20) → should map to data indices 2–4
    TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
    size_t count = 0;
    for (auto [time, entity_id, data_ref] : data.getElementsInRange(video_interval, *video_timeframe)) {
        (void)entity_id; (void)data_ref;
        REQUIRE(time.getValue() >= 2);
        REQUIRE(time.getValue() <= 4);
        count++;
    }
    REQUIRE(count == 3);
}
