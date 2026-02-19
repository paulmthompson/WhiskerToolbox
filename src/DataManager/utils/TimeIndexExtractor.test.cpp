/**
 * @file TimeIndexExtractor.test.cpp
 * @brief Unit tests for extractTimeIndices() utility.
 *
 * Tests extraction of time indices from every supported DataManager data type.
 */

#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "utils/TimeIndexExtractor.hpp"

// =============================================================================
// Helper: create a DataManager with a default TimeFrame
// =============================================================================
static std::unique_ptr<DataManager> makeDataManager() {
    return std::make_unique<DataManager>();
}

// =============================================================================
// AnalogTimeSeries
// =============================================================================

TEST_CASE("extractTimeIndices - AnalogTimeSeries", "[TimeIndexExtractor][Analog]") {
    auto dm = makeDataManager();

    SECTION("Returns indices for populated series") {
        auto analog = std::make_shared<AnalogTimeSeries>(
                std::vector<float>{1.0f, 2.0f, 3.0f},
                std::vector<TimeFrameIndex>{
                        TimeFrameIndex(10),
                        TimeFrameIndex(20),
                        TimeFrameIndex(30)});
        dm->setData<AnalogTimeSeries>("analog_key", analog, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "analog_key");

        REQUIRE_FALSE(result.empty());
        REQUIRE(result.size() == 3);
        REQUIRE(result.indices[0] == TimeFrameIndex(10));
        REQUIRE(result.indices[1] == TimeFrameIndex(20));
        REQUIRE(result.indices[2] == TimeFrameIndex(30));
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty for empty series") {
        auto analog = std::make_shared<AnalogTimeSeries>();
        dm->setData<AnalogTimeSeries>("empty_analog", analog, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_analog");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// DigitalEventSeries
// =============================================================================

TEST_CASE("extractTimeIndices - DigitalEventSeries", "[TimeIndexExtractor][DigitalEvent]") {
    auto dm = makeDataManager();

    SECTION("Returns event timestamps") {
        auto events = std::make_shared<DigitalEventSeries>(
                std::vector<TimeFrameIndex>{
                        TimeFrameIndex(5),
                        TimeFrameIndex(15),
                        TimeFrameIndex(25)});
        dm->setData<DigitalEventSeries>("events_key", events, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "events_key");

        REQUIRE(result.size() == 3);
        REQUIRE(result.indices[0] == TimeFrameIndex(5));
        REQUIRE(result.indices[1] == TimeFrameIndex(15));
        REQUIRE(result.indices[2] == TimeFrameIndex(25));
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty for empty series") {
        auto events = std::make_shared<DigitalEventSeries>();
        dm->setData<DigitalEventSeries>("empty_events", events, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_events");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// DigitalIntervalSeries
// =============================================================================

TEST_CASE("extractTimeIndices - DigitalIntervalSeries", "[TimeIndexExtractor][DigitalInterval]") {
    auto dm = makeDataManager();

    SECTION("Returns interval start times") {
        auto intervals = std::make_shared<DigitalIntervalSeries>();
        intervals->addEvent(Interval{100, 200});
        intervals->addEvent(Interval{300, 400});
        intervals->addEvent(Interval{500, 600});
        dm->setData<DigitalIntervalSeries>("intervals_key", intervals, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "intervals_key");

        REQUIRE(result.size() == 3);
        REQUIRE(result.indices[0] == TimeFrameIndex(100));
        REQUIRE(result.indices[1] == TimeFrameIndex(300));
        REQUIRE(result.indices[2] == TimeFrameIndex(500));
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty for empty series") {
        auto intervals = std::make_shared<DigitalIntervalSeries>();
        dm->setData<DigitalIntervalSeries>("empty_intervals", intervals, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_intervals");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// MaskData (ragged type)
// =============================================================================

TEST_CASE("extractTimeIndices - MaskData", "[TimeIndexExtractor][Mask]") {
    auto dm = makeDataManager();

    SECTION("Returns times with data") {
        auto masks = std::make_shared<MaskData>();
        masks->addAtTime(TimeFrameIndex(10), Mask2D{}, NotifyObservers::No);
        masks->addAtTime(TimeFrameIndex(20), Mask2D{}, NotifyObservers::No);
        dm->setData<MaskData>("masks_key", masks, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "masks_key");

        REQUIRE(result.size() == 2);
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty when no masks") {
        auto masks = std::make_shared<MaskData>();
        dm->setData<MaskData>("empty_masks", masks, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_masks");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// LineData (ragged type)
// =============================================================================

TEST_CASE("extractTimeIndices - LineData", "[TimeIndexExtractor][Line]") {
    auto dm = makeDataManager();

    SECTION("Returns times with data") {
        auto lines = std::make_shared<LineData>();
        lines->addAtTime(TimeFrameIndex(5), Line2D{{0.0f, 0.0f}, {1.0f, 1.0f}}, NotifyObservers::No);
        lines->addAtTime(TimeFrameIndex(15), Line2D{{2.0f, 2.0f}, {3.0f, 3.0f}}, NotifyObservers::No);
        lines->addAtTime(TimeFrameIndex(25), Line2D{{4.0f, 4.0f}, {5.0f, 5.0f}}, NotifyObservers::No);
        dm->setData<LineData>("lines_key", lines, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "lines_key");

        REQUIRE(result.size() == 3);
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty when no lines") {
        auto lines = std::make_shared<LineData>();
        dm->setData<LineData>("empty_lines", lines, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_lines");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// PointData (ragged type)
// =============================================================================

TEST_CASE("extractTimeIndices - PointData", "[TimeIndexExtractor][Points]") {
    auto dm = makeDataManager();

    SECTION("Returns times with data") {
        auto points = std::make_shared<PointData>();
        points->addAtTime(TimeFrameIndex(7), Point2D<float>{1.0f, 2.0f}, NotifyObservers::No);
        points->addAtTime(TimeFrameIndex(14), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);
        dm->setData<PointData>("points_key", points, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "points_key");

        REQUIRE(result.size() == 2);
        REQUIRE(result.time_frame != nullptr);
    }

    SECTION("Returns empty when no points") {
        auto points = std::make_shared<PointData>();
        dm->setData<PointData>("empty_points", points, TimeKey("time"));

        auto result = extractTimeIndices(*dm, "empty_points");

        REQUIRE(result.empty());
    }
}

// =============================================================================
// Unsupported / non-existent keys
// =============================================================================

TEST_CASE("extractTimeIndices - Edge cases", "[TimeIndexExtractor][Edge]") {
    auto dm = makeDataManager();

    SECTION("Returns empty for non-existent key") {
        auto result = extractTimeIndices(*dm, "no_such_key");

        REQUIRE(result.empty());
        REQUIRE(result.time_frame == nullptr);
    }
}
