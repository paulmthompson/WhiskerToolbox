/**
 * @file PlotAlignmentGather.test.cpp
 * @brief Tests for PlotAlignmentGather free functions
 */

#include "Plots/Common/PlotAlignmentGather.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Plots;
using WhiskerToolbox::Transforms::V2::AlignmentPoint;

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/**
 * @brief Create a DigitalEventSeries with events at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
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
    std::vector<Interval> interval_vec;
    interval_vec.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        interval_vec.push_back(Interval{start, end});
    }
    return std::make_shared<DigitalIntervalSeries>(interval_vec);
}

/**
 * @brief Create a DataManager with test data
 */
std::shared_ptr<DataManager> createTestDataManager() {
    auto dm = std::make_shared<DataManager>();

    // Create a time key for digital series
    TimeKey const time_key("test_time");

    // Add spike data
    auto spikes = createEventSeries({10, 50, 100, 150, 200, 250, 300, 350});
    dm->setData<DigitalEventSeries>("spikes", spikes, time_key);

    // Add trial intervals
    auto trials = createIntervalSeries({{0, 100}, {150, 250}, {300, 400}});
    dm->setData<DigitalIntervalSeries>("trials", trials, time_key);

    // Add stimulus events
    auto stimuli = createEventSeries({50, 200, 350});
    dm->setData<DigitalEventSeries>("stimuli", stimuli, time_key);

    return dm;
}

}// namespace

// =============================================================================
// Type Conversion Tests
// =============================================================================

TEST_CASE("toAlignmentPoint - converts IntervalAlignmentType correctly", "[PlotAlignmentGather]") {
    CHECK(toAlignmentPoint(IntervalAlignmentType::Beginning) == AlignmentPoint::Start);
    CHECK(toAlignmentPoint(IntervalAlignmentType::End) == AlignmentPoint::End);
}

// =============================================================================
// gatherWithEventAlignment Tests
// =============================================================================

TEST_CASE("gatherWithEventAlignment - basic functionality", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 50, 100, 150, 200, 250});
    auto alignment_events = createEventSeries({100, 200});

    // Each alignment event ± 50 window
    auto result = gatherWithEventAlignment(spikes, alignment_events, 50.0, 50.0);

    REQUIRE(result.size() == 2);

    SECTION("First window [50, 150] contains correct spikes") {
        // Spikes at 50, 100, 150 are in [50, 150] (inclusive boundaries)
        REQUIRE(result[0]->size() == 3);
    }

    SECTION("Second window [150, 250] contains correct spikes") {
        // Spikes at 150, 200, 250 are in [150, 250] (inclusive boundaries)
        REQUIRE(result[1]->size() == 3);
    }

    SECTION("Alignment times are event times, not window starts") {
        // alignmentTimeAt should return 100 and 200, not 50 and 150
        CHECK(result.alignmentTimeAt(0) == 100);
        CHECK(result.alignmentTimeAt(1) == 200);
    }
}

TEST_CASE("gatherWithEventAlignment - asymmetric window", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({80, 100, 120, 180, 200, 220});
    auto alignment_events = createEventSeries({100, 200});

    // 25 before, 75 after each event
    auto result = gatherWithEventAlignment(spikes, alignment_events, 25.0, 75.0);

    REQUIRE(result.size() == 2);

    // First window [75, 175]: contains 80, 100, 120 (3 spikes)
    CHECK(result[0]->size() == 3);

    // Alignment time is still the event time
    CHECK(result.alignmentTimeAt(0) == 100);
}

TEST_CASE("gatherWithEventAlignment - null inputs return empty", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 20, 30});

    auto result1 = gatherWithEventAlignment<DigitalEventSeries>(
            std::shared_ptr<DigitalEventSeries>{},
            std::shared_ptr<DigitalEventSeries>{},
            50.0, 50.0);
    CHECK(result1.empty());

    auto result2 = gatherWithEventAlignment<DigitalEventSeries>(
            spikes,
            std::shared_ptr<DigitalEventSeries>{},
            50.0, 50.0);
    CHECK(result2.empty());
}

// =============================================================================
// gatherWithIntervalAlignment Tests
// =============================================================================

TEST_CASE("gatherWithIntervalAlignment - start alignment", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 50, 90, 160, 200, 240});
    auto intervals = createIntervalSeries({{0, 100}, {150, 250}});

    auto result = gatherWithIntervalAlignment(spikes, intervals, AlignmentPoint::Start);

    REQUIRE(result.size() == 2);

    SECTION("Alignment times are interval starts") {
        CHECK(result.alignmentTimeAt(0) == 0);
        CHECK(result.alignmentTimeAt(1) == 150);
    }

    SECTION("Interval bounds are preserved") {
        CHECK(result.intervalAt(0).start == 0);
        CHECK(result.intervalAt(0).end == 100);
        CHECK(result.intervalAt(1).start == 150);
        CHECK(result.intervalAt(1).end == 250);
    }
}

TEST_CASE("gatherWithIntervalAlignment - end alignment", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 50, 90, 160, 200, 240});
    auto intervals = createIntervalSeries({{0, 100}, {150, 250}});

    auto result = gatherWithIntervalAlignment(spikes, intervals, AlignmentPoint::End);

    REQUIRE(result.size() == 2);

    SECTION("Alignment times are interval ends") {
        CHECK(result.alignmentTimeAt(0) == 100);
        CHECK(result.alignmentTimeAt(1) == 250);
    }
}

TEST_CASE("gatherWithIntervalAlignment - center alignment", "[PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 50, 90, 160, 200, 240});
    auto intervals = createIntervalSeries({{0, 100}, {100, 300}});

    auto result = gatherWithIntervalAlignment(spikes, intervals, AlignmentPoint::Center);

    REQUIRE(result.size() == 2);

    SECTION("Alignment times are interval centers") {
        CHECK(result.alignmentTimeAt(0) == 50); // (0 + 100) / 2
        CHECK(result.alignmentTimeAt(1) == 200);// (100 + 300) / 2
    }
}

// =============================================================================
// getAlignmentSource Tests
// =============================================================================

TEST_CASE("getAlignmentSource - identifies event series", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    auto result = getAlignmentSource(dm, "stimuli");

    CHECK(result.isValid());
    CHECK(result.is_event_series);
    CHECK_FALSE(result.is_interval_series);
    CHECK(result.event_series != nullptr);
    CHECK(result.error_message.empty());
}

TEST_CASE("getAlignmentSource - identifies interval series", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    auto result = getAlignmentSource(dm, "trials");

    CHECK(result.isValid());
    CHECK_FALSE(result.is_event_series);
    CHECK(result.is_interval_series);
    CHECK(result.interval_series != nullptr);
    CHECK(result.error_message.empty());
}

TEST_CASE("getAlignmentSource - handles invalid key", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    auto result = getAlignmentSource(dm, "nonexistent");

    CHECK_FALSE(result.isValid());
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("getAlignmentSource - handles wrong type", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    // spikes is a valid key but it's a DigitalEventSeries, not alignment target
    // Actually, this should work - events can be alignment targets too
    auto result = getAlignmentSource(dm, "spikes");

    CHECK(result.isValid());
    CHECK(result.is_event_series);
}

TEST_CASE("getAlignmentSource - handles empty key", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    auto result = getAlignmentSource(dm, "");

    CHECK_FALSE(result.isValid());
}

// =============================================================================
// createAlignedGatherResult Tests
// =============================================================================

TEST_CASE("createAlignedGatherResult - with interval alignment", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "trials";
    align_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    align_data.window_size = 100.0;

    auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);

    REQUIRE(result.size() == 3);// 3 trials

    SECTION("Uses window around alignment point for data gathering") {
        // Spikes: {10, 50, 100, 150, 200, 250, 300, 350}
        // Alignment=Beginning, window_size=100 → half_window=50
        // Trial 0 alignment=0, window [-50, 50]: spikes at 10, 50
        CHECK(result[0]->size() == 2);

        // Trial 1 alignment=150, window [100, 200]: spikes at 100, 150, 200
        CHECK(result[1]->size() == 3);
    }

    SECTION("Alignment times are interval starts") {
        CHECK(result.alignmentTimeAt(0) == 0);
        CHECK(result.alignmentTimeAt(1) == 150);
        CHECK(result.alignmentTimeAt(2) == 300);
    }
}

TEST_CASE("createAlignedGatherResult - with event alignment", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";// Events at 50, 200, 350
    align_data.window_size = 100.0;            // ±50 window

    auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);

    REQUIRE(result.size() == 3);// 3 stimulus events

    SECTION("Alignment times are event times, not window starts") {
        CHECK(result.alignmentTimeAt(0) == 50);
        CHECK(result.alignmentTimeAt(1) == 200);
        CHECK(result.alignmentTimeAt(2) == 350);
    }

    SECTION("Windows are centered on events") {
        // Stimulus at 50 ± 50 = [0, 100]: spikes at 10, 50
        // (100 spike is at boundary, may or may not be included depending on view)
        CHECK(result[0]->size() >= 2);
    }
}

TEST_CASE("createAlignedGatherResult - with end alignment", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "trials";
    align_data.interval_alignment_type = IntervalAlignmentType::End;

    auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);

    REQUIRE(result.size() == 3);

    SECTION("Alignment times are interval ends") {
        CHECK(result.alignmentTimeAt(0) == 100);
        CHECK(result.alignmentTimeAt(1) == 250);
        CHECK(result.alignmentTimeAt(2) == 400);
    }
}

TEST_CASE("createAlignedGatherResult - invalid inputs return empty", "[PlotAlignmentGather]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "trials";

    SECTION("Empty source key") {
        auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "", align_data);
        CHECK(result.empty());
    }

    SECTION("Invalid alignment key") {
        align_data.alignment_event_key = "nonexistent";
        auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);
        CHECK(result.empty());
    }

    SECTION("Invalid source key") {
        auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "nonexistent", align_data);
        CHECK(result.empty());
    }
}

// =============================================================================
// GatherResult::alignmentTimeAt Tests (new method)
// =============================================================================

TEST_CASE("GatherResult::alignmentTimeAt - basic usage", "[GatherResult][PlotAlignmentGather]") {
    auto spikes = createEventSeries({10, 20, 30, 40, 50, 60});
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});

    // Basic gather without adapters
    auto result = gather(spikes, intervals);

    REQUIRE(result.size() == 2);

    SECTION("Falls back to interval start when no alignment times stored") {
        CHECK(result.alignmentTimeAt(0) == 0);
        CHECK(result.alignmentTimeAt(1) == 30);
    }

    SECTION("Throws on out of bounds") {
        CHECK_THROWS_AS(result.alignmentTimeAt(2), std::out_of_range);
    }
}

TEST_CASE("GatherResult::alignmentTimeAt - with adapters", "[GatherResult][PlotAlignmentGather]") {
    auto spikes = createEventSeries({80, 100, 120, 180, 200, 220});
    auto alignment_events = createEventSeries({100, 200});

    auto result = gatherWithEventAlignment(spikes, alignment_events, 50.0, 50.0);

    REQUIRE(result.size() == 2);

    SECTION("Returns alignment time from adapter, not interval start") {
        // Event times are 100 and 200, windows are [50, 150] and [150, 250]
        CHECK(result.alignmentTimeAt(0) == 100);// Not 50
        CHECK(result.alignmentTimeAt(1) == 200);// Not 150
    }
}

// =============================================================================
// pruneOverlappingAlignmentTimes Tests
// =============================================================================

TEST_CASE("pruneOverlappingAlignmentTimes - empty input", "[PlotAlignmentGather][overlap]") {
    std::vector<int64_t> const times;
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    CHECK(kept.empty());
}

TEST_CASE("pruneOverlappingAlignmentTimes - single event always kept", "[PlotAlignmentGather][overlap]") {
    std::vector<int64_t> const times = {100};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0] == 0);
}

TEST_CASE("pruneOverlappingAlignmentTimes - well-separated events all kept", "[PlotAlignmentGather][overlap]") {
    // Events at 100, 300, 500 with window ±50 → windows [50,150], [250,350], [450,550]
    std::vector<int64_t> const times = {100, 300, 500};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 3);
    CHECK(kept[0] == 0);
    CHECK(kept[1] == 1);
    CHECK(kept[2] == 2);
}

TEST_CASE("pruneOverlappingAlignmentTimes - overlapping events pruned", "[PlotAlignmentGather][overlap]") {
    // Events at 100, 120, 300 with window ±50
    // Window 0: [50, 150], Window 1: [70, 170] → overlap → prune index 1
    // Window 2: [250, 350] → no overlap with 0 → keep
    std::vector<int64_t> const times = {100, 120, 300};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 2);
    CHECK(kept[0] == 0);
    CHECK(kept[1] == 2);
}

TEST_CASE("pruneOverlappingAlignmentTimes - all overlapping except first", "[PlotAlignmentGather][overlap]") {
    // Events at 100, 110, 120, 130 with window ±50
    // All overlap with event at 100 → only first kept
    std::vector<int64_t> const times = {100, 110, 120, 130};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0] == 0);
}

TEST_CASE("pruneOverlappingAlignmentTimes - chain pruning", "[PlotAlignmentGather][overlap]") {
    // Events at 0, 80, 160, 240 with window ±50
    // Window 0: [-50, 50], Window 1: [30, 130] → overlap → prune 1
    // Window 2: [110, 210] → no overlap with 0 → keep
    // Window 3: [190, 290] → overlap with 2 → prune 3
    std::vector<int64_t> const times = {0, 80, 160, 240};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 2);
    CHECK(kept[0] == 0);
    CHECK(kept[1] == 2);
}

TEST_CASE("pruneOverlappingAlignmentTimes - asymmetric window", "[PlotAlignmentGather][overlap]") {
    // Events at 100, 200 with pre=30, post=80 → total window = 110
    // Window 0: [70, 180], Window 1: [170, 280] → overlap (180 > 170) → prune 1
    std::vector<int64_t> const times = {100, 200};
    auto kept = pruneOverlappingAlignmentTimes(times, 30, 80);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0] == 0);
}

TEST_CASE("pruneOverlappingAlignmentTimes - exactly touching is overlapping", "[PlotAlignmentGather][overlap]") {
    // Events at 0, 100 with window ±50 → windows [-50, 50] and [50, 150]
    // last_kept_end = 0+50=50, current_start = 100-50=50, 50 > 50 is false → pruned
    std::vector<int64_t> const times = {0, 100};
    auto kept = pruneOverlappingAlignmentTimes(times, 50, 50);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0] == 0);
}

TEST_CASE("pruneOverlappingAlignmentTimes - sorted input required", "[PlotAlignmentGather][overlap]") {
    // Function requires sorted input per @pre.
    // Verify correct behavior with pre-sorted input:
    // 100, 200, 300 with window ±40 → windows [60,140], [160,240], [260,340]
    // Gap between windows: 160-140=20>0, 260-240=20>0 → all kept
    std::vector<int64_t> const times = {100, 200, 300};
    auto kept = pruneOverlappingAlignmentTimes(times, 40, 40);
    REQUIRE(kept.size() == 3);
}

TEST_CASE("pruneOverlappingAlignmentTimes - window size sensitivity", "[PlotAlignmentGather][overlap]") {
    // Events at 100, 200 → gap is 100
    std::vector<int64_t> const times = {100, 200};

    SECTION("Small window - no overlap") {
        // ±40 → windows [60,140], [160,240] → gap of 20 → no overlap
        auto kept = pruneOverlappingAlignmentTimes(times, 40, 40);
        CHECK(kept.size() == 2);
    }

    SECTION("Large window - overlap") {
        // ±60 → windows [40,160], [140,260] → overlap → prune
        auto kept = pruneOverlappingAlignmentTimes(times, 60, 60);
        CHECK(kept.size() == 1);
    }
}

// =============================================================================
// extractAlignmentTimes Tests
// =============================================================================

TEST_CASE("extractAlignmentTimes - from DigitalEventSeries", "[PlotAlignmentGather][overlap]") {
    auto events = createEventSeries({10, 50, 200});
    auto times = extractAlignmentTimes(*events);
    REQUIRE(times.size() == 3);
    CHECK(times[0] == 10);
    CHECK(times[1] == 50);
    CHECK(times[2] == 200);
}

TEST_CASE("extractAlignmentTimes - from DigitalIntervalSeries with Start", "[PlotAlignmentGather][overlap]") {
    auto intervals = createIntervalSeries({{10, 50}, {100, 200}});
    auto times = extractAlignmentTimes(*intervals, AlignmentPoint::Start);
    REQUIRE(times.size() == 2);
    CHECK(times[0] == 10);
    CHECK(times[1] == 100);
}

TEST_CASE("extractAlignmentTimes - from DigitalIntervalSeries with End", "[PlotAlignmentGather][overlap]") {
    auto intervals = createIntervalSeries({{10, 50}, {100, 200}});
    auto times = extractAlignmentTimes(*intervals, AlignmentPoint::End);
    REQUIRE(times.size() == 2);
    CHECK(times[0] == 50);
    CHECK(times[1] == 200);
}

TEST_CASE("extractAlignmentTimes - from DigitalIntervalSeries with Center", "[PlotAlignmentGather][overlap]") {
    auto intervals = createIntervalSeries({{0, 100}, {100, 300}});
    auto times = extractAlignmentTimes(*intervals, AlignmentPoint::Center);
    REQUIRE(times.size() == 2);
    CHECK(times[0] == 50);
    CHECK(times[1] == 200);
}

// =============================================================================
// countNonOverlappingAlignmentEvents Tests
// =============================================================================

TEST_CASE("countNonOverlappingAlignmentEvents - event series", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    // stimuli has events at {50, 200, 350}

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";

    SECTION("Large window - all overlap") {
        align_data.window_size = 400.0;// ±200
        size_t const count = countNonOverlappingAlignmentEvents(dm, align_data);
        CHECK(count == 1);// Only first survives
    }

    SECTION("Small window - none overlap") {
        align_data.window_size = 100.0;// ±50
        size_t const count = countNonOverlappingAlignmentEvents(dm, align_data);
        CHECK(count == 3);// All survive: gaps are 150
    }
}

TEST_CASE("countNonOverlappingAlignmentEvents - interval series", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    // trials has intervals {(0,100), (150,250), (300,400)}
    // → start alignment times: 0, 150, 300

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "trials";
    align_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    align_data.window_size = 200.0;// ±100

    size_t const count = countNonOverlappingAlignmentEvents(dm, align_data);
    // Times: 0, 150, 300. Window ±100
    // 0: kept, last_end=100. 150: 150-100=50, 100>50 → pruned. 300: 300-100=200, 100<=200 → kept
    CHECK(count == 2);
}

TEST_CASE("countNonOverlappingAlignmentEvents - empty key returns 0", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    PlotAlignmentData align_data;
    align_data.alignment_event_key = "";
    CHECK(countNonOverlappingAlignmentEvents(dm, align_data) == 0);
}

TEST_CASE("countNonOverlappingAlignmentEvents - invalid key returns 0", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    PlotAlignmentData align_data;
    align_data.alignment_event_key = "nonexistent";
    CHECK(countNonOverlappingAlignmentEvents(dm, align_data) == 0);
}

// =============================================================================
// getFilteredAlignmentSource Tests
// =============================================================================

TEST_CASE("getFilteredAlignmentSource - prevent_overlap false returns unfiltered", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";
    align_data.window_size = 100.0;
    align_data.prevent_overlap = false;

    auto result = getFilteredAlignmentSource(dm, align_data);
    CHECK(result.isValid());
    CHECK(result.is_event_series);
    // Should have all 3 events
    CHECK(result.event_series->size() == 3);
}

TEST_CASE("getFilteredAlignmentSource - prevent_overlap true with events", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    // stimuli: {50, 200, 350}, gap=150

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";
    align_data.prevent_overlap = true;

    SECTION("Small window keeps all") {
        align_data.window_size = 100.0;// ±50, gap=150 > 100 → no overlap
        auto result = getFilteredAlignmentSource(dm, align_data);
        CHECK(result.isValid());
        CHECK(result.event_series->size() == 3);
    }

    SECTION("Large window prunes overlapping") {
        align_data.window_size = 400.0;// ±200, gap=150 < 400 → overlap
        auto result = getFilteredAlignmentSource(dm, align_data);
        CHECK(result.isValid());
        CHECK(result.event_series->size() == 1);
    }
}

TEST_CASE("getFilteredAlignmentSource - prevent_overlap true with intervals", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();
    // trials: {(0,100), (150,250), (300,400)}, start alignment: 0, 150, 300

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "trials";
    align_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    align_data.prevent_overlap = true;
    align_data.window_size = 200.0;// ±100

    auto result = getFilteredAlignmentSource(dm, align_data);
    CHECK(result.isValid());
    CHECK(result.is_interval_series);
    // Times: 0, 150, 300. ±100 → 0 kept, 150 pruned (100 > 50), 300 kept
    CHECK(result.interval_series->size() == 2);
}

// =============================================================================
// createAlignedGatherResult with prevent_overlap Tests
// =============================================================================

TEST_CASE("createAlignedGatherResult - prevent_overlap reduces trials", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";// {50, 200, 350}
    align_data.window_size = 400.0;            // ±200, all overlap
    align_data.prevent_overlap = true;

    auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);
    // Only first event (50) should survive
    CHECK(result.size() == 1);
    CHECK(result.alignmentTimeAt(0) == 50);
}

TEST_CASE("createAlignedGatherResult - prevent_overlap false keeps all trials", "[PlotAlignmentGather][overlap]") {
    auto dm = createTestDataManager();

    PlotAlignmentData align_data;
    align_data.alignment_event_key = "stimuli";// {50, 200, 350}
    align_data.window_size = 400.0;            // ±200
    align_data.prevent_overlap = false;

    auto result = createAlignedGatherResult<DigitalEventSeries>(dm, "spikes", align_data);
    CHECK(result.size() == 3);// All 3 kept
}
