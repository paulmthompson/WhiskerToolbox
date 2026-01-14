/**
 * @file GatherResult.test.cpp
 * @brief Tests for GatherResult template and gather() function
 */

#include "utils/GatherResult.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <vector>

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto t : times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Create a DigitalIntervalSeries with specified intervals
 */
std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<Interval> interval_vec;
    interval_vec.reserve(intervals.size());
    for (auto const & [start, end] : intervals) {
        interval_vec.push_back(Interval{start, end});
    }
    return std::make_shared<DigitalIntervalSeries>(interval_vec);
}

/**
 * @brief Create an AnalogTimeSeries with linear data
 */
std::shared_ptr<AnalogTimeSeries> createAnalogSeries(size_t num_samples) {
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

}// namespace

// =============================================================================
// Basic GatherResult Tests
// =============================================================================

TEST_CASE("GatherResult - Default construction", "[GatherResult]") {
    GatherResult<DigitalEventSeries> result;

    CHECK(result.empty());
    CHECK(result.size() == 0);
    CHECK(result.source() == nullptr);
    CHECK(result.intervals() == nullptr);
}

TEST_CASE("GatherResult - Gather DigitalEventSeries by intervals", "[GatherResult]") {
    // Create event series with events at times: 5, 15, 25, 35, 45, 55
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});

    // Create alignment intervals: [0, 20], [30, 50]
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});

    // Gather
    auto result = gather(events, intervals);

    REQUIRE(result.size() == 2);
    REQUIRE(result.source() == events);
    REQUIRE(result.intervals() == intervals);

    SECTION("First view contains events in [0, 20]") {
        auto const & view = result[0];
        REQUIRE(view != nullptr);

        // Should contain events at times 5 and 15
        size_t count = 0;
        for (auto const & event : view->view()) {
            CHECK((event.time().getValue() == 5 || event.time().getValue() == 15));
            ++count;
        }
        CHECK(count == 2);
    }

    SECTION("Second view contains events in [30, 50]") {
        auto const & view = result[1];
        REQUIRE(view != nullptr);

        // Should contain events at times 35 and 45
        size_t count = 0;
        for (auto const & event : view->view()) {
            CHECK((event.time().getValue() == 35 || event.time().getValue() == 45));
            ++count;
        }
        CHECK(count == 2);
    }
}

TEST_CASE("GatherResult - Range-based for loop", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30, 40, 50});
    auto intervals = createIntervalSeries({{5, 25}, {35, 55}});

    auto result = gather(events, intervals);

    size_t count = 0;
    for (auto const & view : result) {
        REQUIRE(view != nullptr);
        ++count;
    }
    CHECK(count == 2);
}

TEST_CASE("GatherResult - transform() applies function to all views", "[GatherResult]") {
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}, {60, 80}});

    auto result = gather(events, intervals);

    // Count events in each view
    auto counts = result.transform([](auto const & view) {
        return view->size();
    });

    REQUIRE(counts.size() == 3);
    CHECK(counts[0] == 2);// Events 5, 15
    CHECK(counts[1] == 2);// Events 35, 45
    CHECK(counts[2] == 0);// No events in [60, 80]
}

TEST_CASE("GatherResult - transformWithInterval() provides interval access", "[GatherResult]") {
    auto events = createEventSeries({5, 15, 35, 45});
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});

    auto result = gather(events, intervals);

    auto results = result.transformWithInterval(
            [](auto const & view, Interval const & interval) {
                return std::make_pair(view->size(), interval.end - interval.start);
            });

    REQUIRE(results.size() == 2);
    CHECK(results[0].first == 2); // 2 events
    CHECK(results[0].second == 20);// interval length
    CHECK(results[1].first == 2); // 2 events
    CHECK(results[1].second == 20);// interval length
}

TEST_CASE("GatherResult - intervalAt() returns correct intervals", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{0, 15}, {25, 35}});

    auto result = gather(events, intervals);

    CHECK(result.intervalAt(0).start == 0);
    CHECK(result.intervalAt(0).end == 15);
    CHECK(result.intervalAt(1).start == 25);
    CHECK(result.intervalAt(1).end == 35);

    CHECK_THROWS_AS(result.intervalAt(2), std::out_of_range);
}

TEST_CASE("GatherResult - Gather AnalogTimeSeries by intervals", "[GatherResult]") {
    // Create analog series: values 0.0 to 99.0 at times 0 to 99
    auto analog = createAnalogSeries(100);

    // Create alignment intervals
    auto intervals = createIntervalSeries({{10, 20}, {50, 60}});

    auto result = gather(analog, intervals);

    REQUIRE(result.size() == 2);

    SECTION("First view covers [10, 20]") {
        auto const & view = result[0];
        REQUIRE(view != nullptr);

        // Check that view has correct number of samples
        // and values are in expected range
        auto samples = view->getNumSamples();
        CHECK(samples >= 10);// At least 10 samples in range
    }

    SECTION("Second view covers [50, 60]") {
        auto const & view = result[1];
        REQUIRE(view != nullptr);
        auto samples = view->getNumSamples();
        CHECK(samples >= 10);
    }
}

TEST_CASE("GatherResult - Empty intervals produces empty result", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30});
    auto intervals = std::make_shared<DigitalIntervalSeries>();

    auto result = gather(events, intervals);

    CHECK(result.empty());
    CHECK(result.size() == 0);
}

TEST_CASE("GatherResult - views() returns range-compatible view", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30, 40});
    auto intervals = createIntervalSeries({{5, 25}, {25, 45}});

    auto result = gather(events, intervals);

    // Use views() with algorithm
    size_t total_events = 0;
    for (auto const & view : result.views()) {
        total_events += view->size();
    }
    // [5, 25] contains 10, 20 (2 events); [25, 45] contains 30, 40 (2 events)
    CHECK(total_events == 4);
}

TEST_CASE("GatherResult - front() and back() accessors", "[GatherResult]") {
    auto events = createEventSeries({10, 50, 90});
    auto intervals = createIntervalSeries({{0, 20}, {40, 60}, {80, 100}});

    auto result = gather(events, intervals);

    REQUIRE(result.size() == 3);
    CHECK(result.front() == result[0]);
    CHECK(result.back() == result[2]);
}

TEST_CASE("GatherResult - at() with bounds checking", "[GatherResult]") {
    auto events = createEventSeries({10, 20});
    auto intervals = createIntervalSeries({{5, 15}, {15, 25}});

    auto result = gather(events, intervals);

    CHECK_NOTHROW(result.at(0));
    CHECK_NOTHROW(result.at(1));
    CHECK_THROWS_AS(result.at(2), std::out_of_range);
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_CASE("GatherResult - Raster plot simulation", "[GatherResult][integration]") {
    // Simulate spike train: events at regular intervals
    std::vector<int64_t> spike_times;
    for (int64_t t = 0; t < 1000; t += 7) {// Spike every 7 time units
        spike_times.push_back(t);
    }
    auto spikes = createEventSeries(spike_times);

    // Simulate trial intervals: 10 trials of 50 time units each
    std::vector<std::pair<int64_t, int64_t>> trial_intervals;
    for (int64_t i = 0; i < 10; ++i) {
        int64_t start = i * 100;
        trial_intervals.push_back({start, start + 50});
    }
    auto trials = createIntervalSeries(trial_intervals);

    // Create raster
    auto raster = gather(spikes, trials);

    REQUIRE(raster.size() == 10);

    // Each trial should have approximately same number of spikes
    auto spike_counts = raster.transform([](auto const & trial) {
        return trial->size();
    });

    // Check that all trials have similar spike counts (50/7 ≈ 7 spikes per trial)
    for (auto count : spike_counts) {
        CHECK(count >= 5);
        CHECK(count <= 9);
    }

    // Total spikes in raster should be subset of original
    size_t total_in_raster = 0;
    for (auto c : spike_counts) {
        total_in_raster += c;
    }
    CHECK(total_in_raster <= spikes->size());
}

TEST_CASE("GatherResult - Convenience overloads accept non-const shared_ptr", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{5, 15}, {25, 35}});

    // Non-const source
    auto result1 = gather(events, intervals);
    CHECK(result1.size() == 2);

    // Const cast version
    std::shared_ptr<DigitalEventSeries const> const_events = events;
    auto result2 = gather(const_events, intervals);
    CHECK(result2.size() == 2);
}
