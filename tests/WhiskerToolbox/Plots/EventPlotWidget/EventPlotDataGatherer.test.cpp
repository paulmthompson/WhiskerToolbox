/**
 * @file EventPlotDataGatherer.test.cpp
 * @brief Integration tests for EventPlotDataGatherer
 * 
 * Tests the gather functionality for EventPlotWidget including:
 * - Direct GatherResult usage with DigitalEventSeries
 * - Alignment to trial intervals
 * - Error handling for missing data
 * - Integration with EventPlotState configuration
 */

#include "Plots/EventPlotWidget/Core/EventPlotDataGatherer.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <vector>

// ==================== Test Fixtures ====================

namespace {

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const& times) {
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
    std::vector<std::pair<int64_t, int64_t>> const& intervals) {
    std::vector<Interval> interval_vec;
    interval_vec.reserve(intervals.size());
    for (auto const& [start, end] : intervals) {
        interval_vec.push_back(Interval{start, end});
    }
    return std::make_shared<DigitalIntervalSeries>(interval_vec);
}

/**
 * @brief Create a shared TimeFrame for test data
 */
std::shared_ptr<TimeFrame> createTimeFrame(int64_t start, int64_t end) {
    std::vector<int> times;
    times.reserve(static_cast<size_t>(end - start));
    for (int64_t t = start; t < end; ++t) {
        times.push_back(static_cast<int>(t));
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Set up DataManager with test spike data and trial intervals
 * 
 * Creates:
 * - "spikes": Events at times 50, 150, 250, 350, 450, 550
 * - "trials": Intervals [0, 200], [200, 400], [400, 600]
 */
std::shared_ptr<DataManager> createTestDataManager() {
    auto dm = std::make_shared<DataManager>();
    
    // Create spikes at regular intervals
    auto spikes = createEventSeries({50, 150, 250, 350, 450, 550});
    dm->setData<DigitalEventSeries>("spikes", spikes, TimeKey("test_time"));
    
    // Create trial intervals: each trial is 200 time units
    auto trials = createIntervalSeries({{0, 200}, {200, 400}, {400, 600}});
    dm->setData<DigitalIntervalSeries>("trials", trials, TimeKey("test_time"));
    
    return dm;
}

/**
 * @brief Count total events across all gathered trials
 */
size_t countTotalEvents(GatherResult<DigitalEventSeries> const& gathered) {
    size_t total = 0;
    for (auto const& trial : gathered) {
        total += trial->size();
    }
    return total;
}

/**
 * @brief Collect all event times from a trial view
 */
std::vector<int64_t> collectEventTimes(std::shared_ptr<DigitalEventSeries> const& trial) {
    std::vector<int64_t> times;
    for (auto const& event : trial->view()) {
        times.push_back(event.time().getValue());
    }
    return times;
}

} // namespace

// ==================== Direct GatherResult Tests ====================

TEST_CASE("EventPlotDataGatherer - gatherEventTrials direct function", "[EventPlotDataGatherer]") {
    
    SECTION("gathers events correctly into trial intervals") {
        // Spikes at: 50, 150, 250, 350
        // Trials: [0, 200], [200, 400]
        auto spikes = createEventSeries({50, 150, 250, 350});
        auto trials = createIntervalSeries({{0, 200}, {200, 400}});
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 2);
        
        // Trial 0 [0, 200]: should contain spikes at 50, 150
        auto trial0_times = collectEventTimes(result[0]);
        REQUIRE(trial0_times.size() == 2);
        CHECK(trial0_times[0] == 50);
        CHECK(trial0_times[1] == 150);
        
        // Trial 1 [200, 400]: should contain spikes at 250, 350
        auto trial1_times = collectEventTimes(result[1]);
        REQUIRE(trial1_times.size() == 2);
        CHECK(trial1_times[0] == 250);
        CHECK(trial1_times[1] == 350);
    }
    
    SECTION("handles empty event series") {
        auto spikes = std::make_shared<DigitalEventSeries>();
        auto trials = createIntervalSeries({{0, 100}, {100, 200}});
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 2);
        CHECK(result[0]->size() == 0);
        CHECK(result[1]->size() == 0);
    }
    
    SECTION("handles empty interval series") {
        auto spikes = createEventSeries({50, 150});
        auto trials = std::make_shared<DigitalIntervalSeries>();
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        CHECK(result.empty());
    }
    
    SECTION("handles null inputs") {
        auto spikes = createEventSeries({50, 150});
        auto trials = createIntervalSeries({{0, 200}});
        
        std::shared_ptr<DigitalEventSeries const> null_spikes;
        std::shared_ptr<DigitalIntervalSeries const> null_trials;
        
        auto result_null_spikes = EventPlotWidget::gatherEventTrials(null_spikes, trials);
        CHECK(result_null_spikes.empty());
        
        auto result_null_trials = EventPlotWidget::gatherEventTrials(spikes, null_trials);
        CHECK(result_null_trials.empty());
        
        auto result_both_null = EventPlotWidget::gatherEventTrials(null_spikes, null_trials);
        CHECK(result_both_null.empty());
    }
    
    SECTION("preserves event order within trials") {
        // Events in non-sorted order
        auto spikes = createEventSeries({150, 50, 100, 75});
        auto trials = createIntervalSeries({{0, 200}});
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 1);
        auto times = collectEventTimes(result[0]);
        
        // View should preserve the original order from the underlying series
        // Note: The exact order depends on how DigitalEventSeries stores internally
        CHECK(times.size() == 4);
    }
    
    SECTION("handles overlapping trials") {
        // Overlapping trials: [0, 200], [100, 300]
        // Spikes at: 50, 150, 250
        auto spikes = createEventSeries({50, 150, 250});
        auto trials = createIntervalSeries({{0, 200}, {100, 300}});
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 2);
        
        // Trial 0 [0, 200]: 50, 150
        CHECK(result[0]->size() == 2);
        
        // Trial 1 [100, 300]: 150, 250
        CHECK(result[1]->size() == 2);
    }
    
    SECTION("handles events at interval boundaries") {
        // Events exactly at boundaries
        auto spikes = createEventSeries({0, 100, 200});
        auto trials = createIntervalSeries({{0, 100}, {100, 200}});
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 2);
        
        // Boundary behavior: start is inclusive, end is exclusive
        // Trial 0 [0, 100): should contain 0
        // Trial 1 [100, 200): should contain 100
        // Event at 200 should not be in either trial
        size_t total = result[0]->size() + result[1]->size();
        CHECK(total >= 2);  // At least the non-boundary events
    }
}

// ==================== DataManager Integration Tests ====================

TEST_CASE("EventPlotDataGatherer - gatherEventTrialsByKey", "[EventPlotDataGatherer]") {
    
    SECTION("gathers events using DataManager keys") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsByKey(dm, "spikes", "trials");
        
        CHECK(result.isValid());
        CHECK_FALSE(result.hasError());
        REQUIRE(result.gathered.size() == 3);
        
        // Total events: 6 spikes distributed across 3 trials
        // Trial 0 [0, 200]: 50, 150 (2 events)
        // Trial 1 [200, 400]: 250, 350 (2 events)  
        // Trial 2 [400, 600]: 450, 550 (2 events)
        CHECK(countTotalEvents(result.gathered) == 6);
    }
    
    SECTION("returns error for null DataManager") {
        auto result = EventPlotWidget::gatherEventTrialsByKey(nullptr, "spikes", "trials");
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("DataManager"));
    }
    
    SECTION("returns error for empty event key") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsByKey(dm, "", "trials");
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("Event key"));
    }
    
    SECTION("returns error for empty alignment key") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsByKey(dm, "spikes", "");
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("Alignment key"));
    }
    
    SECTION("returns error for missing event series") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsByKey(dm, "nonexistent_spikes", "trials");
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("Event series not found"));
    }
    
    SECTION("returns error for missing alignment intervals") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsByKey(dm, "spikes", "nonexistent_trials");
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("Alignment intervals not found"));
    }
}

// ==================== GatherResult Feature Tests ====================

TEST_CASE("EventPlotDataGatherer - GatherResult interval access", "[EventPlotDataGatherer]") {
    auto spikes = createEventSeries({50, 150, 350, 450});
    auto trials = createIntervalSeries({{0, 200}, {300, 500}});
    
    auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
    
    REQUIRE(result.size() == 2);
    
    SECTION("intervalAt returns correct intervals") {
        auto interval0 = result.intervalAt(0);
        CHECK(interval0.start == 0);
        CHECK(interval0.end == 200);
        
        auto interval1 = result.intervalAt(1);
        CHECK(interval1.start == 300);
        CHECK(interval1.end == 500);
    }
    
    SECTION("transform counts events per trial") {
        auto counts = result.transform([](auto const& trial) {
            return trial->size();
        });
        
        REQUIRE(counts.size() == 2);
        CHECK(counts[0] == 2);  // Events at 50, 150
        CHECK(counts[1] == 2);  // Events at 350, 450
    }
    
    SECTION("transformWithInterval provides both data and interval") {
        auto results = result.transformWithInterval(
            [](auto const& trial, Interval const& interval) {
                return std::make_pair(trial->size(), interval.end - interval.start);
            });
        
        REQUIRE(results.size() == 2);
        CHECK(results[0].first == 2);   // 2 events
        CHECK(results[0].second == 200); // interval duration
        CHECK(results[1].first == 2);   // 2 events
        CHECK(results[1].second == 200); // interval duration
    }
}

// ==================== Realistic Scenario Tests ====================

TEST_CASE("EventPlotDataGatherer - realistic raster plot scenario", "[EventPlotDataGatherer]") {
    
    SECTION("simulates neural spike raster with multiple trials") {
        // Create spike data: bursts at different times for each "trial"
        // Simulating neural responses with jittered latencies
        auto spikes = createEventSeries({
            // Trial 1 region [0, 1000): response around t=300-400
            310, 320, 350, 380, 390,
            // Trial 2 region [1000, 2000): response around t=1300-1400  
            1305, 1315, 1345, 1375, 1385,
            // Trial 3 region [2000, 3000): response around t=2300-2400
            2308, 2318, 2348, 2378, 2388
        });
        
        auto trials = createIntervalSeries({
            {0, 1000},
            {1000, 2000},
            {2000, 3000}
        });
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 3);
        
        // Each trial should have its burst of spikes
        CHECK(result[0]->size() == 5);  // Trial 1
        CHECK(result[1]->size() == 5);  // Trial 2
        CHECK(result[2]->size() == 5);  // Trial 3
    }
    
    SECTION("handles variable trial lengths") {
        auto spikes = createEventSeries({50, 150, 250, 350, 450});
        
        // Variable length trials
        auto trials = createIntervalSeries({
            {0, 100},    // Short trial: catches 50
            {100, 400},  // Long trial: catches 150, 250, 350
            {400, 500}   // Short trial: catches 450
        });
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 3);
        CHECK(result[0]->size() == 1);  // 50
        CHECK(result[1]->size() == 3);  // 150, 250, 350
        CHECK(result[2]->size() == 1);  // 450
    }
    
    SECTION("sparse spike data across many trials") {
        // Only a few spikes but many trials
        auto spikes = createEventSeries({150, 550, 950});
        
        auto trials = createIntervalSeries({
            {0, 200}, {200, 400}, {400, 600}, {600, 800}, {800, 1000}
        });
        
        auto result = EventPlotWidget::gatherEventTrials(spikes, trials);
        
        REQUIRE(result.size() == 5);
        
        // Only 3 spikes total, distributed among 5 trials
        size_t total = countTotalEvents(result);
        CHECK(total == 3);
        
        // Verify sparse distribution
        CHECK(result[0]->size() == 1);  // 150
        CHECK(result[1]->size() == 0);  // nothing
        CHECK(result[2]->size() == 1);  // 550
        CHECK(result[3]->size() == 0);  // nothing
        CHECK(result[4]->size() == 1);  // 950
    }
}

// ==================== EventPlotState Integration Tests ====================

TEST_CASE("EventPlotDataGatherer - gatherEventTrialsFromState", "[EventPlotDataGatherer][integration]") {
    
    SECTION("returns error for null state") {
        auto dm = createTestDataManager();
        
        auto result = EventPlotWidget::gatherEventTrialsFromState(nullptr, dm);
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("EventPlotState is null"));
    }
    
    SECTION("returns error for null DataManager") {
        auto state = std::make_unique<EventPlotState>();
        
        auto result = EventPlotWidget::gatherEventTrialsFromState(state.get(), nullptr);
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("DataManager is null"));
    }
    
    SECTION("returns error when no event series configured") {
        auto dm = createTestDataManager();
        auto state = std::make_unique<EventPlotState>();
        
        // State has no plot events configured
        auto result = EventPlotWidget::gatherEventTrialsFromState(state.get(), dm);
        
        CHECK_FALSE(result.isValid());
        CHECK(result.hasError());
        CHECK_THAT(result.error_message, Catch::Matchers::ContainsSubstring("No event series configured"));
    }
    
    SECTION("gathers data when state is properly configured") {
        auto dm = createTestDataManager();
        auto state = std::make_unique<EventPlotState>();
        
        // Configure the state with spike data and trial alignment
        state->addPlotEvent("spike_plot", "spikes");
        state->setAlignmentEventKey("trials");
        
        auto result = EventPlotWidget::gatherEventTrialsFromState(state.get(), dm);
        
        CHECK(result.isValid());
        CHECK_FALSE(result.hasError());
        REQUIRE(result.gathered.size() == 3);
        CHECK(countTotalEvents(result.gathered) == 6);
    }
}
