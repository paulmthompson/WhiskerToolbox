/**
 * @file test_frame_filter.cpp
 * @brief Catch2 unit tests for FrameFilter and DataKeyFrameFilter.
 *
 * These tests are Qt-free.  They exercise:
 *   - shouldSkip() with known intervals (skip-tracked mode)
 *   - Inverted mode (skip-untracked)
 *   - Boundary cases: first frame, last frame, empty series, fully-tracked video
 *   - scanToNextNonSkipped: forward, backward, all-skipped, already-non-skipped
 */

#include "TimeScrollBar/FrameFilter.hpp"

#include "DataManager/DataManager.hpp"
#include "DataObjects/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a minimal DataManager with a single DigitalIntervalSeries under @p key.
struct Fixture {
    std::shared_ptr<DataManager> dm;
    std::shared_ptr<TimeFrame> tf;
    std::string key;

    Fixture()
        : dm{std::make_shared<DataManager>()},
          tf{std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9})},
          key{"tracked_regions"} {
        dm->setTime(TimeKey("time"), tf, true);
        dm->setData<DigitalIntervalSeries>(key, TimeKey("time"));
    }

    /// Get the DigitalIntervalSeries from dm so we can add intervals.
    std::shared_ptr<DigitalIntervalSeries> series() {
        return dm->getData<DigitalIntervalSeries>(key);
    }

    /// Build a DataKeyFrameFilter pointing at this fixture's key.
    std::unique_ptr<DataKeyFrameFilter> makeFilter(bool skip_tracked = true) {
        auto f = std::make_unique<DataKeyFrameFilter>(dm, key, skip_tracked);
        f->setTimeFrame(tf);
        return f;
    }
};

}// namespace

// ===========================================================================
// DataKeyFrameFilter — skip-tracked mode
// ===========================================================================

TEST_CASE("DataKeyFrameFilter: empty series — no frames skipped", "[FrameFilter]") {
    Fixture fix;
    auto filter = fix.makeFilter();

    // No intervals have been added.
    for (int i = 0; i <= 9; ++i) {
        REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(i)));
    }
}

TEST_CASE("DataKeyFrameFilter: single interval [3, 6] — correct frames skipped", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(3), TimeFrameIndex(6));
    auto filter = fix.makeFilter();

    // Outside interval
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(0)));
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(2)));
    // Interval boundaries (inclusive)
    REQUIRE(filter->shouldSkip(TimeFrameIndex(3)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(4)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(5)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(6)));
    // Outside interval
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(7)));
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(9)));
}

TEST_CASE("DataKeyFrameFilter: first-frame boundary [0, 0]", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(0), TimeFrameIndex(0));
    auto filter = fix.makeFilter();

    REQUIRE(filter->shouldSkip(TimeFrameIndex(0)));
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(1)));
}

TEST_CASE("DataKeyFrameFilter: last-frame boundary [9, 9]", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(9), TimeFrameIndex(9));
    auto filter = fix.makeFilter();

    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(8)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(9)));
}

TEST_CASE("DataKeyFrameFilter: fully-tracked [0, 9] — all frames skipped", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(0), TimeFrameIndex(9));
    auto filter = fix.makeFilter();

    for (int i = 0; i <= 9; ++i) {
        REQUIRE(filter->shouldSkip(TimeFrameIndex(i)));
    }
}

// ===========================================================================
// DataKeyFrameFilter — inverted mode (skip_tracked = false)
// ===========================================================================

TEST_CASE("DataKeyFrameFilter: inverted mode — skips frames outside interval", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(2), TimeFrameIndex(4));
    auto filter = fix.makeFilter(/*skip_tracked=*/false);

    // Frames inside the interval are NOT skipped.
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(2)));
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(3)));
    REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(4)));

    // Frames outside the interval ARE skipped.
    REQUIRE(filter->shouldSkip(TimeFrameIndex(0)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(1)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(5)));
    REQUIRE(filter->shouldSkip(TimeFrameIndex(9)));
}

// ===========================================================================
// DataKeyFrameFilter — null-guard
// ===========================================================================

TEST_CASE("DataKeyFrameFilter: no TimeFrame set — never skips", "[FrameFilter]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(0), TimeFrameIndex(9));
    // Deliberately do NOT call setTimeFrame().
    auto filter = std::make_unique<DataKeyFrameFilter>(fix.dm, fix.key);

    for (int i = 0; i <= 9; ++i) {
        REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(i)));
    }
}

TEST_CASE("DataKeyFrameFilter: missing key in DataManager — never skips", "[FrameFilter]") {
    Fixture fix;
    auto filter = std::make_unique<DataKeyFrameFilter>(fix.dm, "nonexistent_key");
    filter->setTimeFrame(fix.tf);

    for (int i = 0; i <= 9; ++i) {
        REQUIRE_FALSE(filter->shouldSkip(TimeFrameIndex(i)));
    }
}

// ===========================================================================
// scanToNextNonSkipped
// ===========================================================================

TEST_CASE("scanToNextNonSkipped: no skipped frames — returns start immediately", "[FrameFilter][scan]") {
    Fixture fix;// empty series
    auto filter = fix.makeFilter();

    int result = frame_filter::scanToNextNonSkipped(*filter, 5, 1, 0, 9);
    REQUIRE(result == 5);

    result = frame_filter::scanToNextNonSkipped(*filter, 5, -1, 0, 9);
    REQUIRE(result == 5);
}

TEST_CASE("scanToNextNonSkipped: forward scan skips tracked interval [3,6]", "[FrameFilter][scan]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(3), TimeFrameIndex(6));
    auto filter = fix.makeFilter();

    // Scan starts at 3 (tracked): should land on 7.
    int result = frame_filter::scanToNextNonSkipped(*filter, 3, 1, 0, 9);
    REQUIRE(result == 7);
}

TEST_CASE("scanToNextNonSkipped: backward scan skips tracked interval [3,6]", "[FrameFilter][scan]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(3), TimeFrameIndex(6));
    auto filter = fix.makeFilter();

    // Scan starts at 6 (tracked): should land on 2.
    int result = frame_filter::scanToNextNonSkipped(*filter, 6, -1, 0, 9);
    REQUIRE(result == 2);
}

TEST_CASE("scanToNextNonSkipped: fully-tracked — returns -1", "[FrameFilter][scan]") {
    Fixture fix;
    fix.series()->addEvent(TimeFrameIndex(0), TimeFrameIndex(9));
    auto filter = fix.makeFilter();

    int result = frame_filter::scanToNextNonSkipped(*filter, 0, 1, 0, 9);
    REQUIRE(result == -1);
}

TEST_CASE("scanToNextNonSkipped: scan hits boundary — no wrap", "[FrameFilter][scan]") {
    Fixture fix;
    // Interval covers 7-9, scan starts at 7 forward: no non-skipped frame available.
    fix.series()->addEvent(TimeFrameIndex(7), TimeFrameIndex(9));
    auto filter = fix.makeFilter();

    int result = frame_filter::scanToNextNonSkipped(*filter, 7, 1, 0, 9);
    REQUIRE(result == -1);
}

TEST_CASE("scanToNextNonSkipped: scan before range start — returns -1 immediately", "[FrameFilter][scan]") {
    Fixture fix;
    auto filter = fix.makeFilter();

    // start is below min_val — should bail out immediately.
    int result = frame_filter::scanToNextNonSkipped(*filter, -1, 1, 0, 9);
    REQUIRE(result == -1);
}
