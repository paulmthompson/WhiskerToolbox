#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/computers/IntervalReductionComputer.h"
#include "utils/TableView/computers/IntervalOverlapComputer.h"
#include "utils/TableView/computers/IntervalPropertyComputer.h"
#include "utils/TableView/computers/AnalogSliceGathererComputer.h"
#include "utils/TableView/computers/StandardizeComputer.h"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>
#include <vector>
#include <random>
#include <iostream> // Added for debug output


TEST_CASE("TableView Point Data Integration Test", "[TableView][Integration]") {
    
    SECTION("Extract X and Y components from Point Data") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create sample point data for multiple time frames (single point per frame for analog source compatibility)
        std::vector<std::vector<Point2D<float>>> pointFrames = {
            {{1.0f, 2.0f}},     // Frame 0
            {{7.0f, 8.0f}},     // Frame 1
            {{13.0f, 14.0f}},   // Frame 2
            {{19.0f, 20.0f}}    // Frame 3
        };
        
        // Create a TimeFrame for the point data
        std::vector<int> timeValues = {0, 1, 2, 3};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();

        for (size_t frameIdx = 0; frameIdx < pointFrames.size(); ++frameIdx) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(frameIdx));
            for (const auto& point : pointFrames[frameIdx]) {
                pointData->addAtTime(timeIndex,point, NotifyObservers::No);
            }
        }
        
        // Add the point data to the DataManager
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        // Create DataManagerExtension for TableView
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals for each time frame
        // Since PointComponentAdapter flattens all points from all time frames into a single array,
        // we need to create intervals that correspond to the array indices, not time frame indices
        // Frame 0: point at array index 0
        // Frame 1: point at array index 1  
        // Frame 2: point at array index 2
        // Frame 3: point at array index 3
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0)),  // Frame 0: index 0
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)),  // Frame 1: index 1
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(2)),  // Frame 2: index 2
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(3))   // Frame 3: index 3
        };
        auto rowSelector = std::make_unique<IntervalSelector>(intervals, timeFrame);
        
        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));
        
        // Get analog sources for X and Y components
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        auto ySource = dataManagerExtension->getAnalogSource("TestPoints.y");
        
        REQUIRE(xSource != nullptr);
        REQUIRE(ySource != nullptr);
        
        // Add columns for X and Y components using direct access
        builder.addColumn("X_Values", 
            std::make_unique<IntervalReductionComputer>(xSource, ReductionType::Mean));
        builder.addColumn("Y_Values", 
            std::make_unique<IntervalReductionComputer>(ySource, ReductionType::Mean));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("X_Values"));
        REQUIRE(table.hasColumn("Y_Values"));
        
        // Get the column data
        auto xValues = table.getColumnValues<double>("X_Values");
        auto yValues = table.getColumnValues<double>("Y_Values");
        
        // Verify the extracted values match the original data
        // Since we're using IntervalSelector with single time indices, each "interval" contains one point
        // at that time index, and we're taking the mean, so we should get the value of the single point
        
        // Expected values: single point at each time index
        // Frame 0: 1.0, 2.0
        // Frame 1: 7.0, 8.0
        // Frame 2: 13.0, 14.0
        // Frame 3: 19.0, 20.0
        std::vector<double> expectedX = {1.0, 7.0, 13.0, 19.0};
        std::vector<double> expectedY = {2.0, 8.0, 14.0, 20.0};
        
        REQUIRE(xValues.size() == 4);
        REQUIRE(yValues.size() == 4);
        
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(xValues[i] == Catch::Approx(expectedX[i]).epsilon(0.001));
            REQUIRE(yValues[i] == Catch::Approx(expectedY[i]).epsilon(0.001));
        }
    }
    
    SECTION("Extract X and Y components using interval reduction") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create sample point data with single point per frame for interval reduction testing
        std::vector<std::vector<Point2D<float>>> pointFrames = {
            {{1.0f, 2.0f}},     // Frame 0
            {{7.0f, 8.0f}},     // Frame 1  
            {{13.0f, 14.0f}},   // Frame 2
            {{19.0f, 20.0f}},   // Frame 3
            {{25.0f, 26.0f}},   // Frame 4
            {{31.0f, 32.0f}}    // Frame 5
        };
        
        // Create a TimeFrame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();

        for (size_t frameIdx = 0; frameIdx < pointFrames.size(); ++frameIdx) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(frameIdx));
            for (const auto& point : pointFrames[frameIdx]) {
                pointData->addAtTime(timeIndex, point, NotifyObservers::No);
            }
        }
        
        // Add the point data to the DataManager
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        // Create DataManagerExtension for TableView
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals that span multiple frames to test interval reduction
        // Interval 0: frames 0-1 (array indices 0-1) -> mean of 1.0,7.0 = 4.0 and 2.0,8.0 = 5.0
        // Interval 1: frames 2-3 (array indices 2-3) -> mean of 13.0,19.0 = 16.0 and 14.0,20.0 = 17.0  
        // Interval 2: frames 4-5 (array indices 4-5) -> mean of 25.0,31.0 = 28.0 and 26.0,32.0 = 29.0
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)),  // Frames 0-1: indices 0-1
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)),  // Frames 2-3: indices 2-3
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(5))   // Frames 4-5: indices 4-5
        };
        auto rowSelector = std::make_unique<IntervalSelector>(intervals, timeFrame);
        
        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));
        
        // Get analog sources for X and Y components
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        auto ySource = dataManagerExtension->getAnalogSource("TestPoints.y");
        
        REQUIRE(xSource != nullptr);
        REQUIRE(ySource != nullptr);
        
        // Add columns for X and Y components using mean reduction
        builder.addColumn("X_Mean", 
            std::make_unique<IntervalReductionComputer>(xSource, ReductionType::Mean));
        builder.addColumn("Y_Mean", 
            std::make_unique<IntervalReductionComputer>(ySource, ReductionType::Mean));
        
        // Add columns for X and Y components using max reduction
        builder.addColumn("X_Max", 
            std::make_unique<IntervalReductionComputer>(xSource, ReductionType::Max));
        builder.addColumn("Y_Max", 
            std::make_unique<IntervalReductionComputer>(ySource, ReductionType::Max));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 4);
        
        // Get the column data
        auto xMean = table.getColumnValues<double>("X_Mean");
        auto yMean = table.getColumnValues<double>("Y_Mean");
        auto xMax = table.getColumnValues<double>("X_Max");
        auto yMax = table.getColumnValues<double>("Y_Max");
        
        // Verify the computed means
        // Interval 0: frames 0-1 -> mean of (1.0, 7.0) = 4.0, mean of (2.0, 8.0) = 5.0
        // Interval 1: frames 2-3 -> mean of (13.0, 19.0) = 16.0, mean of (14.0, 20.0) = 17.0
        // Interval 2: frames 4-5 -> mean of (25.0, 31.0) = 28.0, mean of (26.0, 32.0) = 29.0
        std::vector<double> expectedXMean = {4.0, 16.0, 28.0};
        std::vector<double> expectedYMean = {5.0, 17.0, 29.0};
        // Max values from each interval
        std::vector<double> expectedXMax = {7.0, 19.0, 31.0};
        std::vector<double> expectedYMax = {8.0, 20.0, 32.0};
        
        REQUIRE(xMean.size() == 3);
        REQUIRE(yMean.size() == 3);
        REQUIRE(xMax.size() == 3);
        REQUIRE(yMax.size() == 3);
        
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(xMean[i] == Catch::Approx(expectedXMean[i]).epsilon(0.001));
            REQUIRE(yMean[i] == Catch::Approx(expectedYMean[i]).epsilon(0.001));
            REQUIRE(xMax[i] == Catch::Approx(expectedXMax[i]).epsilon(0.001));
            REQUIRE(yMax[i] == Catch::Approx(expectedYMax[i]).epsilon(0.001));
        }
    }
    
    SECTION("Test lazy evaluation and caching") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create simple point data
        std::vector<Point2D<float>> points = {{1.0f, 2.0f}, {3.0f, 4.0f}};
        
        // Create TimeFrame
        std::vector<int> timeValues = {0, 1};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        // Create PointData
        auto pointData = std::make_shared<PointData>();

        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i], NotifyObservers::No);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create TableView
        TableViewBuilder builder(dataManagerExtension);
        // For 2 points, they will be at array indices 0 and 1
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0)),  // Single point at index 0
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1))   // Single point at index 1
        };
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals, timeFrame));
        
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        builder.addColumn("X_Values", 
            std::make_unique<IntervalReductionComputer>(xSource, ReductionType::Mean));
        
        TableView table = builder.build();
        
        // First access should trigger computation
        auto xValues1 = table.getColumnValues<double>("X_Values");
        REQUIRE(xValues1.size() == 2);
        REQUIRE(xValues1[0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(xValues1[1] == Catch::Approx(3.0).epsilon(0.001));
        
        // Second access should use cached data (same values)
        auto xValues2 = table.getColumnValues<double>("X_Values");
        REQUIRE(xValues2.size() == 2);
        REQUIRE(xValues2[0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(xValues2[1] == Catch::Approx(3.0).epsilon(0.001));
        
        // The spans should point to the same cached data
        // We aren't caching anymore
        //REQUIRE(xValues1.data() == xValues2.data());
    }
    
    SECTION("Test error handling") {
        DataManager dataManager;
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Test with non-existent point data
        auto nonExistentSource = dataManagerExtension->getAnalogSource("NonExistent.x");
        REQUIRE(nonExistentSource == nullptr);
        
        // Test with invalid component
        auto invalidSource = dataManagerExtension->getAnalogSource("TestPoints.z");
        REQUIRE(invalidSource == nullptr);
    }
}

TEST_CASE("TableView AnalogSliceGathererComputer Test", "[TableView][AnalogSliceGathererComputer]") {
    
    SECTION("Test analog slice gathering from point data") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create sample point data with known values
        std::vector<Point2D<float>> points = {
            {1.0f, 10.0f},   // Index 0
            {2.0f, 20.0f},   // Index 1
            {3.0f, 30.0f},   // Index 2
            {4.0f, 40.0f},   // Index 3
            {5.0f, 50.0f},   // Index 4
            {6.0f, 60.0f}    // Index 5
        };
        
        // Create TimeFrame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();
        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i], NotifyObservers::No);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals for gathering slices
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),  // Gather indices 0-2
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4)),  // Gather indices 2-4
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(5))   // Gather indices 4-5
        };
        
        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals, timeFrame));

        // Get analog sources for X and Y components
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        auto ySource = dataManagerExtension->getAnalogSource("TestPoints.y");
        
        REQUIRE(xSource != nullptr);
        REQUIRE(ySource != nullptr);
        
        // Add columns for gathering X and Y slices
        builder.addColumn<std::vector<double>>("X_Slices", 
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(xSource));
        builder.addColumn<std::vector<double>>("Y_Slices", 
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(ySource));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("X_Slices"));
        REQUIRE(table.hasColumn("Y_Slices"));
        
        // Get the column data using the heterogeneous interface
        const auto& xSlices = table.getColumnValues<std::vector<double>>("X_Slices");
        const auto& ySlices = table.getColumnValues<std::vector<double>>("Y_Slices");
        
        // Verify the number of intervals
        REQUIRE(xSlices.size() == 3);
        REQUIRE(ySlices.size() == 3);
        
        // Verify first interval (indices 0-2): X values {1.0, 2.0, 3.0}
        REQUIRE(xSlices[0].size() == 3);
        REQUIRE(xSlices[0][0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(xSlices[0][1] == Catch::Approx(2.0).epsilon(0.001));
        REQUIRE(xSlices[0][2] == Catch::Approx(3.0).epsilon(0.001));
        
        // Verify first interval Y values {10.0, 20.0, 30.0}
        REQUIRE(ySlices[0].size() == 3);
        REQUIRE(ySlices[0][0] == Catch::Approx(10.0).epsilon(0.001));
        REQUIRE(ySlices[0][1] == Catch::Approx(20.0).epsilon(0.001));
        REQUIRE(ySlices[0][2] == Catch::Approx(30.0).epsilon(0.001));
        
        // Verify second interval (indices 2-4): X values {3.0, 4.0, 5.0}
        REQUIRE(xSlices[1].size() == 3);
        REQUIRE(xSlices[1][0] == Catch::Approx(3.0).epsilon(0.001));
        REQUIRE(xSlices[1][1] == Catch::Approx(4.0).epsilon(0.001));
        REQUIRE(xSlices[1][2] == Catch::Approx(5.0).epsilon(0.001));
        
        // Verify second interval Y values {30.0, 40.0, 50.0}
        REQUIRE(ySlices[1].size() == 3);
        REQUIRE(ySlices[1][0] == Catch::Approx(30.0).epsilon(0.001));
        REQUIRE(ySlices[1][1] == Catch::Approx(40.0).epsilon(0.001));
        REQUIRE(ySlices[1][2] == Catch::Approx(50.0).epsilon(0.001));
        
        // Verify third interval (indices 4-5): X values {5.0, 6.0}
        REQUIRE(xSlices[2].size() == 2);
        REQUIRE(xSlices[2][0] == Catch::Approx(5.0).epsilon(0.001));
        REQUIRE(xSlices[2][1] == Catch::Approx(6.0).epsilon(0.001));
        
        // Verify third interval Y values {50.0, 60.0}
        REQUIRE(ySlices[2].size() == 2);
        REQUIRE(ySlices[2][0] == Catch::Approx(50.0).epsilon(0.001));
        REQUIRE(ySlices[2][1] == Catch::Approx(60.0).epsilon(0.001));
    }
    
    SECTION("Test single-point intervals") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create sample point data
        std::vector<Point2D<float>> points = {
            {1.5f, 2.5f},   // Index 0
            {3.5f, 4.5f},   // Index 1
            {5.5f, 6.5f}    // Index 2
        };
        
        // Create TimeFrame
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();
        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i], NotifyObservers::No);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create single-point intervals
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0)),  // Single point at index 0
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)),  // Single point at index 1
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(2))   // Single point at index 2
        };
        
        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals, timeFrame));
        
        // Get analog source for X component
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        REQUIRE(xSource != nullptr);
        
        // Add column for gathering X slices
        builder.addColumn<std::vector<double>>("X_Slices", 
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(xSource));
        
        // Build the table
        TableView table = builder.build();
        
        // Get the column data
        const auto& xSlices = table.getColumnValues<std::vector<double>>("X_Slices");
        
        // Verify the results
        REQUIRE(xSlices.size() == 3);
        
        // Each slice should contain exactly one point
        REQUIRE(xSlices[0].size() == 1);
        REQUIRE(xSlices[0][0] == Catch::Approx(1.5).epsilon(0.001));
        
        REQUIRE(xSlices[1].size() == 1);
        REQUIRE(xSlices[1][0] == Catch::Approx(3.5).epsilon(0.001));
        
        REQUIRE(xSlices[2].size() == 1);
        REQUIRE(xSlices[2][0] == Catch::Approx(5.5).epsilon(0.001));
    }
    
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
        dataManager.setTime(TimeKey("test_time"), timeFrame);
        
        auto pointData = std::make_shared<PointData>();
        pointData->addAtTime(TimeFrameIndex(0), points[0], NotifyObservers::No);
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));
        
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        REQUIRE(xSource != nullptr);
        
        auto computer = std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(xSource);
        
        // Create an ExecutionPlan with indices instead of intervals
        ExecutionPlan planWithIndices(std::vector<TimeFrameIndex>{TimeFrameIndex(0)}, nullptr);

        // This should throw because the computer expects intervals
        REQUIRE_THROWS_AS(computer->compute(planWithIndices), std::invalid_argument);
    }
}

TEST_CASE("TableView Different TimeFrames Test", "[TableView][TimeFrame]") {
    
    SECTION("Test TableView with different time frames") {

        // Create a DataManager instance
        DataManager dataManager;
        
        // Create a simple TimeFrame
        std::vector<int> timeValues = {0, 1, 2, 3};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime(TimeKey("test_time"), timeFrame);

        std::vector<int> timeValues2 = {1, 3};
        auto timeFrame2 = std::make_shared<TimeFrame>(timeValues2);
        dataManager.setTime(TimeKey("test_time2"), timeFrame2);

        // Create PointData with multiple time frames
        auto pointData = std::make_shared<PointData>();
        pointData->addAtTime(TimeFrameIndex(0), Point2D<float>(1.0f, 2.0f), NotifyObservers::No);
        pointData->addAtTime(TimeFrameIndex(1), Point2D<float>(3.0f, 4.0f), NotifyObservers::No);
        pointData->addAtTime(TimeFrameIndex(2), Point2D<float>(5.0f, 6.0f), NotifyObservers::No);
        pointData->addAtTime(TimeFrameIndex(3), Point2D<float>(7.0f, 8.0f), NotifyObservers::No);

        // Add the point data to the DataManager
        dataManager.setData<PointData>("TestPoints", pointData, TimeKey("test_time"));

        // Add another PointData with a different time frame
        auto pointData2 = std::make_shared<PointData>();
        pointData2->addAtTime(TimeFrameIndex(0), Point2D<float>(9.0f, 10.0f), NotifyObservers::No);
        pointData2->addAtTime(TimeFrameIndex(1), Point2D<float>(11.0f, 12.0f), NotifyObservers::No);
        dataManager.setData<PointData>("TestPoints2", pointData2, TimeKey("test_time2"));

        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals with the TimeFrame key
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)),
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3))
        };
        
        // Create an IntervalSelector with the TimeFrame key
        auto rowSelector = std::make_unique<IntervalSelector>(intervals, timeFrame);

        // Create TableView builder
        TableViewBuilder builder(dataManagerExtension);
        builder.setRowSelector(std::move(rowSelector));

        // Add a simple column using IntervalReductionComputer
        auto source = dataManagerExtension->getAnalogSource("TestPoints.x");
        REQUIRE(source != nullptr);

        auto source2 = dataManagerExtension->getAnalogSource("TestPoints2.x");
        REQUIRE(source2 != nullptr);

        builder.addColumn("Time_Values", 
            std::make_unique<IntervalReductionComputer>(source, ReductionType::Mean));

        builder.addColumn("Mean_TestPoints2.x", 
            std::make_unique<IntervalReductionComputer>(source2, ReductionType::Mean));
        
        // Try to build the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 2);

        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("Time_Values"));

        // Get the column data
        auto timeValuesColumn = table.getColumnValues<double>("Time_Values");

        REQUIRE(!timeValuesColumn.empty());
        REQUIRE(timeValuesColumn.size() == 2);
        REQUIRE(timeValuesColumn[0] == Catch::Approx(2.0).epsilon(0.001)); // Mean of 1.0 and 3.0
        REQUIRE(timeValuesColumn[1] == Catch::Approx(6.0).epsilon(0.001)); // Mean of 5.0 and 7.0

        // Verify that the second time frame data is not included
        auto timeValues2Column = table.getColumnValues<double>("Mean_TestPoints2.x");

        REQUIRE(!timeValues2Column.empty());
        REQUIRE(timeValues2Column.size() == 2);
        REQUIRE(timeValues2Column[0] == Catch::Approx(9.0).epsilon(0.001)); // Mean of 9.0 and NaN
        REQUIRE(timeValues2Column[1] == Catch::Approx(11.0).epsilon(0.001)); // Mean of NaN and 11.0

    

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
        intervalSeries1->addEvent(TimeFrameIndex(1), TimeFrameIndex(3));  // Interval 1-3
        intervalSeries1->addEvent(TimeFrameIndex(5), TimeFrameIndex(7));  // Interval 5-7
        intervalSeries1->addEvent(TimeFrameIndex(9), TimeFrameIndex(10));  // Interval 9-10
        
        dataManager.setData<DigitalIntervalSeries>("RowIntervals", intervalSeries1, TimeKey("time1"));    
        
        // Create second DigitalIntervalSeries (will be used for overlap analysis)
        auto intervalSeries2 = std::make_shared<DigitalIntervalSeries>();
        intervalSeries2->addEvent(TimeFrameIndex(0), TimeFrameIndex(2));  // Interval 0-4 (indices 0-2 in timeFrame2)
        intervalSeries2->addEvent(TimeFrameIndex(1), TimeFrameIndex(3));  // Interval 2-6 (indices 1-3 in timeFrame2)
        intervalSeries2->addEvent(TimeFrameIndex(3), TimeFrameIndex(4));  // Interval 6-8 (indices 3-4 in timeFrame2)
        
        dataManager.setData<DigitalIntervalSeries>("ColumnIntervals", intervalSeries2, TimeKey("time2"));
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create row selector from the first interval series
        auto rowIntervalSource = dataManagerExtension->getIntervalSource("RowIntervals");
        REQUIRE(rowIntervalSource != nullptr);
        
        // Get the intervals from the row source to create the row selector
        auto rowIntervals = rowIntervalSource->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(10), timeFrame1.get());
        
        REQUIRE(rowIntervals.size() == 3);
        
        // Convert to TimeFrameIntervals for the selector
        std::vector<TimeFrameInterval> timeFrameIntervals;
        for (const auto& interval : rowIntervals) {
            timeFrameIntervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        
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
        std::vector<int64_t> expectedContainingID = {0, 0, -1}; // First row interval is contained by first column interval
        
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
        rowIntervalSeries->addEvent(TimeFrameIndex(15000), TimeFrameIndex(16000)); // Interval 15000-16000
        
        dataManager.setData<DigitalIntervalSeries>("RowIntervals", rowIntervalSeries, TimeKey("fine_time"));
        
        // Create second DigitalIntervalSeries (column data) using coarse timeframe
        auto columnIntervalSeries = std::make_shared<DigitalIntervalSeries>();
        columnIntervalSeries->addEvent(TimeFrameIndex(0), TimeFrameIndex(1));    // Interval 0-100
        columnIntervalSeries->addEvent(TimeFrameIndex(5), TimeFrameIndex(7));    // Interval 500-700
        columnIntervalSeries->addEvent(TimeFrameIndex(15), TimeFrameIndex(16));  // Interval 1500-1600
        
        dataManager.setData<DigitalIntervalSeries>("ColumnIntervals", columnIntervalSeries, TimeKey("coarse_time"));
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create row selector from the row interval series
        auto rowIntervalSource = dataManagerExtension->getIntervalSource("RowIntervals");
        REQUIRE(rowIntervalSource != nullptr);
        
        // Get the intervals from the row source
        auto rowIntervals = rowIntervalSource->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(30000), fineTimeFrame.get());
        
        REQUIRE(rowIntervals.size() == 3);
        
        // Convert to TimeFrameIntervals for the selector
        std::vector<TimeFrameInterval> timeFrameIntervals;
        for (const auto& interval : rowIntervals) {
            timeFrameIntervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        
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
        std::vector<int64_t> expectedContainingID = {2, -1, -1}; // No row intervals are contained by any column intervals

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
