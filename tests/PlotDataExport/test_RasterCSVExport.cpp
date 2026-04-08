/**
 * @file test_RasterCSVExport.cpp
 * @brief Unit tests for RasterCSVExport (trial-aligned event CSV export)
 *
 * Tests cover: empty inputs, single/multiple series, multiple trials,
 * custom delimiters, metadata header, and trials with no events.
 *
 * Construction pattern: build DigitalEventSeries with known events at known
 * TimeFrameIndex values, create DigitalIntervalSeries for trial intervals,
 * call gather() to produce GatherResult, then pass to exportRasterToCSV().
 *
 * With basic gather() (no alignment adapters), alignmentTimeAt(trial) returns
 * interval.start. Relative time = event_time - interval.start.
 */

#include "PlotDataExport/RasterCSVExport.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "GatherResult/GatherResult.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

std::shared_ptr<DigitalEventSeries> makeEvents(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto t: times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

std::shared_ptr<DigitalIntervalSeries> makeIntervals(
        std::vector<std::pair<int64_t, int64_t>> const & ivs) {
    std::vector<Interval> iv;
    iv.reserve(ivs.size());
    for (auto const & [s, e]: ivs) {
        iv.push_back(Interval{s, e});
    }
    return std::make_shared<DigitalIntervalSeries>(iv);
}

/**
 * @brief Create a TimeFrame with identity mapping: index i → time i
 * @pre size must exceed the maximum event time used in the test
 */
std::shared_ptr<TimeFrame> makeIdentityTimeFrame(int size) {
    std::vector<int> times(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        times[static_cast<size_t>(i)] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

}// namespace

// =============================================================================
// Empty-input tests
// =============================================================================

TEST_CASE("exportRasterToCSV - empty series vector produces headers only",
          "[PlotDataExport][RasterCSVExport]") {
    std::string const csv = PlotDataExport::exportRasterToCSV({});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: raster"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("trial_index,event_key,relative_time"));

    // Exactly 2 lines: comment + header, no data rows
    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    CHECK(newlines == 2);
}

TEST_CASE("exportRasterToCSV - empty GatherResult produces headers only",
          "[PlotDataExport][RasterCSVExport]") {
    GatherResult<DigitalEventSeries> const empty_gathered;
    auto tf = makeIdentityTimeFrame(10);

    PlotDataExport::RasterSeriesInput const input{"spikes", &empty_gathered, tf.get()};
    std::string const csv = PlotDataExport::exportRasterToCSV({input});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("trial_index,event_key,relative_time"));

    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    CHECK(newlines == 2);
}

// =============================================================================
// Single series correctness
// =============================================================================

TEST_CASE("exportRasterToCSV - single series, single trial, single event",
          "[PlotDataExport][RasterCSVExport]") {
    // event at time 15, trial [10, 20]: relative = 15 - 10 = 5
    auto events = makeEvents({15});
    auto intervals = makeIntervals({{10, 20}});
    auto tf = makeIdentityTimeFrame(30);

    auto gathered = gather(events, intervals);
    PlotDataExport::RasterSeriesInput const input{"spikes", &gathered, tf.get()};

    std::string const csv =
            PlotDataExport::exportRasterToCSV({input});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("trial_index,event_key,relative_time\n"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,5\n"));
}

TEST_CASE("exportRasterToCSV - single series, single trial, multiple events",
          "[PlotDataExport][RasterCSVExport]") {
    // events at 11, 15, 18 in [10, 20]; alignment = 10 → relative: 1, 5, 8
    auto events = makeEvents({11, 15, 18});
    auto intervals = makeIntervals({{10, 20}});
    auto tf = makeIdentityTimeFrame(30);

    auto gathered = gather(events, intervals);
    PlotDataExport::RasterSeriesInput const input{"spikes", &gathered, tf.get()};

    std::string const csv = PlotDataExport::exportRasterToCSV({input});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,1\n"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,5\n"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,8\n"));
}

TEST_CASE("exportRasterToCSV - single series, multiple trials",
          "[PlotDataExport][RasterCSVExport]") {
    // trial 0: event 15 in [10,20], relative = 5
    // trial 1: event 35 in [30,40], relative = 5
    auto events = makeEvents({15, 35});
    auto intervals = makeIntervals({{10, 20}, {30, 40}});
    auto tf = makeIdentityTimeFrame(50);

    auto gathered = gather(events, intervals);
    PlotDataExport::RasterSeriesInput const input{"spikes", &gathered, tf.get()};

    std::string const csv = PlotDataExport::exportRasterToCSV({input});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,5\n"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("1,spikes,5\n"));
}

TEST_CASE("exportRasterToCSV - trial with no events produces no data row",
          "[PlotDataExport][RasterCSVExport]") {
    // event at 15 is only in trial [10,20]; trial [30,40] is empty
    auto events = makeEvents({15});
    auto intervals = makeIntervals({{10, 20}, {30, 40}});
    auto tf = makeIdentityTimeFrame(50);

    auto gathered = gather(events, intervals);
    PlotDataExport::RasterSeriesInput const input{"spikes", &gathered, tf.get()};

    std::string const csv = PlotDataExport::exportRasterToCSV({input});

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,5\n"));
    CHECK(csv.find("1,spikes,") == std::string::npos);
}

// =============================================================================
// Multiple series
// =============================================================================

TEST_CASE("exportRasterToCSV - multiple series, correct event_key per row",
          "[PlotDataExport][RasterCSVExport]") {
    auto spikes = makeEvents({15});
    auto licks = makeEvents({12});
    auto intervals = makeIntervals({{10, 20}});
    auto tf = makeIdentityTimeFrame(30);

    auto gathered_spikes = gather(spikes, intervals);
    auto gathered_licks = gather(licks, intervals);

    PlotDataExport::RasterSeriesInput const in_spikes{"spikes", &gathered_spikes, tf.get()};
    PlotDataExport::RasterSeriesInput const in_licks{"licks", &gathered_licks, tf.get()};

    std::string const csv =
            PlotDataExport::exportRasterToCSV({in_spikes, in_licks});

    // spikes: 15 - 10 = 5  |  licks: 12 - 10 = 2
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,spikes,5\n"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0,licks,2\n"));
}

// =============================================================================
// Custom delimiter
// =============================================================================

TEST_CASE("exportRasterToCSV - custom tab delimiter",
          "[PlotDataExport][RasterCSVExport]") {
    auto events = makeEvents({15});
    auto intervals = makeIntervals({{10, 20}});
    auto tf = makeIdentityTimeFrame(30);

    auto gathered = gather(events, intervals);
    PlotDataExport::RasterSeriesInput const input{"spikes", &gathered, tf.get()};

    std::string const csv =
            PlotDataExport::exportRasterToCSV({input}, {}, "\t");

    CHECK_THAT(csv,
               Catch::Matchers::ContainsSubstring(
                       "trial_index\tevent_key\trelative_time"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("0\tspikes\t5\n"));
}

// =============================================================================
// Metadata comment header
// =============================================================================

TEST_CASE("exportRasterToCSV - metadata comment header is written",
          "[PlotDataExport][RasterCSVExport]") {
    PlotDataExport::RasterExportMetadata meta;
    meta.alignment_key = "stimulus";
    meta.window_size = 1000.0;

    std::string const csv = PlotDataExport::exportRasterToCSV({}, meta);

    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: raster"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("# alignment_key: stimulus"));
    CHECK_THAT(csv, Catch::Matchers::ContainsSubstring("# window_size: 1000"));
}

TEST_CASE("exportRasterToCSV - metadata not written when empty",
          "[PlotDataExport][RasterCSVExport]") {
    // Default metadata: empty key and zero window — neither extra line should appear
    std::string const csv = PlotDataExport::exportRasterToCSV({});

    CHECK(csv.find("# alignment_key:") == std::string::npos);
    CHECK(csv.find("# window_size:") == std::string::npos);
}
