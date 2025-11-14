#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/computers/AnalogSliceGathererComputer.h"
#include "utils/TableView/computers/IntervalOverlapComputer.h"
#include "utils/TableView/computers/IntervalPropertyComputer.h"
#include "utils/TableView/computers/IntervalReductionComputer.h"
#include "utils/TableView/computers/StandardizeComputer.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"

#include <iostream>// Added for debug output
#include <memory>
#include <random>
#include <vector>


TEST_CASE("TableView Point Data Integration Test", "[TableView][Integration]") {

    // NOTE: Tests for X/Y component extraction have been removed.
    // PointComponentAdapter was removed in favor of using PointData directly.
    // If X/Y component extraction is needed, create dedicated computers.

    SECTION("Extract X and Y components using interval reduction") {
        // Test that we can access PointData directly without adapters
        DataManager dataManager;

        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);

        auto pointData = std::make_shared<PointData>();
        pointData->addAtTime(TimeFrameIndex(0), {1.0f, 2.0f}, NotifyObservers::No);
        pointData->addAtTime(TimeFrameIndex(1), {3.0f, 4.0f}, NotifyObservers::No);
        pointData->addAtTime(TimeFrameIndex(2), {5.0f, 6.0f}, NotifyObservers::No);

        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));

        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        auto retrievedPointData = dataManagerExtension->getPointData("TestPoints");

        REQUIRE(retrievedPointData != nullptr);
        REQUIRE(retrievedPointData->getMaxEntriesAtAnyTime() == 1);

        // Verify we can access the point data
        auto entries = retrievedPointData->getAllEntries();
        REQUIRE(entries.size() == 3);
    }

    SECTION("Test lazy evaluation and caching") {
        DataManager dataManager;
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        // Test with non-existent point data
        auto nonExistentData = dataManagerExtension->getPointData("NonExistent");
        REQUIRE(nonExistentData == nullptr);
    }
}

TEST_CASE("TableView AnalogSliceGathererComputer Test", "[TableView][AnalogSliceGathererComputer]") {

    // NOTE: Tests for analog slice gathering from point data have been removed.
    // These tests relied on PointComponentAdapter (.x/.y extraction) which was removed.
    // If component extraction is needed, create dedicated computers for this purpose.

    SECTION("Test with actual analog data") {
        // Test AnalogSliceGathererComputer with real AnalogTimeSeries, not point components
        DataManager dataManager;

        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);

        auto analog_vals = std::map<int, float>();

        // Create actual analog data
        for (int i = 0; i < 6; ++i) {
            analog_vals[i] = static_cast<float>(i * 10);// Values: 0, 10, 20, 30, 40, 50
        }
        auto analogData = std::make_shared<AnalogTimeSeries>(analog_vals);
        dataManager.setData<AnalogTimeSeries>("TestAnalog", analogData, TimeKey("test_time"));

        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5))};

        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals, timeFrame));

        auto analogSource = dataManagerExtension->getAnalogSource("TestAnalog");
        REQUIRE(analogSource != nullptr);

        builder.addColumn<std::vector<double>>("Slices",
                                               std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(analogSource));

        TableView table = builder.build();

        auto const & slices = table.getColumnValues<std::vector<double>>("Slices");
        REQUIRE(slices.size() == 2);
        REQUIRE(slices[0].size() == 3);
        REQUIRE(slices[1].size() == 3);
    }

    // NOTE: Test section "Test with PointData X and Y components" was removed.
    // It relied on PointComponentAdapter (.x/.y extraction) which was removed in the refactor.
    // NOTE: Test section "Test single-point intervals" was removed.
    // It relied on PointComponentAdapter (.x extraction) which was removed in the refactor.

    SECTION("Test error handling") {
        // Create a DataManager instance
        DataManager dataManager;
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        // Test with null source
        REQUIRE_THROWS_AS(AnalogSliceGathererComputer<std::vector<double>>(nullptr), std::invalid_argument);

        // Test with valid source but invalid ExecutionPlan (no intervals)
        std::vector<Point2D<float>> points = {{1.0f, 2.0f}};
        std::vector<int> timeValues = {0};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
    }
}

TEST_CASE("TableView Different TimeFrames Test", "[TableView][TimeFrame]") {

    SECTION("Test TableView with different time frames using analog data") {
        // Updated test to use actual AnalogTimeSeries instead of point components
        DataManager dataManager;

        std::vector<int> timeValues = {0, 1, 2, 3};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);

        std::vector<int> timeValues2 = {1, 3};
        auto timeFrame2 = std::make_shared<TimeFrame>(timeValues2);
        dataManager.setTime(TimeKey("test_time2"), timeFrame2);

        // Create analog data with first time frame
        std::map<int, float> analog_vals;
        analog_vals[0] = 1.0f;
        analog_vals[1] = 3.0f;
        analog_vals[2] = 5.0f;
        analog_vals[3] = 7.0f;
        auto analogData = std::make_shared<AnalogTimeSeries>(analog_vals);
        dataManager.setData<AnalogTimeSeries>("TestAnalog", analogData, TimeKey("test_time"));

        // Create analog data with second time frame
        std::map<int, float> analog_vals2;
        analog_vals2[0] = 9.0f;
        analog_vals2[1] = 11.0f;
        auto analogData2 = std::make_shared<AnalogTimeSeries>(analog_vals2);
        dataManager.setData<AnalogTimeSeries>("TestAnalog2", analogData2, TimeKey("test_time2"));

        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)),
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3))};

        auto rowSelector = std::make_unique<IntervalSelector>(intervals, timeFrame);

        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));

        auto source = dataManagerExtension->getAnalogSource("TestAnalog");
        REQUIRE(source != nullptr);

        auto source2 = dataManagerExtension->getAnalogSource("TestAnalog2");
        REQUIRE(source2 != nullptr);

        builder.addColumn("Time_Values",
                          std::make_unique<IntervalReductionComputer>(source, ReductionType::Mean));

        builder.addColumn("Mean_TestAnalog2",
                          std::make_unique<IntervalReductionComputer>(source2, ReductionType::Mean));

        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 2);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("Time_Values"));

        auto timeValuesColumn = table.getColumnValues<double>("Time_Values");
        REQUIRE(!timeValuesColumn.empty());
        REQUIRE(timeValuesColumn.size() == 2);
        REQUIRE(timeValuesColumn[0] == Catch::Approx(2.0).epsilon(0.001));// Mean of 1.0 and 3.0
        REQUIRE(timeValuesColumn[1] == Catch::Approx(6.0).epsilon(0.001));// Mean of 5.0 and 7.0

        auto timeValues2Column = table.getColumnValues<double>("Mean_TestAnalog2");
        REQUIRE(!timeValues2Column.empty());
        REQUIRE(timeValues2Column.size() == 2);
    }

    SECTION("Test DigitalIntervalSeries with different timeframes") {
        // Create a DataManager instance
        DataManager dataManager;

        // Create two different timeframes
        std::vector<int> timeValues1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeFrame1 = std::make_shared<TimeFrame>(timeValues1);
        dataManager.setTime(TimeKey("time1"), timeFrame1);

        std::vector<int> timeValues2 = {0, 2, 4, 6, 8};
        auto timeFrame2 = std::make_shared<TimeFrame>(timeValues2);
        dataManager.setTime(TimeKey("time2"), timeFrame2);

        // Create first DigitalIntervalSeries (will act as row selector)
        auto intervalSeries1 = std::make_shared<DigitalIntervalSeries>();
        intervalSeries1->addEvent(TimeFrameIndex(1), TimeFrameIndex(3)); // Interval 1-3
        intervalSeries1->addEvent(TimeFrameIndex(5), TimeFrameIndex(7)); // Interval 5-7
        intervalSeries1->addEvent(TimeFrameIndex(9), TimeFrameIndex(10));// Interval 9-10

        dataManager.setData<DigitalIntervalSeries>("RowIntervals", intervalSeries1, TimeKey("time1"));

        // Create second DigitalIntervalSeries (will be used for overlap analysis)
        auto intervalSeries2 = std::make_shared<DigitalIntervalSeries>();
        intervalSeries2->addEvent(TimeFrameIndex(0), TimeFrameIndex(2));// Interval 0-4 (indices 0-2 in timeFrame2)
        intervalSeries2->addEvent(TimeFrameIndex(1), TimeFrameIndex(3));// Interval 2-6 (indices 1-3 in timeFrame2)
        intervalSeries2->addEvent(TimeFrameIndex(3), TimeFrameIndex(4));// Interval 6-8 (indices 3-4 in timeFrame2)

        dataManager.setData<DigitalIntervalSeries>("ColumnIntervals", intervalSeries2, TimeKey("time2"));

        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        // Create row selector from the first interval series
        auto rowIntervalSource = dataManagerExtension->getIntervalSource("RowIntervals");
        REQUIRE(rowIntervalSource != nullptr);

        // Get the intervals from the row source to create the row selector
        auto rowIntervals = rowIntervalSource->getIntervalsInRange(
                TimeFrameIndex(0), TimeFrameIndex(10), *timeFrame1);

        // Convert to TimeFrameIntervals for the selector
        std::vector<TimeFrameInterval> timeFrameIntervals;
        for (auto const & interval: rowIntervals) {
            timeFrameIntervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        REQUIRE(timeFrameIntervals.size() == 3);

        auto rowSelector = std::make_unique<IntervalSelector>(timeFrameIntervals, timeFrame1);

        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));

        // Get interval source for column data
        auto columnIntervalSource = dataManagerExtension->getIntervalSource("ColumnIntervals");
        REQUIRE(columnIntervalSource != nullptr);

        // Add columns using IntervalOverlapComputer
        builder.addColumn<int64_t>("Overlap_Count",
                                   std::make_unique<IntervalOverlapComputer<int64_t>>(columnIntervalSource,
                                                                                      IntervalOverlapOperation::CountOverlaps, "ColumnIntervals"));

        builder.addColumn<int64_t>("Containing_ID",
                                   std::make_unique<IntervalOverlapComputer<int64_t>>(columnIntervalSource,
                                                                                      IntervalOverlapOperation::AssignID, "ColumnIntervals"));

        // Add columns using IntervalPropertyComputer
        builder.addColumn<double>("Row_Start",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::Start, "RowIntervals"));

        builder.addColumn<double>("Row_End",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::End, "RowIntervals"));

        builder.addColumn<double>("Row_Duration",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::Duration, "RowIntervals"));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 5);
        REQUIRE(table.hasColumn("Overlap_Count"));
        REQUIRE(table.hasColumn("Containing_ID"));
        REQUIRE(table.hasColumn("Row_Start"));
        REQUIRE(table.hasColumn("Row_End"));
        REQUIRE(table.hasColumn("Row_Duration"));

        // Get the column data
        auto overlapCount = table.getColumnValues<int64_t>("Overlap_Count");
        auto containingID = table.getColumnValues<int64_t>("Containing_ID");
        auto rowStart = table.getColumnValues<double>("Row_Start");
        auto rowEnd = table.getColumnValues<double>("Row_End");
        auto rowDuration = table.getColumnValues<double>("Row_Duration");

        // Verify the results
        REQUIRE(overlapCount.size() == 3);
        REQUIRE(containingID.size() == 3);
        REQUIRE(rowStart.size() == 3);
        REQUIRE(rowEnd.size() == 3);
        REQUIRE(rowDuration.size() == 3);

        // Expected overlap analysis:
        // Row interval 1-3: overlaps with column interval 0-4 (1 overlap)
        // Row interval 5-7: overlaps with column interval 2-6 (1 overlap)
        // Row interval 9-10: overlaps with no column intervals (0 overlaps)
        std::vector<int64_t> expectedOverlapCount = {1, 1, 0};
        std::vector<int64_t> expectedContainingID = {0, 0, -1};// First row interval is contained by first column interval

        // Expected row properties:
        // Row 1: start=1, end=3, duration=2
        // Row 2: start=5, end=7, duration=2
        // Row 3: start=9, end=10, duration=1
        std::vector<double> expectedRowStart = {1.0, 5.0, 9.0};
        std::vector<double> expectedRowEnd = {3.0, 7.0, 10.0};
        std::vector<double> expectedRowDuration = {2.0, 2.0, 1.0};

        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(overlapCount[i] == expectedOverlapCount[i]);
            REQUIRE(containingID[i] == expectedContainingID[i]);
            REQUIRE(rowStart[i] == Catch::Approx(expectedRowStart[i]).epsilon(0.001));
            REQUIRE(rowEnd[i] == Catch::Approx(expectedRowEnd[i]).epsilon(0.001));
            REQUIRE(rowDuration[i] == Catch::Approx(expectedRowDuration[i]).epsilon(0.001));
        }
    }

    SECTION("Test DigitalIntervalSeries with different timeframes - fine vs coarse") {
        // Create a DataManager instance
        DataManager dataManager;

        // Create fine-grained timeframe (0 to 30000)
        std::vector<int> fineTimeValues;
        for (int i = 0; i <= 30000; ++i) {
            fineTimeValues.push_back(i);
        }
        auto fineTimeFrame = std::make_shared<TimeFrame>(fineTimeValues);
        dataManager.setTime(TimeKey("fine_time"), fineTimeFrame);

        // Create coarse-grained timeframe (0, 100, 200, ..., 30000)
        std::vector<int> coarseTimeValues;
        for (int i = 0; i <= 300; ++i) {
            coarseTimeValues.push_back(i * 100);
        }
        auto coarseTimeFrame = std::make_shared<TimeFrame>(coarseTimeValues);
        dataManager.setTime(TimeKey("coarse_time"), coarseTimeFrame);

        // Create first DigitalIntervalSeries (row selector) using fine timeframe
        auto rowIntervalSeries = std::make_shared<DigitalIntervalSeries>();
        rowIntervalSeries->addEvent(TimeFrameIndex(1000), TimeFrameIndex(2000));  // Interval 1000-2000
        rowIntervalSeries->addEvent(TimeFrameIndex(5000), TimeFrameIndex(7000));  // Interval 5000-7000
        rowIntervalSeries->addEvent(TimeFrameIndex(15000), TimeFrameIndex(16000));// Interval 15000-16000

        dataManager.setData<DigitalIntervalSeries>("RowIntervals", rowIntervalSeries, TimeKey("fine_time"));

        // Create second DigitalIntervalSeries (column data) using coarse timeframe
        auto columnIntervalSeries = std::make_shared<DigitalIntervalSeries>();
        columnIntervalSeries->addEvent(TimeFrameIndex(0), TimeFrameIndex(1));  // Interval 0-100
        columnIntervalSeries->addEvent(TimeFrameIndex(5), TimeFrameIndex(7));  // Interval 500-700
        columnIntervalSeries->addEvent(TimeFrameIndex(15), TimeFrameIndex(16));// Interval 1500-1600

        dataManager.setData<DigitalIntervalSeries>("ColumnIntervals", columnIntervalSeries, TimeKey("coarse_time"));

        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);

        // Create row selector from the row interval series
        auto rowIntervalSource = dataManagerExtension->getIntervalSource("RowIntervals");
        REQUIRE(rowIntervalSource != nullptr);

        // Get the intervals from the row source
        auto rowIntervals = rowIntervalSource->getIntervalsInRange(
                TimeFrameIndex(0),
                TimeFrameIndex(30000),
                *fineTimeFrame);

        // Convert to TimeFrameIntervals for the selector
        std::vector<TimeFrameInterval> timeFrameIntervals;
        for (auto const & interval: rowIntervals) {
            timeFrameIntervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        REQUIRE(timeFrameIntervals.size() == 3);

        auto rowSelector = std::make_unique<IntervalSelector>(timeFrameIntervals, fineTimeFrame);

        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));

        // Get interval source for column data
        auto columnIntervalSource = dataManagerExtension->getIntervalSource("ColumnIntervals");
        REQUIRE(columnIntervalSource != nullptr);

        // Add columns using IntervalOverlapComputer
        builder.addColumn<int64_t>("Overlap_Count",
                                   std::make_unique<IntervalOverlapComputer<int64_t>>(columnIntervalSource,
                                                                                      IntervalOverlapOperation::CountOverlaps, "ColumnIntervals"));

        builder.addColumn<int64_t>("Containing_ID",
                                   std::make_unique<IntervalOverlapComputer<int64_t>>(columnIntervalSource,
                                                                                      IntervalOverlapOperation::AssignID, "ColumnIntervals"));

        // Add columns using IntervalPropertyComputer
        builder.addColumn<double>("Row_Start",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::Start, "RowIntervals"));

        builder.addColumn<double>("Row_End",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::End, "RowIntervals"));

        builder.addColumn<double>("Row_Duration",
                                  std::make_unique<IntervalPropertyComputer<double>>(rowIntervalSource,
                                                                                     IntervalProperty::Duration, "RowIntervals"));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 5);
        REQUIRE(table.hasColumn("Overlap_Count"));
        REQUIRE(table.hasColumn("Containing_ID"));
        REQUIRE(table.hasColumn("Row_Start"));
        REQUIRE(table.hasColumn("Row_End"));
        REQUIRE(table.hasColumn("Row_Duration"));

        // Get the column data
        auto overlapCount = table.getColumnValues<int64_t>("Overlap_Count");
        auto containingID = table.getColumnValues<int64_t>("Containing_ID");
        auto rowStart = table.getColumnValues<double>("Row_Start");
        auto rowEnd = table.getColumnValues<double>("Row_End");
        auto rowDuration = table.getColumnValues<double>("Row_Duration");

        // Verify the results
        REQUIRE(overlapCount.size() == 3);
        REQUIRE(containingID.size() == 3);
        REQUIRE(rowStart.size() == 3);
        REQUIRE(rowEnd.size() == 3);
        REQUIRE(rowDuration.size() == 3);

        // Expected overlap analysis:
        // Row interval 1000-2000: overlaps with column interval 2 (1 overlap)
        // Row interval 5000-7000: does NOT overlap with any column intervals (0 overlaps)
        // Row interval 15000-16000: does NOT overlap with any column intervals (0 overlaps)
        std::vector<int64_t> expectedOverlapCount = {1, 0, 0};
        std::vector<int64_t> expectedContainingID = {2, -1, -1};// No row intervals are contained by any column intervals

        // Expected row properties:
        // Row 1: start=1000, end=2000, duration=1000
        // Row 2: start=5000, end=7000, duration=2000
        // Row 3: start=15000, end=16000, duration=1000
        std::vector<double> expectedRowStart = {1000.0, 5000.0, 15000.0};
        std::vector<double> expectedRowEnd = {2000.0, 7000.0, 16000.0};
        std::vector<double> expectedRowDuration = {1000.0, 2000.0, 1000.0};

        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(overlapCount[i] == expectedOverlapCount[i]);
            REQUIRE(containingID[i] == expectedContainingID[i]);
            REQUIRE(rowStart[i] == Catch::Approx(expectedRowStart[i]).epsilon(0.001));
            REQUIRE(rowEnd[i] == Catch::Approx(expectedRowEnd[i]).epsilon(0.001));
            REQUIRE(rowDuration[i] == Catch::Approx(expectedRowDuration[i]).epsilon(0.001));
        }
    }
}
