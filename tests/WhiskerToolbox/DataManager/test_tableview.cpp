#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "DataManager.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/computers/IntervalReductionComputer.h"
#include "utils/TableView/computers/AnalogSliceGathererComputer.h"
#include "utils/TableView/computers/StandardizeComputer.h"
#include "Points/Point_Data.hpp"
#include "TimeFrame.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>
#include <vector>

TEST_CASE("TableView Point Data Integration Test", "[TableView][Integration]") {
    
    SECTION("Extract X and Y components from Point Data") {
        // Create a DataManager instance
        DataManager dataManager;
        
        // Create sample point data for multiple time frames
        std::vector<std::vector<Point2D<float>>> pointFrames = {
            {{1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f}},     // Frame 0
            {{7.0f, 8.0f}, {9.0f, 10.0f}, {11.0f, 12.0f}},  // Frame 1
            {{13.0f, 14.0f}, {15.0f, 16.0f}, {17.0f, 18.0f}}, // Frame 2
            {{19.0f, 20.0f}, {21.0f, 22.0f}, {23.0f, 24.0f}}  // Frame 3
        };
        
        // Create a TimeFrame for the point data
        std::vector<int> timeValues = {0, 1, 2, 3};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime("test_time", timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();

        for (size_t frameIdx = 0; frameIdx < pointFrames.size(); ++frameIdx) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(frameIdx));
            for (const auto& point : pointFrames[frameIdx]) {
                pointData->addAtTime(timeIndex,point);
            }
        }
        
        // Add the point data to the DataManager
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
        // Create DataManagerExtension for TableView
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals for each time frame
        // Since PointComponentAdapter flattens all points from all time frames into a single array,
        // we need to create intervals that correspond to the array indices, not time frame indices
        // Frame 0: points at array indices 0, 1, 2
        // Frame 1: points at array indices 3, 4, 5  
        // Frame 2: points at array indices 6, 7, 8
        // Frame 3: points at array indices 9, 10, 11
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),  // Frame 0: indices 0-2
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)),  // Frame 1: indices 3-5
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)),  // Frame 2: indices 6-8
            TimeFrameInterval(TimeFrameIndex(9), TimeFrameIndex(11))  // Frame 3: indices 9-11
        };
        auto rowSelector = std::make_unique<IntervalSelector>(intervals);
        
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
        auto xValues = table.getColumnSpan("X_Values");
        auto yValues = table.getColumnSpan("Y_Values");
        
        // Verify the extracted values match the original data
        // Since we're using IntervalSelector with single time indices, each "interval" contains all points
        // at that time index, and we're taking the mean, so we should get the mean of all points at each time
        
        // Expected values: mean of all points at each time index
        // Frame 0: (1.0 + 3.0 + 5.0) / 3 = 3.0, (2.0 + 4.0 + 6.0) / 3 = 4.0
        // Frame 1: (7.0 + 9.0 + 11.0) / 3 = 9.0, (8.0 + 10.0 + 12.0) / 3 = 10.0
        // Frame 2: (13.0 + 15.0 + 17.0) / 3 = 15.0, (14.0 + 16.0 + 18.0) / 3 = 16.0
        // Frame 3: (19.0 + 21.0 + 23.0) / 3 = 21.0, (20.0 + 22.0 + 24.0) / 3 = 22.0
        std::vector<double> expectedX = {3.0, 9.0, 15.0, 21.0};
        std::vector<double> expectedY = {4.0, 10.0, 16.0, 22.0};
        
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
        
        // Create sample point data with more points per frame
        std::vector<std::vector<Point2D<float>>> pointFrames = {
            {{1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f}},     // Frame 0: mean X=3.0, Y=4.0
            {{7.0f, 8.0f}, {9.0f, 10.0f}, {11.0f, 12.0f}},  // Frame 1: mean X=9.0, Y=10.0
            {{13.0f, 14.0f}, {15.0f, 16.0f}, {17.0f, 18.0f}}, // Frame 2: mean X=15.0, Y=16.0
        };
        
        // Create a TimeFrame
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime("test_time", timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();

        for (size_t frameIdx = 0; frameIdx < pointFrames.size(); ++frameIdx) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(frameIdx));
            for (const auto& point : pointFrames[frameIdx]) {
                pointData->addAtTime(timeIndex, point);
            }
        }
        
        // Add the point data to the DataManager
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
        // Create DataManagerExtension for TableView
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create intervals that span each frame
        // Frame 0: points at array indices 0, 1, 2
        // Frame 1: points at array indices 3, 4, 5
        // Frame 2: points at array indices 6, 7, 8
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),  // Frame 0: indices 0-2
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)),  // Frame 1: indices 3-5
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8))   // Frame 2: indices 6-8
        };
        auto rowSelector = std::make_unique<IntervalSelector>(intervals);
        
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
        auto xMean = table.getColumnSpan("X_Mean");
        auto yMean = table.getColumnSpan("Y_Mean");
        auto xMax = table.getColumnSpan("X_Max");
        auto yMax = table.getColumnSpan("Y_Max");
        
        // Verify the computed means
        // Frame 0: (1.0 + 3.0 + 5.0) / 3 = 3.0, (2.0 + 4.0 + 6.0) / 3 = 4.0
        // Frame 1: (7.0 + 9.0 + 11.0) / 3 = 9.0, (8.0 + 10.0 + 12.0) / 3 = 10.0
        // Frame 2: (13.0 + 15.0 + 17.0) / 3 = 15.0, (14.0 + 16.0 + 18.0) / 3 = 16.0
        std::vector<double> expectedXMean = {3.0, 9.0, 15.0};
        std::vector<double> expectedYMean = {4.0, 10.0, 16.0};
        // Max values from each frame
        std::vector<double> expectedXMax = {5.0, 11.0, 17.0};
        std::vector<double> expectedYMax = {6.0, 12.0, 18.0};
        
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
        dataManager.setTime("test_time", timeFrame);
        
        // Create PointData
        auto pointData = std::make_shared<PointData>();

        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i]);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
        // Create DataManagerExtension
        auto dataManagerExtension = std::make_shared<DataManagerExtension>(dataManager);
        
        // Create TableView
        TableViewBuilder builder(dataManagerExtension);
        // For 2 points, they will be at array indices 0 and 1
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0)),  // Single point at index 0
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1))   // Single point at index 1
        };
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));
        
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        builder.addColumn("X_Values", 
            std::make_unique<IntervalReductionComputer>(xSource, ReductionType::Mean));
        
        TableView table = builder.build();
        
        // First access should trigger computation
        auto xValues1 = table.getColumnSpan("X_Values");
        REQUIRE(xValues1.size() == 2);
        REQUIRE(xValues1[0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(xValues1[1] == Catch::Approx(3.0).epsilon(0.001));
        
        // Second access should use cached data (same values)
        auto xValues2 = table.getColumnSpan("X_Values");
        REQUIRE(xValues2.size() == 2);
        REQUIRE(xValues2[0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(xValues2[1] == Catch::Approx(3.0).epsilon(0.001));
        
        // The spans should point to the same cached data
        REQUIRE(xValues1.data() == xValues2.data());
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
        dataManager.setTime("test_time", timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();
        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i]);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
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
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));
        
        // Get analog sources for X and Y components
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        auto ySource = dataManagerExtension->getAnalogSource("TestPoints.y");
        
        REQUIRE(xSource != nullptr);
        REQUIRE(ySource != nullptr);
        
        // Add columns for gathering X and Y slices
        builder.addColumn<std::vector<double>>("X_Slices", 
            std::make_unique<AnalogSliceGathererComputer<double>>(xSource));
        builder.addColumn<std::vector<double>>("Y_Slices", 
            std::make_unique<AnalogSliceGathererComputer<double>>(ySource));
        
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
        dataManager.setTime("test_time", timeFrame);
        
        // Create PointData and add points
        auto pointData = std::make_shared<PointData>();
        for (size_t i = 0; i < points.size(); ++i) {
            TimeFrameIndex timeIndex(static_cast<int64_t>(i));
            pointData->addAtTime(timeIndex, points[i]);
        }
        
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
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
        builder.setRowSelector(std::make_unique<IntervalSelector>(intervals));
        
        // Get analog source for X component
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        REQUIRE(xSource != nullptr);
        
        // Add column for gathering X slices
        builder.addColumn<std::vector<double>>("X_Slices", 
            std::make_unique<AnalogSliceGathererComputer<double>>(xSource));
        
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
        REQUIRE_THROWS_AS(AnalogSliceGathererComputer<double>(nullptr), std::invalid_argument);
        
        // Test with valid source but invalid ExecutionPlan (no intervals)
        std::vector<Point2D<float>> points = {{1.0f, 2.0f}};
        std::vector<int> timeValues = {0};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        dataManager.setTime("test_time", timeFrame);
        
        auto pointData = std::make_shared<PointData>();
        pointData->addAtTime(TimeFrameIndex(0), points[0]);
        dataManager.setData<PointData>("TestPoints", pointData);
        dataManager.setTimeFrame("TestPoints", "test_time");
        
        auto xSource = dataManagerExtension->getAnalogSource("TestPoints.x");
        REQUIRE(xSource != nullptr);
        
        auto computer = std::make_unique<AnalogSliceGathererComputer<double>>(xSource);
        
        // Create an ExecutionPlan with indices instead of intervals
        ExecutionPlan planWithIndices(std::vector<TimeFrameIndex>{TimeFrameIndex(0)});
        
        // This should throw because the computer expects intervals
        REQUIRE_THROWS_AS(computer->compute(planWithIndices), std::invalid_argument);
    }
}

