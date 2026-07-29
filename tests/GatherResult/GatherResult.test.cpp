/**
 * @file GatherResult.test.cpp
 * @brief Tests for GatherResult template and gather() function
 */

#include "GatherResult/GatherResult.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/GatherAlignmentFixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

using Neuralyzer::Test::GatherFixtures::createAlignmentEventsForIntervals;
using Neuralyzer::Test::GatherFixtures::createWindowsAroundEvents;
using Neuralyzer::Test::GatherFixtures::TestIntervalAlignmentPoint;

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/**
 * @brief Create an identity TimeFrame large enough for the supplied maximum time.
 */
std::shared_ptr<TimeFrame> createIdentityTimeFrameForMax(int64_t max_time) {
    auto const size = static_cast<int>(std::max<int64_t>(max_time + 10'000, 10'000));
    std::vector<int> times(static_cast<std::size_t>(size));
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    auto const max_time = times.empty() ? int64_t{0} : *std::max_element(times.begin(), times.end());
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    for (auto t: times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Create a DigitalIntervalSeries with specified intervals
 */
std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<TimeFrameInterval> interval_vec;
    interval_vec.reserve(intervals.size());
    int64_t max_time = 0;
    for (auto const & [start, end]: intervals) {
        interval_vec.push_back(TimeFrameInterval(TimeFrameIndex(start), TimeFrameIndex(end)));
        max_time = std::max(max_time, end);
    }
    auto series = std::make_shared<DigitalIntervalSeries>(interval_vec);
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    return series;
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
        times.emplace_back(static_cast<int64_t>(i));
    }
    auto series = std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
    series->setTimeFrame(createIdentityTimeFrameForMax(static_cast<int64_t>(num_samples)));
    return series;
}

}// namespace

// =============================================================================
// Basic GatherResult Tests
// =============================================================================

TEST_CASE("GatherResult - Default construction", "[GatherResult]") {
    GatherResult<DigitalEventSeries> result;

    CHECK(result.empty());
    CHECK(result.empty());
    CHECK(result.source() == nullptr);
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

    auto gather_intervals = result.intervals();

    REQUIRE(gather_intervals == std::vector<TimeFrameInterval>{{TimeFrameIndex(0), TimeFrameIndex(20)}, {TimeFrameIndex(30), TimeFrameIndex(50)}});

    SECTION("First view contains events in [0, 20]") {
        auto const & view = result[0];
        REQUIRE(view != nullptr);

        // Should contain events at times 5 and 15
        size_t count = 0;
        for (auto const & event: view->view()) {
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
        for (auto const & event: view->view()) {
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
    for (auto const & view: result) {
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
            [](auto const & view, TimeFrameInterval const & interval) {
                return std::make_pair(view->size(), interval.end.getValue() - interval.start.getValue());
            });

    REQUIRE(results.size() == 2);
    CHECK(results[0].first == 2);  // 2 events
    CHECK(results[0].second == 20);// interval length
    CHECK(results[1].first == 2);  // 2 events
    CHECK(results[1].second == 20);// interval length
}

TEST_CASE("GatherResult - intervalAt() returns correct intervals", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{0, 15}, {25, 35}});

    auto result = gather(events, intervals);

    CHECK(result.intervalAt(0).start == TimeFrameIndex(0));
    CHECK(result.intervalAt(0).end == TimeFrameIndex(15));
    CHECK(result.intervalAt(1).start == TimeFrameIndex(25));
    CHECK(result.intervalAt(1).end == TimeFrameIndex(35));

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
    intervals->setTimeFrame(createIdentityTimeFrameForMax(0));

    auto result = gather(events, intervals);

    CHECK(result.empty());
    CHECK(result.empty());
}

TEST_CASE("GatherResult - views() returns range-compatible view", "[GatherResult]") {
    auto events = createEventSeries({10, 20, 30, 40});
    auto intervals = createIntervalSeries({{5, 25}, {25, 45}});

    auto result = gather(events, intervals);

    // Use views() with algorithm
    size_t total_events = 0;
    for (auto const & view: result.views()) {
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

TEST_CASE("GatherResult - fromRows stores row DataObjects with optional metadata",
          "[GatherResult][fromRows][Phase5_5]") {
    std::vector<std::shared_ptr<DigitalEventSeries>> rows{
            createEventSeries({11, 12}),
            createEventSeries({31, 32})};
    auto windows = createIntervalSeries({{10, 20}, {30, 40}});
    auto alignment_events = createEventSeries({15, 35});

    auto result = GatherResult<DigitalEventSeries>::fromRows(rows, windows, alignment_events);

    REQUIRE(result.size() == 2);
    CHECK(result.source() == nullptr);
    CHECK(result[0] == rows[0]);
    CHECK(result[1] == rows[1]);
    CHECK(result.windows() == windows);
    CHECK(result.alignmentPoints() == alignment_events);
    CHECK(result.intervalAt(0) == TimeFrameInterval{TimeFrameIndex(10), TimeFrameIndex(20)});
    CHECK(result.intervalAt(1) == TimeFrameInterval{TimeFrameIndex(30), TimeFrameIndex(40)});
    CHECK(result.alignmentTimeAt(0) == ClockTicks(15));
    CHECK(result.alignmentTimeAt(1) == ClockTicks(35));
}

TEST_CASE("GatherResult - fromRows validates rows and metadata",
          "[GatherResult][fromRows][Phase5_5]") {
    std::vector<std::shared_ptr<DigitalEventSeries>> rows{
            createEventSeries({1}),
            createEventSeries({2})};

    SECTION("Null row pointer throws") {
        rows[1] = nullptr;
        CHECK_THROWS_AS(
                GatherResult<DigitalEventSeries>::fromRows(rows),
                std::invalid_argument);
    }

    SECTION("Window count mismatch throws") {
        auto windows = createIntervalSeries({{0, 10}});
        CHECK_THROWS_AS(
                GatherResult<DigitalEventSeries>::fromRows(rows, windows),
                std::invalid_argument);
    }

    SECTION("Alignment point count mismatch throws") {
        auto alignment_events = createEventSeries({5});
        CHECK_THROWS_AS(
                GatherResult<DigitalEventSeries>::fromRows(rows, nullptr, alignment_events),
                std::invalid_argument);
    }

    SECTION("Metadata-free rows reject interval and alignment access") {
        auto result = GatherResult<DigitalEventSeries>::fromRows(rows);
        REQUIRE(result.size() == 2);
        CHECK_THROWS_AS(result.intervalAt(0), std::out_of_range);
        CHECK_THROWS_AS(result.alignmentTimeAt(0), std::out_of_range);
    }
}

TEST_CASE("GatherResult - fromRowsLike preserves reordered parent metadata",
          "[GatherResult][fromRows][Phase5_5]") {
    auto spikes = createEventSeries({5, 15, 105, 115});
    auto alignment_events = createEventSeries({10, 110});
    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto parent = gather(spikes, windows, alignment_events).reorder({1, 0});

    std::vector<std::shared_ptr<DigitalEventSeries>> rows{
            createEventSeries({1001}),
            createEventSeries({2001})};
    auto child = GatherResult<DigitalEventSeries>::fromRowsLike(parent, rows);

    REQUIRE(child.size() == 2);
    CHECK(child.source() == nullptr);
    CHECK(child[0] == rows[0]);
    CHECK(child[1] == rows[1]);
    CHECK(child.windows() == windows);
    CHECK(child.alignmentPoints() == alignment_events);
    CHECK(child.isReordered());
    CHECK(child.originalIndex(0) == 1);
    CHECK(child.originalIndex(1) == 0);
    CHECK(child.intervalAtReordered(0) == parent.intervalAtReordered(0));
    CHECK(child.intervalAtReordered(1) == parent.intervalAtReordered(1));
    CHECK(child.alignmentTimeAt(0) == parent.alignmentTimeAt(0));
    CHECK(child.alignmentTimeAt(1) == parent.alignmentTimeAt(1));

    rows.push_back(createEventSeries({3001}));
    CHECK_THROWS_AS(
            GatherResult<DigitalEventSeries>::fromRowsLike(parent, rows),
            std::invalid_argument);
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
        int64_t const start = i * 100;
        trial_intervals.emplace_back(start, start + 50);
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
    for (auto count: spike_counts) {
        CHECK(count >= 5);
        CHECK(count <= 9);
    }

    // Total spikes in raster should be subset of original
    size_t total_in_raster = 0;
    for (auto c: spike_counts) {
        total_in_raster += c;
    }
    CHECK(total_in_raster <= spikes->size());
}

// =============================================================================
// Alignment Time Normalization Tests
// =============================================================================

TEST_CASE("GatherResult - Normalize spike times with event alignment", "[GatherResult][alignment]") {
    // Spikes at times 10, 15, 20
    auto spikes = createEventSeries({10, 15, 20});

    // Alignment event at time 15
    auto alignment_events = createEventSeries({15});

    // Create prepared windows around event 15: [0, 30], aligned at 15.
    auto windows = createWindowsAroundEvents(alignment_events, 15, 15);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0]->size() == 3);

    SECTION("Alignment time is stored correctly") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(15));
    }

    SECTION("Normalized spike times are relative to alignment") {
        ClockTicks const alignment_time = result.alignmentTimeAt(0);

        std::vector<int64_t> normalized_times;
        for (auto const & event: result[0]->view()) {
            int64_t const normalized = event.time().getValue() - alignment_time.getValue();
            normalized_times.push_back(normalized);
        }

        // Expected: 10-15=-5, 15-15=0, 20-15=5
        REQUIRE(normalized_times.size() == 3);
        CHECK(normalized_times[0] == -5);
        CHECK(normalized_times[1] == 0);
        CHECK(normalized_times[2] == 5);
    }
}

TEST_CASE("GatherResult - Normalize spike times with interval start alignment", "[GatherResult][alignment]") {
    // Spikes at times 10, 20, 30 in trial [0, 40] aligned to start (0)
    auto spikes = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{0, 40}});

    auto alignment_events = createAlignmentEventsForIntervals(
            intervals,
            TestIntervalAlignmentPoint::Start);
    auto result = gather(spikes, intervals, alignment_events);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0]->size() == 3);

    SECTION("Alignment time is interval start") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(0));
    }

    SECTION("Normalized spike times are relative to interval start") {
        ClockTicks const alignment_time = result.alignmentTimeAt(0);

        std::vector<int64_t> normalized_times;
        for (auto const & event: result[0]->view()) {
            normalized_times.push_back(event.time().getValue() - alignment_time.getValue());
        }

        // Aligned to 0: 10-0=10, 20-0=20, 30-0=30
        REQUIRE(normalized_times.size() == 3);
        CHECK(normalized_times[0] == 10);
        CHECK(normalized_times[1] == 20);
        CHECK(normalized_times[2] == 30);
    }
}

TEST_CASE("GatherResult - Normalize spike times with interval end alignment", "[GatherResult][alignment]") {
    // Spikes at times 10, 20, 30 in trial [0, 40] aligned to end (40)
    auto spikes = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{0, 40}});

    auto alignment_events = createAlignmentEventsForIntervals(
            intervals,
            TestIntervalAlignmentPoint::End);
    auto result = gather(spikes, intervals, alignment_events);

    REQUIRE(result.size() == 1);

    SECTION("Alignment time is interval end") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(40));
    }

    SECTION("Normalized spike times are relative to interval end") {
        ClockTicks const alignment_time = result.alignmentTimeAt(0);

        std::vector<int64_t> normalized_times;
        for (auto const & event: result[0]->view()) {
            normalized_times.push_back(event.time().getValue() - alignment_time.getValue());
        }

        // Aligned to 40: 10-40=-30, 20-40=-20, 30-40=-10
        REQUIRE(normalized_times.size() == 3);
        CHECK(normalized_times[0] == -30);
        CHECK(normalized_times[1] == -20);
        CHECK(normalized_times[2] == -10);
    }
}

TEST_CASE("GatherResult - Normalize spike times with interval center alignment", "[GatherResult][alignment]") {
    // Spikes at times 10, 20, 30 in trial [0, 40] aligned to center (20)
    auto spikes = createEventSeries({10, 20, 30});
    auto intervals = createIntervalSeries({{0, 40}});

    auto alignment_events = createAlignmentEventsForIntervals(
            intervals,
            TestIntervalAlignmentPoint::Center);
    auto result = gather(spikes, intervals, alignment_events);

    REQUIRE(result.size() == 1);

    SECTION("Alignment time is interval center") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(20));// (0 + 40) / 2
    }

    SECTION("Normalized spike times are relative to center") {
        ClockTicks const alignment_time = result.alignmentTimeAt(0);

        std::vector<int64_t> normalized_times;
        for (auto const & event: result[0]->view()) {
            normalized_times.push_back(event.time().getValue() - alignment_time.getValue());
        }

        // Aligned to 20: 10-20=-10, 20-20=0, 30-20=10
        REQUIRE(normalized_times.size() == 3);
        CHECK(normalized_times[0] == -10);
        CHECK(normalized_times[1] == 0);
        CHECK(normalized_times[2] == 10);
    }
}

TEST_CASE("GatherResult - Multiple trials with alignment normalization", "[GatherResult][alignment]") {
    // Spikes at various times across two trials
    auto spikes = createEventSeries({5, 10, 15,      // Trial 1: events around alignment at 10
                                     105, 110, 115});// Trial 2: events around alignment at 110

    // Two alignment events
    auto alignment_events = createEventSeries({10, 110});

    // Expand each event to a prepared ±10 window.
    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 2);

    SECTION("Each trial has correct alignment time") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(10));
        CHECK(result.alignmentTimeAt(1) == ClockTicks(110));
    }

    SECTION("Trial 1: normalized times") {
        ClockTicks const alignment = result.alignmentTimeAt(0);
        std::vector<int64_t> normalized;
        for (auto const & event: result[0]->view()) {
            normalized.push_back(event.time().getValue() - alignment.getValue());
        }
        // 5-10=-5, 10-10=0, 15-10=5
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -5);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 5);
    }

    SECTION("Trial 2: normalized times") {
        ClockTicks const alignment = result.alignmentTimeAt(1);
        std::vector<int64_t> normalized;
        for (auto const & event: result[1]->view()) {
            normalized.push_back(event.time().getValue() - alignment.getValue());
        }
        // 105-110=-5, 110-110=0, 115-110=5
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -5);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 5);
    }
}
// =============================================================================
// Cross-TimeFrame Alignment Tests
// =============================================================================

/**
 * @brief Create a TimeFrame that maps indices to another time base
 * 
 * For example, 500Hz events mapped to 30kHz time base:
 * - Index 0 → 0
 * - Index 1 → 60  (30000/500 = 60)
 * - Index 2 → 120
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::shared_ptr<TimeFrame> createTimeFrameForRate(int indices, int samples_per_index) {
    std::vector<int> times;
    times.reserve(indices);
    for (int i = 0; i < indices; ++i) {
        times.push_back(i * samples_per_index);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a simple identity TimeFrame (index == time)
 */
std::shared_ptr<TimeFrame> createIdentityTimeFrame(int size) {
    std::vector<int> times;
    times.reserve(size);
    for (int i = 0; i < size; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

TEST_CASE("GatherResult - Automatic cross-timeframe conversion (30kHz spikes, 500Hz events)", "[GatherResult][alignment][timeframe]") {
    // Scenario: Spikes recorded at 30kHz, alignment events recorded at 500Hz
    // The ratio is 30000/500 = 60, so event index 1 = spike index 60

    // Create TimeFrames
    // Spikes: identity timeframe (index == time, representing 30kHz samples)
    auto spike_timeframe = createIdentityTimeFrame(1000);// 1000 30kHz samples

    // Events: 500Hz timeframe, each index maps to 60 samples at 30kHz
    auto event_timeframe = createTimeFrameForRate(20, 60);// 20 events, 60 samples per event

    // Spikes at 30kHz time base: indices 54, 60, 66 (around index 60)
    auto spikes = createEventSeries({54, 60, 66});
    spikes->setTimeFrame(spike_timeframe);

    // Alignment events at 500Hz indices 1 (which maps to time 60 at 30kHz)
    auto alignment_events = createEventSeries({1});// Event at 500Hz index 1
    alignment_events->setTimeFrame(event_timeframe);

    SECTION("Verify TimeFrame mapping") {
        CHECK(event_timeframe->getTimeAtIndex(TimeFrameIndex(0)) == ClockTicks(0));
        CHECK(event_timeframe->getTimeAtIndex(TimeFrameIndex(1)) == ClockTicks(60));
        CHECK(event_timeframe->getTimeAtIndex(TimeFrameIndex(2)) == ClockTicks(120));
    }

    SECTION("Automatic alignment using cross-timeframe conversion") {
        // Expand: ±60 time units around alignment event
        // Event at 500Hz index 1 → absolute time 60
        // Window: ±60 time units → [0, 120] at 30kHz
        auto windows = createWindowsAroundEvents(alignment_events, 60, 60);
        auto result = gather(spikes, windows, alignment_events);

        REQUIRE(result.size() == 1);

        // Interval should be [0, 120] at 30kHz
        CHECK(result.intervalAt(0).start == TimeFrameIndex(0));
        CHECK(result.intervalAt(0).end == TimeFrameIndex(120));

        // Alignment time is absolute time (60)
        CHECK(result.alignmentTimeAt(0) == ClockTicks(60));

        // All spikes should be in range [0, 120]
        REQUIRE(result[0]->size() == 3);

        // Normalized spike times: 54-60=-6, 60-60=0, 66-60=6
        std::vector<int64_t> normalized;
        for (auto const & event: result[0]->view()) {
            normalized.push_back(event.time().getValue() - result.alignmentTimeAt(0).getValue());
        }

        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -6);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 6);
    }
}

TEST_CASE("GatherResult - Automatic cross-timeframe with multiple trials", "[GatherResult][alignment][timeframe]") {
    // Spikes at 30kHz: two clusters around alignment events at 500Hz indices 1 and 3
    // 500Hz index 1 → 30kHz index 60
    // 500Hz index 3 → 30kHz index 180

    auto spike_timeframe = createIdentityTimeFrame(500);
    auto event_timeframe = createTimeFrameForRate(10, 60);

    auto spikes = createEventSeries({
            50, 60, 70,  // Around alignment at 60
            170, 180, 190// Around alignment at 180
    });
    spikes->setTimeFrame(spike_timeframe);

    // Alignment events at 500Hz indices 1 and 3
    auto alignment_events = createEventSeries({1, 3});// Events in 500Hz indices
    alignment_events->setTimeFrame(event_timeframe);

    // Expand: ±60 time units around each alignment event
    auto windows = createWindowsAroundEvents(alignment_events, 60, 60);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 2);

    SECTION("Intervals are converted to 30kHz") {
        // Trial 1: alignment abs time = 60, window ±60 → [0, 120]
        CHECK(result.intervalAt(0).start == TimeFrameIndex(0));
        CHECK(result.intervalAt(0).end == TimeFrameIndex(120));

        // Trial 2: alignment abs time = 180, window ±60 → [120, 240]
        CHECK(result.intervalAt(1).start == TimeFrameIndex(120));
        CHECK(result.intervalAt(1).end == TimeFrameIndex(240));
    }

    SECTION("Alignment times are absolute time") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(60)); // 500Hz index 1 → abs time 60
        CHECK(result.alignmentTimeAt(1) == ClockTicks(180));// 500Hz index 3 → abs time 180
    }

    SECTION("Trial 1: normalized times") {
        std::vector<int64_t> normalized;
        for (auto const & event: result[0]->view()) {
            normalized.push_back(event.time().getValue() - result.alignmentTimeAt(0).getValue());
        }
        // 50-60=-10, 60-60=0, 70-60=10
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -10);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 10);
    }

    SECTION("Trial 2: normalized times") {
        std::vector<int64_t> normalized;
        for (auto const & event: result[1]->view()) {
            normalized.push_back(event.time().getValue() - result.alignmentTimeAt(1).getValue());
        }
        // 170-180=-10, 180-180=0, 190-180=10
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -10);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 10);
    }
}

TEST_CASE("GatherResult - Same TimeFrame does no conversion", "[GatherResult][alignment][timeframe]") {
    // When both source and alignment have the same TimeFrame, no conversion needed
    auto shared_timeframe = createIdentityTimeFrame(100);

    auto spikes = createEventSeries({10, 15, 20});
    spikes->setTimeFrame(shared_timeframe);

    auto alignment_events = createEventSeries({15});
    alignment_events->setTimeFrame(shared_timeframe);// Same TimeFrame!

    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 1);

    // Interval should be unchanged: [5, 25]
    CHECK(result.intervalAt(0).start == TimeFrameIndex(5));
    CHECK(result.intervalAt(0).end == TimeFrameIndex(25));

    // Alignment time unchanged
    CHECK(result.alignmentTimeAt(0) == ClockTicks(15));

    // All 3 spikes should be included
    REQUIRE(result[0]->size() == 3);
}

TEST_CASE("GatherResult - DigitalIntervalSeries path converts cross-timeframe windows",
          "[GatherResult][timeframe][migration][phase2]") {
    auto source_timeframe = createIdentityTimeFrame(1000);
    auto window_timeframe = createTimeFrameForRate(20, 60);

    auto spikes = createEventSeries({50, 60, 90, 120, 130});
    spikes->setTimeFrame(source_timeframe);

    auto intervals = createIntervalSeries({{1, 2}});
    intervals->setTimeFrame(window_timeframe);

    auto result = gather(spikes, intervals);

    REQUIRE(result.size() == 1);
    auto const query_interval = result.intervalAt(0);
    CHECK(query_interval.start == TimeFrameIndex(60));
    CHECK(query_interval.end == TimeFrameIndex(120));
    REQUIRE(result[0]->size() == 3);

    std::vector<int64_t> gathered_times;
    for (auto const & event: result[0]->view()) {
        gathered_times.push_back(event.time().getValue());
    }
    CHECK(gathered_times == std::vector<int64_t>{60, 90, 120});
}

TEST_CASE("GatherResult - DigitalIntervalSeries path rejects missing interval TimeFrame",
          "[GatherResult][timeframe][migration][phase2]") {
    auto spikes = createEventSeries({10, 20, 30});
    auto intervals = std::make_shared<DigitalIntervalSeries>(
            std::vector<TimeFrameInterval>{{TimeFrameIndex(0), TimeFrameIndex(20)}});

    CHECK_THROWS_AS(gather(spikes, intervals), std::invalid_argument);
}

TEST_CASE("GatherResult - Asymmetric window with automatic cross-timeframe", "[GatherResult][alignment][timeframe]") {
    // Scenario: Behavioral events (e.g., lick) at 500Hz, neural spikes at 30kHz
    // Want to see spikes 30ms before and 100ms after each lick
    // At 500Hz: 30ms = 15 samples, 100ms = 50 samples

    auto spike_timeframe = createIdentityTimeFrame(10000);
    auto event_timeframe = createTimeFrameForRate(200, 60);// 500Hz → 30kHz: factor of 60

    // Spikes clustered around what will be time 6000 (500Hz event at index 100)
    auto spikes = createEventSeries({5100, 5500, 6000, 6500, 8000, 8500});
    spikes->setTimeFrame(spike_timeframe);

    // Behavioral event at 500Hz index 100 → 30kHz time 6000
    auto alignment_events = createEventSeries({100});// 500Hz index
    alignment_events->setTimeFrame(event_timeframe);

    // Asymmetric window in time units: 900 before (30ms at 30kHz), 3000 after (100ms at 30kHz)
    auto windows = createWindowsAroundEvents(alignment_events, 900, 3000);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 1);
    CHECK(result.alignmentTimeAt(0) == ClockTicks(6000));

    // Window in 30kHz: [6000-900, 6000+3000] = [5100, 9000]
    CHECK(result.intervalAt(0).start == TimeFrameIndex(5100));
    CHECK(result.intervalAt(0).end == TimeFrameIndex(9000));

    // Spikes in range: 5100, 5500, 6000, 6500, 8000, 8500
    REQUIRE(result[0]->size() == 6);

    // Normalized times
    std::vector<int64_t> normalized;
    for (auto const & event: result[0]->view()) {
        normalized.push_back(event.time().getValue() - result.alignmentTimeAt(0).getValue());
    }

    // 5100-6000=-900 (start of pre-window)
    // 5500-6000=-500
    // 6000-6000=0 (alignment point)
    // 6500-6000=500
    // 8000-6000=2000
    // 8500-6000=2500
    REQUIRE(normalized.size() == 6);
    CHECK(normalized[0] == -900);
    CHECK(normalized[1] == -500);
    CHECK(normalized[2] == 0);
    CHECK(normalized[3] == 500);
    CHECK(normalized[4] == 2000);
    CHECK(normalized[5] == 2500);
}

// =============================================================================
// Negative Relative Time Tests (events before alignment)
// =============================================================================

/**
 * @brief Verify that events occurring before the alignment event produce
 * negative relative times when normalized, while the GatherResult itself
 * always stores non-negative absolute TimeFrameIndex values.
 *
 * Scenario: events at [10, 20, 30], alignment events at [11, 21, 31].
 * After gathering with a window of ±5, each trial should contain one event
 * whose absolute time is less than the alignment time, yielding a relative
 * offset of -1 when subtracting alignment_time.
 *
 * The GatherResult must NEVER store negative TimeFrameIndex values.
 * Negative offsets only arise when computing (event_time - alignment_time)
 * for display in the plotting buffer.
 */
TEST_CASE("GatherResult - Events before alignment produce negative relative times",
          "[GatherResult][alignment][negative]") {

    // Spikes slightly before each alignment event
    auto spikes = createEventSeries({10, 20, 30});

    // Alignment events are 1 unit after each spike
    auto alignment_events = createEventSeries({11, 21, 31});

    // Window of ±5 around each alignment event
    auto windows = createWindowsAroundEvents(alignment_events, 5, 5);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 3);

    SECTION("alignment times match the alignment events") {
        CHECK(result.alignmentTimeAt(0) == ClockTicks(11));
        CHECK(result.alignmentTimeAt(1) == ClockTicks(21));
        CHECK(result.alignmentTimeAt(2) == ClockTicks(31));
    }

    SECTION("intervals are correct windows around alignment events") {
        // [11-5, 11+5] = [6, 16]
        CHECK(result.intervalAt(0).start == TimeFrameIndex(6));
        CHECK(result.intervalAt(0).end == TimeFrameIndex(16));

        // [21-5, 21+5] = [16, 26]
        CHECK(result.intervalAt(1).start == TimeFrameIndex(16));
        CHECK(result.intervalAt(1).end == TimeFrameIndex(26));

        // [31-5, 31+5] = [26, 36]
        CHECK(result.intervalAt(2).start == TimeFrameIndex(26));
        CHECK(result.intervalAt(2).end == TimeFrameIndex(36));
    }

    SECTION("gathered events store absolute (non-negative) TimeFrameIndex values") {
        for (auto const & trial: result) {
            REQUIRE(trial->size() == 1);
            for (auto const & event: trial->view()) {
                // Absolute index must never be negative
                CHECK(event.time().getValue() >= 0);
            }
        }
    }

    SECTION("normalized times are negative (-1) for each trial") {
        for (size_t trial = 0; trial < result.size(); ++trial) {
            ClockTicks const alignment = result.alignmentTimeAt(trial);
            REQUIRE(result[trial]->size() == 1);

            for (auto const & event: result[trial]->view()) {
                int64_t const relative = event.time().getValue() - alignment.getValue();
                CHECK(relative == -1);
            }
        }
    }
}

TEST_CASE("GatherResult - Mixed pre-and-post alignment events",
          "[GatherResult][alignment][negative]") {

    // Events both before and after each alignment event
    // Spikes at: 8, 10, 12 (around alignment at 10)
    //           18, 20, 22 (around alignment at 20)
    auto spikes = createEventSeries({8, 10, 12, 18, 20, 22});
    auto alignment_events = createEventSeries({10, 20});

    auto windows = createWindowsAroundEvents(alignment_events, 5, 5);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 2);

    SECTION("Trial 0 has pre, on, and post alignment events") {
        REQUIRE(result[0]->size() == 3);
        ClockTicks const alignment = result.alignmentTimeAt(0);// 10

        std::vector<int64_t> normalized;
        for (auto const & event: result[0]->view()) {
            CHECK(event.time().getValue() >= 0);// absolute is non-negative
            normalized.push_back(event.time().getValue() - alignment.getValue());
        }

        // 8-10=-2, 10-10=0, 12-10=2
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -2);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 2);
    }

    SECTION("Trial 1 has pre, on, and post alignment events") {
        REQUIRE(result[1]->size() == 3);
        ClockTicks const alignment = result.alignmentTimeAt(1);// 20

        std::vector<int64_t> normalized;
        for (auto const & event: result[1]->view()) {
            CHECK(event.time().getValue() >= 0);// absolute is non-negative
            normalized.push_back(event.time().getValue() - alignment.getValue());
        }

        // 18-20=-2, 20-20=0, 22-20=2
        REQUIRE(normalized.size() == 3);
        CHECK(normalized[0] == -2);
        CHECK(normalized[1] == 0);
        CHECK(normalized[2] == 2);
    }
}

TEST_CASE("GatherResult - All events before alignment event",
          "[GatherResult][alignment][negative]") {

    // All spikes occur before the alignment event
    auto spikes = createEventSeries({95, 96, 97, 98, 99});
    auto alignment_events = createEventSeries({100});

    // Window: [100-10, 100+10] = [90, 110]
    auto windows = createWindowsAroundEvents(alignment_events, 10, 10);
    auto result = gather(spikes, windows, alignment_events);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0]->size() == 5);
    CHECK(result.alignmentTimeAt(0) == ClockTicks(100));

    SECTION("all normalized times are negative") {
        for (auto const & event: result[0]->view()) {
            int64_t const abs_time = event.time().getValue();
            CHECK(abs_time >= 0);// stored as non-negative

            int64_t const relative = abs_time - result.alignmentTimeAt(0).getValue();
            CHECK(relative < 0);// all events are before alignment
        }
    }

    SECTION("specific negative values are correct") {
        std::vector<int64_t> normalized;
        for (auto const & event: result[0]->view()) {
            normalized.push_back(event.time().getValue() - result.alignmentTimeAt(0).getValue());
        }
        // 95-100=-5, 96-100=-4, 97-100=-3, 98-100=-2, 99-100=-1
        REQUIRE(normalized.size() == 5);
        CHECK(normalized[0] == -5);
        CHECK(normalized[1] == -4);
        CHECK(normalized[2] == -3);
        CHECK(normalized[3] == -2);
        CHECK(normalized[4] == -1);
    }
}

// =============================================================================
// Reorder preserves alignment times
// =============================================================================

TEST_CASE("GatherResult reorder preserves alignment times",
          "[GatherResult][reorder][alignment]") {
    // Create events at 10, 20, 30 with a simple 0-39 TimeFrame (step=1)
    auto events = createEventSeries({10, 20, 30});
    std::vector<int> times(40);
    std::iota(times.begin(), times.end(), 0);
    auto tf = std::make_shared<TimeFrame>(times);
    events->setTimeFrame(tf);

    // Expand each event into a ±5 window → 3 trials
    auto windows = createWindowsAroundEvents(events, 5, 5);
    auto gathered = gather(events, windows, events);

    REQUIRE(gathered.size() == 3);

    // Record alignment times before reorder
    ClockTicks const align_0 = gathered.alignmentTimeAt(0);// event at t=10
    ClockTicks const align_1 = gathered.alignmentTimeAt(1);// event at t=20
    ClockTicks const align_2 = gathered.alignmentTimeAt(2);// event at t=30

    CHECK(align_0 == ClockTicks(10));
    CHECK(align_1 == ClockTicks(20));
    CHECK(align_2 == ClockTicks(30));

    SECTION("reverse order") {
        auto reordered = gathered.reorder({2, 1, 0});

        // After reorder [2,1,0]: position 0 should have trial 2's alignment
        CHECK(reordered.alignmentTimeAt(0) == align_2);
        CHECK(reordered.alignmentTimeAt(1) == align_1);
        CHECK(reordered.alignmentTimeAt(2) == align_0);

        // Views should also be reordered consistently
        CHECK(reordered[0]->size() == gathered[2]->size());
        CHECK(reordered[2]->size() == gathered[0]->size());
    }

    SECTION("arbitrary permutation") {
        auto reordered = gathered.reorder({1, 2, 0});

        CHECK(reordered.alignmentTimeAt(0) == align_1);
        CHECK(reordered.alignmentTimeAt(1) == align_2);
        CHECK(reordered.alignmentTimeAt(2) == align_0);
    }

    SECTION("identity permutation") {
        auto reordered = gathered.reorder({0, 1, 2});

        CHECK(reordered.alignmentTimeAt(0) == align_0);
        CHECK(reordered.alignmentTimeAt(1) == align_1);
        CHECK(reordered.alignmentTimeAt(2) == align_2);
    }
}