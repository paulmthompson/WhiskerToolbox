/**
 * @file LineBatchBuilder.test.cpp
 * @brief Unit tests for building LineBatchData from LineData and GatherResult.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/LineBatch/LineBatchBuilder.hpp"
#include "CorePlotting/LineBatch/LineBatchData.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cstdint>
#include <memory>
#include <vector>

using namespace CorePlotting;

// ═══════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════

namespace {

/// Create a LineData with known lines for testing.
auto makeLineData()
{
    auto ld = std::make_shared<LineData>();

    // Line A: triangle (3 points → 2 segments), at time 0
    std::vector<Point2D<float>> line_a = {{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 0.0f}};
    ld->addAtTime(TimeFrameIndex(0), line_a, NotifyObservers::No);

    // Line B: single segment (2 points → 1 segment), at time 1
    std::vector<Point2D<float>> line_b = {{3.0f, 3.0f}, {4.0f, 4.0f}};
    ld->addAtTime(TimeFrameIndex(1), line_b, NotifyObservers::No);

    return ld;
}

/// Create a simple AnalogTimeSeries over [start, start+count).
auto makeAnalog(std::int64_t start, std::size_t count)
{
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(count);
    times.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        data.push_back(static_cast<float>(i) * 0.1f);
        times.push_back(TimeFrameIndex(start + static_cast<std::int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// buildLineBatchFromLineData
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("buildLineBatchFromLineData — basic topology", "[CorePlotting][LineBatch]")
{
    auto ld = makeLineData();
    auto batch = buildLineBatchFromLineData(*ld, 800.0f, 600.0f);

    SECTION("canvas size is preserved") {
        CHECK(batch.canvas_width == 800.0f);
        CHECK(batch.canvas_height == 600.0f);
    }

    SECTION("correct line and segment counts") {
        // Line A: 2 segments, Line B: 1 segment
        REQUIRE(batch.numLines() == 2);
        REQUIRE(batch.numSegments() == 3);
    }

    SECTION("segment data is correct for line A") {
        // First segment: (0,0)→(1,1)
        CHECK(batch.segments[0] == 0.0f);
        CHECK(batch.segments[1] == 0.0f);
        CHECK(batch.segments[2] == 1.0f);
        CHECK(batch.segments[3] == 1.0f);

        // Second segment: (1,1)→(2,0)
        CHECK(batch.segments[4] == 1.0f);
        CHECK(batch.segments[5] == 1.0f);
        CHECK(batch.segments[6] == 2.0f);
        CHECK(batch.segments[7] == 0.0f);
    }

    SECTION("segment data is correct for line B") {
        // Third segment: (3,3)→(4,4)
        CHECK(batch.segments[8] == 3.0f);
        CHECK(batch.segments[9] == 3.0f);
        CHECK(batch.segments[10] == 4.0f);
        CHECK(batch.segments[11] == 4.0f);
    }

    SECTION("line_ids are 1-based consecutive") {
        CHECK(batch.line_ids[0] == 1);
        CHECK(batch.line_ids[1] == 1);
        CHECK(batch.line_ids[2] == 2);
    }

    SECTION("LineInfo metadata") {
        CHECK(batch.lines[0].first_segment == 0);
        CHECK(batch.lines[0].segment_count == 2);
        CHECK(batch.lines[1].first_segment == 2);
        CHECK(batch.lines[1].segment_count == 1);
    }

    SECTION("all lines visible, none selected") {
        REQUIRE(batch.visibility_mask.size() == 2);
        REQUIRE(batch.selection_mask.size() == 2);
        CHECK(batch.visibility_mask[0] == 1);
        CHECK(batch.visibility_mask[1] == 1);
        CHECK(batch.selection_mask[0] == 0);
        CHECK(batch.selection_mask[1] == 0);
    }
}

TEST_CASE("buildLineBatchFromLineData — empty LineData", "[CorePlotting][LineBatch]")
{
    LineData empty_ld;
    auto batch = buildLineBatchFromLineData(empty_ld, 1.0f, 1.0f);

    REQUIRE(batch.empty());
    REQUIRE(batch.numSegments() == 0);
}

TEST_CASE("buildLineBatchFromLineData — single-point lines are skipped", "[CorePlotting][LineBatch]")
{
    auto ld = std::make_shared<LineData>();

    // One-point line → should be skipped (can't form a segment)
    std::vector<Point2D<float>> single_pt = {{5.0f, 5.0f}};
    ld->addAtTime(TimeFrameIndex(0), single_pt, NotifyObservers::No);

    auto batch = buildLineBatchFromLineData(*ld, 1.0f, 1.0f);
    REQUIRE(batch.empty());
}

// ═══════════════════════════════════════════════════════════════════════
// buildLineBatchFromGatherResult
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("buildLineBatchFromGatherResult — basic topology", "[CorePlotting][LineBatch]")
{
    // Source analog: 100 samples at times [0..99]
    auto analog = makeAnalog(0, 100);

    // Two trials: [10,20) and [50,60)
    auto intervals = std::make_shared<DigitalIntervalSeries>(
        std::vector<Interval>{{10, 20}, {50, 60}});

    auto gathered = gather(analog, intervals);
    REQUIRE(gathered.size() == 2);

    // Alignment times at interval starts
    std::vector<std::int64_t> align = {10, 50};

    auto batch = buildLineBatchFromGatherResult(gathered, align);

    SECTION("line count matches trial count") {
        REQUIRE(batch.numLines() == 2);
    }

    SECTION("segment counts match sample counts minus 1") {
        // Each trial has 11 samples (inclusive range [10,20], [50,60]) → 10 segments each
        CHECK(batch.lines[0].segment_count == 10);
        CHECK(batch.lines[1].segment_count == 10);
        CHECK(batch.numSegments() == 20);
    }

    SECTION("trial indices are correct") {
        CHECK(batch.lines[0].trial_index == 0);
        CHECK(batch.lines[1].trial_index == 1);
    }

    SECTION("x-coordinates are relative to alignment time") {
        // First segment of trial 0: time 10 with align 10 → x=0
        // and time 11 with align 10 → x=1
        CHECK(batch.segments[0] == 0.0f); // x1 = 10 - 10
        CHECK(batch.segments[2] == 1.0f); // x2 = 11 - 10
    }

    SECTION("all lines visible, none selected") {
        CHECK(batch.visibility_mask.size() == 2);
        CHECK(batch.selection_mask.size() == 2);
        for (std::size_t i = 0; i < 2; ++i) {
            CHECK(batch.visibility_mask[i] == 1);
            CHECK(batch.selection_mask[i] == 0);
        }
    }
}

TEST_CASE("buildLineBatchFromGatherResult — empty gather", "[CorePlotting][LineBatch]")
{
    // Source with only 5 samples, no intervals overlap
    auto analog = makeAnalog(0, 5);
    auto intervals = std::make_shared<DigitalIntervalSeries>(
        std::vector<Interval>{{100, 200}});

    auto gathered = gather(analog, intervals);

    std::vector<std::int64_t> align = {100};
    auto batch = buildLineBatchFromGatherResult(gathered, align);

    // The gathered trial may exist but have < 2 samples → line skipped
    // Result should be empty or have 0 usable lines
    CHECK(batch.numLines() == 0);
}
