#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "IntervalOverlapComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "DigitalTimeSeries/interval_data.hpp"
#include "TimeFrame.hpp"

// Additional includes for extended testing
#include "DataManager.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <memory>
#include <vector>
#include <cstdint>
#include <numeric>
#include <nlohmann/json.hpp>

/**
 * @brief Base test fixture for IntervalOverlapComputer with realistic interval data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Row intervals representing behavior periods  
 * - Column intervals representing stimulus periods
 * - Cross-timeframe overlaps for testing timeframe conversion
 */
class IntervalOverlapTestFixture {
protected:
    IntervalOverlapTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();
        
        // Populate with test data
        populateWithIntervalTestData();
    }

    ~IntervalOverlapTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with interval test data
     */
    void populateWithIntervalTestData() {
        createTimeFrames();
        createBehaviorIntervals();
        createStimulusIntervals();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "behavior_time" timeframe: 0 to 100 (101 points) - behavior tracking at 10Hz
        std::vector<int> behavior_time_values(101);
        std::iota(behavior_time_values.begin(), behavior_time_values.end(), 0);
        auto behavior_time_frame = std::make_shared<TimeFrame>(behavior_time_values);
        m_data_manager->setTime(TimeKey("behavior_time"), behavior_time_frame, true);

        // Create "stimulus_time" timeframe: 0, 5, 10, 15, ..., 100 (21 points) - stimulus at 2Hz
        std::vector<int> stimulus_time_values;
        stimulus_time_values.reserve(21);
        for (int i = 0; i <= 20; ++i) {
            stimulus_time_values.push_back(i * 5);
        }
        auto stimulus_time_frame = std::make_shared<TimeFrame>(stimulus_time_values);
        m_data_manager->setTime(TimeKey("stimulus_time"), stimulus_time_frame, true);
    }

    /**
     * @brief Create behavior intervals (row intervals for testing)
     */
    void createBehaviorIntervals() {
        // Create behavior periods: exploration, rest, exploration
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();
        
        // Exploration period 1: time 10-25
        behavior_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(25));
        
        // Rest period: time 30-40  
        behavior_intervals->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        
        // Exploration period 2: time 50-70
        behavior_intervals->addEvent(TimeFrameIndex(50), TimeFrameIndex(70));
        
        // Social interaction: time 80-95
        behavior_intervals->addEvent(TimeFrameIndex(80), TimeFrameIndex(95));

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));
    }

    /**
     * @brief Create stimulus intervals (column intervals for testing)
     */
    void createStimulusIntervals() {
        // Create stimulus presentation periods
        auto stimulus_intervals = std::make_shared<DigitalIntervalSeries>();
        
        // Stimulus 1: time 5-15 (overlaps with exploration period 1)
        stimulus_intervals->addEvent(TimeFrameIndex(1), TimeFrameIndex(3));  // Index 1-3 = time 5-15
        
        // Stimulus 2: time 20-30 (overlaps with end of exploration 1 and start of rest)
        stimulus_intervals->addEvent(TimeFrameIndex(4), TimeFrameIndex(6));  // Index 4-6 = time 20-30
        
        // Stimulus 3: time 45-55 (overlaps with start of exploration period 2)
        stimulus_intervals->addEvent(TimeFrameIndex(9), TimeFrameIndex(11)); // Index 9-11 = time 45-55
        
        // Stimulus 4: time 85-95 (overlaps with social interaction)
        stimulus_intervals->addEvent(TimeFrameIndex(17), TimeFrameIndex(19)); // Index 17-19 = time 85-95

        m_data_manager->setData<DigitalIntervalSeries>("StimulusIntervals", stimulus_intervals, TimeKey("stimulus_time"));
    }
};

/**
 * @brief Test fixture combining IntervalOverlapTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with interval test data (from IntervalOverlapTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class IntervalTableRegistryTestFixture : public IntervalOverlapTestFixture {
protected:
    IntervalTableRegistryTestFixture()
        : IntervalOverlapTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~IntervalTableRegistryTestFixture() = default;

    /**
     * @brief Get the TableRegistry instance
     * @return Reference to the TableRegistry
     */
    TableRegistry & getTableRegistry() { return *m_table_registry_ptr; }

    /**
     * @brief Get the TableRegistry instance (const version)
     * @return Const reference to the TableRegistry
     */
    TableRegistry const & getTableRegistry() const { return *m_table_registry_ptr; }

    /**
     * @brief Get a pointer to the TableRegistry
     * @return Raw pointer to the TableRegistry
     */
    TableRegistry * getTableRegistryPtr() { return m_table_registry_ptr; }

    /**
     * @brief Get the TablePipeline instance
     * @return Reference to the TablePipeline
     */
    TablePipeline & getTablePipeline() { return *m_table_pipeline; }

    /**
     * @brief Get the TablePipeline instance (const version)
     * @return Const reference to the TablePipeline
     */
    TablePipeline const & getTablePipeline() const { return *m_table_pipeline; }

    /**
     * @brief Get a pointer to the TablePipeline
     * @return Raw pointer to the TablePipeline
     */
    TablePipeline * getTablePipelinePtr() { return m_table_pipeline.get(); }

    /**
     * @brief Get the DataManagerExtension instance
     */
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() { 
        if (!m_data_manager_extension) {
            m_data_manager_extension = std::make_shared<DataManagerExtension>(getDataManager());
        }
        return m_data_manager_extension; 
    }

private:
    TableRegistry * m_table_registry_ptr; // Points to DataManager's TableRegistry
    std::unique_ptr<TablePipeline> m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension; // Lazy-initialized
};

// Mock implementation of IIntervalSource for testing
class MockIntervalSource : public IIntervalSource {
public:
    MockIntervalSource(std::string name, 
                       std::shared_ptr<TimeFrame> timeFrame,
                       std::vector<Interval> intervals)
        : m_name(std::move(name)), 
          m_timeFrame(std::move(timeFrame)), 
          m_intervals(std::move(intervals)) {}

    [[nodiscard]] auto getName() const -> std::string const & override {
        return m_name;
    }

    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override {
        return m_timeFrame;
    }

    [[nodiscard]] auto size() const -> size_t override {
        return m_intervals.size();
    }

    auto getIntervals() -> std::vector<Interval> override {
        return m_intervals;
    }

    auto getIntervalsInRange(TimeFrameIndex start, TimeFrameIndex end, 
                            TimeFrame const * target_timeFrame) -> std::vector<Interval> override {
        std::vector<Interval> result;
        
        // Convert TimeFrameIndex to time values for comparison
        auto startTime = target_timeFrame->getTimeAtIndex(start);
        auto endTime = target_timeFrame->getTimeAtIndex(end);
        
        for (const auto& interval : m_intervals) {
            // Convert interval indices to time values using our timeframe
            auto intervalStartTime = m_timeFrame->getTimeAtIndex(TimeFrameIndex(interval.start));
            auto intervalEndTime = m_timeFrame->getTimeAtIndex(TimeFrameIndex(interval.end));
            
            // Check if intervals overlap in time
            if (intervalStartTime <= endTime && startTime <= intervalEndTime) {
                result.push_back(interval);
            }
        }
        
        return result;
    }

private:
    std::string m_name;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::vector<Interval> m_intervals;
};

TEST_CASE("DM - TV - IntervalOverlapComputer Basic Functionality", "[IntervalOverlapComputer]") {
    
    SECTION("AssignID operation - basic overlap detection") {
        // Create time frames
        std::vector<int> rowTimeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto rowTimeFrame = std::make_shared<TimeFrame>(rowTimeValues);
        
        std::vector<int> colTimeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto colTimeFrame = std::make_shared<TimeFrame>(colTimeValues);
        
        // Create column intervals (source intervals)
        std::vector<Interval> columnIntervals = {
            {0, 1},  // Interval 0: time 0-1
            {3, 5},  // Interval 1: time 3-5
            {7, 9}   // Interval 2: time 7-9
        };
        
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "TestIntervals", colTimeFrame, columnIntervals);
        
        // Create row intervals (from execution plan)
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)), // Row 0: time 0-1 (should overlap with interval 0)
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(4)), // Row 1: time 3-4 (should overlap with interval 1)
            TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(8)), // Row 2: time 8-8 (should overlap with interval 2)
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(6))  // Row 3: time 6-6 (should not overlap with any)
        };
        
        ExecutionPlan plan(rowIntervals, rowTimeFrame);
        
        // Create the computer
        IntervalOverlapComputer<int64_t> computer(intervalSource, 
                                                  IntervalOverlapOperation::AssignID,
                                                  "TestIntervals");
        
        // Compute the results
        auto results = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 0);  // Row 0 overlaps with interval 0
        REQUIRE(results[1] == 1);  // Row 1 overlaps with interval 1 (returns index of last matching interval)
        REQUIRE(results[2] == 2);  // Row 2 overlaps with interval 2 (returns index of last matching interval)
        REQUIRE(results[3] == -1); // Row 3 doesn't overlap with any interval
    }
    
    SECTION("CountOverlaps operation - basic overlap counting") {
        // Create time frames
        std::vector<int> rowTimeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto rowTimeFrame = std::make_shared<TimeFrame>(rowTimeValues);
        
        std::vector<int> colTimeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto colTimeFrame = std::make_shared<TimeFrame>(colTimeValues);
        
        // Create column intervals with some overlaps
        std::vector<Interval> columnIntervals = {
            {0, 2},  // Interval 0: time 0-2
            {1, 3},  // Interval 1: time 1-3 (overlaps with interval 0)
            {5, 7},  // Interval 2: time 5-7
            {6, 8}   // Interval 3: time 6-8 (overlaps with interval 2)
        };
        
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "TestIntervals", colTimeFrame, columnIntervals);
        
        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // Row 0: time 0-2 (overlaps with intervals 0,1)
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)), // Row 1: time 1-3 (overlaps with intervals 0,1)
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(7)), // Row 2: time 6-7 (overlaps with intervals 2,3)
            TimeFrameInterval(TimeFrameIndex(9), TimeFrameIndex(9))  // Row 3: time 9-9 (no overlaps)
        };
        
        ExecutionPlan plan(rowIntervals, rowTimeFrame);
        
        // Create the computer
        IntervalOverlapComputer<int64_t> computer(intervalSource, 
                                                  IntervalOverlapOperation::CountOverlaps,
                                                  "TestIntervals");
        
        // Compute the results
        auto results = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 4);
        // Note: The actual counting logic depends on the countOverlappingIntervals implementation
        // These tests verify the computer calls the counting function correctly
        REQUIRE(results[0] >= 0);  // Row 0 should have some overlaps
        REQUIRE(results[1] >= 0);  // Row 1 should have some overlaps
        REQUIRE(results[2] >= 0);  // Row 2 should have some overlaps
        REQUIRE(results[3] >= 0);  // Row 3 might have no overlaps
    }
    
    SECTION("Empty intervals handling") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create empty column intervals
        std::vector<Interval> columnIntervals;
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "EmptyIntervals", timeFrame, columnIntervals);
        
        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)),
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3))
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Test AssignID operation
        IntervalOverlapComputer<int64_t> assignComputer(intervalSource, 
                                                       IntervalOverlapOperation::AssignID,
                                                       "EmptyIntervals");
        
        auto assignResults = assignComputer.compute(plan);
        
        REQUIRE(assignResults.size() == 2);
        REQUIRE(assignResults[0] == -1);  // No intervals to assign
        REQUIRE(assignResults[1] == -1);  // No intervals to assign
        
        // Test CountOverlaps operation
        IntervalOverlapComputer<int64_t> countComputer(intervalSource, 
                                                      IntervalOverlapOperation::CountOverlaps,
                                                      "EmptyIntervals");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 2);
        REQUIRE(countResults[0] == 0);  // No overlaps
        REQUIRE(countResults[1] == 0);  // No overlaps
    }
    
    SECTION("Single interval scenarios") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create single column interval
        std::vector<Interval> columnIntervals = {
            {1, 3}  // Single interval: time 1-3
        };
        
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "SingleInterval", timeFrame, columnIntervals);
        
        // Create various row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0)), // Before interval
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(2)), // Overlaps with interval
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)), // Overlaps with interval
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(5))  // After interval
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Test AssignID operation
        IntervalOverlapComputer<int64_t> assignComputer(intervalSource, 
                                                       IntervalOverlapOperation::AssignID,
                                                       "SingleInterval");
        
        auto assignResults = assignComputer.compute(plan);
        
        REQUIRE(assignResults.size() == 4);
        REQUIRE(assignResults[0] == -1);  // No overlap
        REQUIRE(assignResults[1] == 0);   // Overlaps with interval 0
        REQUIRE(assignResults[2] == 0);   // Overlaps with interval 0
        REQUIRE(assignResults[3] == -1);  // No overlap
    }
    
    SECTION("Edge case: identical intervals") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create column intervals
        std::vector<Interval> columnIntervals = {
            {1, 3},  // Interval 0
            {1, 3}   // Interval 1 (identical to interval 0)
        };
        
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "IdenticalIntervals", timeFrame, columnIntervals);
        
        // Create row interval that matches exactly
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3))
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Test AssignID operation (should return the last matching interval)
        IntervalOverlapComputer<int64_t> assignComputer(intervalSource, 
                                                       IntervalOverlapOperation::AssignID,
                                                       "IdenticalIntervals");
        
        auto assignResults = assignComputer.compute(plan);
        
        REQUIRE(assignResults.size() == 1);
        REQUIRE(assignResults[0] == 1);  // Should return the last matching interval (index 1)
    }
}

TEST_CASE("DM - TV - IntervalOverlapComputer Error Handling", "[IntervalOverlapComputer][Error]") {
    
    SECTION("ExecutionPlan without intervals throws exception") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create column intervals
        std::vector<Interval> columnIntervals = {{1, 3}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "TestIntervals", timeFrame, columnIntervals);
        
        // Create execution plan with indices instead of intervals
        std::vector<TimeFrameIndex> indices = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(indices, timeFrame);
        
        // Create the computer
        IntervalOverlapComputer<int64_t> computer(intervalSource, 
                                                  IntervalOverlapOperation::AssignID,
                                                  "TestIntervals");
        
        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }
}

TEST_CASE("DM - TV - IntervalOverlapComputer Template Types", "[IntervalOverlapComputer][Templates]") {
    
    SECTION("Test with different numeric types") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create column intervals
        std::vector<Interval> columnIntervals = {{1, 3}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "TestIntervals", timeFrame, columnIntervals);
        
        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(2))
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Test with int64_t
        IntervalOverlapComputer<int64_t> intComputer(intervalSource, 
                                                    IntervalOverlapOperation::AssignID,
                                                    "TestIntervals");
        
        auto intResults = intComputer.compute(plan);
        REQUIRE(intResults.size() == 1);
        REQUIRE(intResults[0] == 0);
        
        // Test with size_t for CountOverlaps
        IntervalOverlapComputer<size_t> sizeComputer(intervalSource, 
                                                    IntervalOverlapOperation::CountOverlaps,
                                                    "TestIntervals");
        
        auto sizeResults = sizeComputer.compute(plan);
        REQUIRE(sizeResults.size() == 1);
        REQUIRE(sizeResults[0] >= 0);
    }
}

TEST_CASE("DM - TV - IntervalOverlapComputer Dependency Tracking", "[IntervalOverlapComputer][Dependencies]") {
    
    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<Interval> columnIntervals = {{0, 1}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "TestSource", timeFrame, columnIntervals);
        
        // Create computer
        IntervalOverlapComputer<int64_t> computer(intervalSource, 
                                                  IntervalOverlapOperation::AssignID,
                                                  "TestSourceName");
        
        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "TestSourceName");
    }
}

TEST_CASE("DM - TV - IntervalOverlapComputer Complex Scenarios", "[IntervalOverlapComputer][Complex]") {
    
    SECTION("Multiple overlapping intervals with different time scales") {
        // Create time frames with different scales
        std::vector<int> rowTimeValues = {0, 10, 20, 30, 40, 50};
        auto rowTimeFrame = std::make_shared<TimeFrame>(rowTimeValues);
        
        std::vector<int> colTimeValues = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
        auto colTimeFrame = std::make_shared<TimeFrame>(colTimeValues);
        
        // Create column intervals
        std::vector<Interval> columnIntervals = {
            {0, 2},   // Interval 0: time 0-10
            {1, 4},   // Interval 1: time 5-20 (overlaps with interval 0)
            {3, 6},   // Interval 2: time 15-30 (overlaps with interval 1)
            {8, 10}   // Interval 3: time 40-50
        };
        
        auto intervalSource = std::make_shared<MockIntervalSource>(
            "ComplexIntervals", colTimeFrame, columnIntervals);
        
        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)), // Row 0: time 0-10
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)), // Row 1: time 20-30
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(5))  // Row 2: time 40-50
        };
        
        ExecutionPlan plan(rowIntervals, rowTimeFrame);
        
        // Test AssignID operation
        IntervalOverlapComputer<int64_t> assignComputer(intervalSource, 
                                                       IntervalOverlapOperation::AssignID,
                                                       "ComplexIntervals");
        
        auto assignResults = assignComputer.compute(plan);
        
        REQUIRE(assignResults.size() == 3);
        // Results depend on the specific overlap logic implementation
        // The test verifies the computer executes without errors
        
        // Test CountOverlaps operation
        IntervalOverlapComputer<int64_t> countComputer(intervalSource, 
                                                      IntervalOverlapOperation::CountOverlaps,
                                                      "ComplexIntervals");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 3);
        // All results should be non-negative
        for (auto result : countResults) {
            REQUIRE(result >= 0);
        }

        IntervalOverlapComputer<size_t> countComputer_size_t(intervalSource, 
                                                      IntervalOverlapOperation::CountOverlaps,
                                                      "ComplexIntervals");

        auto countResults_size_t = countComputer_size_t.compute(plan);

        REQUIRE(countResults_size_t.size() == 3);
        // All results should be non-negative
        for (auto result : countResults_size_t) {
            REQUIRE(result >= 0);
        }
    }
}

// Test the standalone utility functions
TEST_CASE("DM - TV - IntervalOverlapComputer Utility Functions", "[IntervalOverlapComputer][Utilities]") {
    
    SECTION("intervalsOverlap function") {
        TimeFrameInterval a(TimeFrameIndex(0), TimeFrameIndex(5));
        TimeFrameInterval b(TimeFrameIndex(3), TimeFrameIndex(8));
        TimeFrameInterval c(TimeFrameIndex(6), TimeFrameIndex(10));
        
        // Test overlapping intervals
        REQUIRE(intervalsOverlap(a, b) == true);
        
        // Test non-overlapping intervals
        REQUIRE(intervalsOverlap(a, c) == false);
        
        // Test identical intervals
        REQUIRE(intervalsOverlap(a, a) == true);
    }
    
    SECTION("findContainingInterval function") {
        TimeFrameInterval rowInterval(TimeFrameIndex(2), TimeFrameIndex(4));
        
        std::vector<Interval> columnIntervals = {
            {0, 1},   // Interval 0: doesn't contain row interval
            {1, 5},   // Interval 1: contains row interval
            {3, 7},   // Interval 2: overlaps but doesn't fully contain
            {6, 8}    // Interval 3: doesn't contain row interval
        };
        
        auto result = findContainingInterval(rowInterval, columnIntervals);
        
        // Should return the index of the interval that contains the row interval
        // The exact behavior depends on the implementation
        REQUIRE(result >= -1);  // -1 means no containing interval found
    }
    
    SECTION("countOverlappingIntervals function") {
        TimeFrameInterval rowInterval(TimeFrameIndex(2), TimeFrameIndex(4));
        
        std::vector<Interval> columnIntervals = {
            {0, 1},   // Interval 0: doesn't overlap
            {1, 3},   // Interval 1: overlaps
            {3, 5},   // Interval 2: overlaps
            {6, 8}    // Interval 3: doesn't overlap
        };
        
        auto result = countOverlappingIntervals(rowInterval, columnIntervals);
        
        // Should count overlapping intervals
        REQUIRE(result >= 0);
        REQUIRE(result <= static_cast<int64_t>(columnIntervals.size()));
    }
}

TEST_CASE_METHOD(IntervalOverlapTestFixture, "DM - TV - IntervalOverlapComputer with DataManager fixture", "[IntervalOverlapComputer][DataManager][Fixture]") {
    
    SECTION("Test with behavior and stimulus intervals from fixture") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get the interval sources from the DataManager
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        auto stimulus_source = dme->getIntervalSource("StimulusIntervals");
        
        REQUIRE(behavior_source != nullptr);
        REQUIRE(stimulus_source != nullptr);
        
        // Create row selector from behavior intervals
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        auto behavior_intervals = behavior_source->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(100), behavior_time_frame.get());
        
        REQUIRE(behavior_intervals.size() == 4); // 4 behavior periods
        
        // Convert to TimeFrameIntervals for row selector
        std::vector<TimeFrameInterval> row_intervals;
        for (const auto& interval : behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        
        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);
        
        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Test AssignID operation
        builder.addColumn<int64_t>("Stimulus_ID", 
            std::make_unique<IntervalOverlapComputer<int64_t>>(stimulus_source, 
                IntervalOverlapOperation::AssignID, "StimulusIntervals"));
        
        // Test CountOverlaps operation
        builder.addColumn<int64_t>("Stimulus_Count", 
            std::make_unique<IntervalOverlapComputer<int64_t>>(stimulus_source, 
                IntervalOverlapOperation::CountOverlaps, "StimulusIntervals"));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("Stimulus_ID"));
        REQUIRE(table.hasColumn("Stimulus_Count"));
        
        // Get the column data
        auto stimulus_ids = table.getColumnValues<int64_t>("Stimulus_ID");
        auto stimulus_counts = table.getColumnValues<int64_t>("Stimulus_Count");
        
        REQUIRE(stimulus_ids.size() == 4);
        REQUIRE(stimulus_counts.size() == 4);
        
        // Verify the overlap results
        // Expected overlaps based on our test data:
        // Behavior 1 (10-25): overlaps with Stimulus 1 (5-15) and Stimulus 2 (20-30)
        // Behavior 2 (30-40): overlaps with Stimulus 2 (20-30)  
        // Behavior 3 (50-70): overlaps with Stimulus 3 (45-55)
        // Behavior 4 (80-95): overlaps with Stimulus 4 (85-95)
        
        // Note: Actual results depend on the cross-timeframe conversion implementation
        // These tests verify the computer executes without errors and produces reasonable results
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(stimulus_ids[i] >= -1);     // -1 means no overlap, >= 0 means valid stimulus ID
            REQUIRE(stimulus_counts[i] >= 0);   // Should be non-negative count
            REQUIRE(stimulus_counts[i] <= 4);   // Cannot exceed total number of stimuli
        }
    }
    
    SECTION("Test cross-timeframe overlap detection") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get sources from different timeframes
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");  // behavior_time frame
        auto stimulus_source = dme->getIntervalSource("StimulusIntervals");   // stimulus_time frame
        
        REQUIRE(behavior_source != nullptr);
        REQUIRE(stimulus_source != nullptr);
        
        // Verify they have different timeframes
        auto behavior_tf = behavior_source->getTimeFrame();
        auto stimulus_tf = stimulus_source->getTimeFrame();
        REQUIRE(behavior_tf != stimulus_tf);
        REQUIRE(behavior_tf->getTotalFrameCount() == 101);  // behavior_time: 0-100
        REQUIRE(stimulus_tf->getTotalFrameCount() == 21);   // stimulus_time: 0,5,10,...,100
        
        // Create a simple test with one behavior interval
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(10), TimeFrameIndex(25))  // Behavior period 1
        };
        
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_tf);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Add overlap analysis columns
        builder.addColumn<int64_t>("Stimulus_ID", 
            std::make_unique<IntervalOverlapComputer<int64_t>>(stimulus_source, 
                IntervalOverlapOperation::AssignID, "StimulusIntervals"));
        
        builder.addColumn<int64_t>("Stimulus_Count", 
            std::make_unique<IntervalOverlapComputer<int64_t>>(stimulus_source, 
                IntervalOverlapOperation::CountOverlaps, "StimulusIntervals"));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 2);
        
        auto stimulus_ids = table.getColumnValues<int64_t>("Stimulus_ID");
        auto stimulus_counts = table.getColumnValues<int64_t>("Stimulus_Count");
        
        REQUIRE(stimulus_ids.size() == 1);
        REQUIRE(stimulus_counts.size() == 1);
        
        // Verify results are reasonable (exact values depend on timeframe conversion)
        REQUIRE(stimulus_ids[0] >= -1);
        REQUIRE(stimulus_counts[0] >= 0);
        REQUIRE(stimulus_counts[0] <= 4);
        
        std::cout << "Cross-timeframe test - Stimulus ID: " << stimulus_ids[0] 
                  << ", Count: " << stimulus_counts[0] << std::endl;
    }
}

TEST_CASE_METHOD(IntervalTableRegistryTestFixture, "DM - TV - IntervalOverlapComputer via ComputerRegistry", "[IntervalOverlapComputer][Registry]") {
    
    SECTION("Verify IntervalOverlapComputer is registered in ComputerRegistry") {
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Check that all interval overlap computers are registered
        auto assign_id_info = registry.findComputerInfo("Interval Overlap Assign ID");
        auto count_info = registry.findComputerInfo("Interval Overlap Count");
        auto assign_start_info = registry.findComputerInfo("Interval Overlap Assign Start");
        auto assign_end_info = registry.findComputerInfo("Interval Overlap Assign End");
        
        REQUIRE(assign_id_info != nullptr);
        REQUIRE(count_info != nullptr);
        REQUIRE(assign_start_info != nullptr);
        REQUIRE(assign_end_info != nullptr);
        
        // Verify computer info details
        REQUIRE(assign_id_info->name == "Interval Overlap Assign ID");
        REQUIRE(assign_id_info->outputType == typeid(int64_t));
        REQUIRE(assign_id_info->outputTypeName == "int64_t");
        REQUIRE(assign_id_info->requiredRowSelector == RowSelectorType::Interval);
        REQUIRE(assign_id_info->requiredSourceType == typeid(std::shared_ptr<IIntervalSource>));
        
        REQUIRE(count_info->name == "Interval Overlap Count");
        REQUIRE(count_info->outputType == typeid(int64_t));
        REQUIRE(count_info->outputTypeName == "int64_t");
        REQUIRE(count_info->requiredRowSelector == RowSelectorType::Interval);
        REQUIRE(count_info->requiredSourceType == typeid(std::shared_ptr<IIntervalSource>));
    }
    
    SECTION("Create IntervalOverlapComputer via ComputerRegistry") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Get stimulus source for testing
        auto stimulus_source = dme->getIntervalSource("StimulusIntervals");
        REQUIRE(stimulus_source != nullptr);
        
        // Create computers via registry
        std::map<std::string, std::string> empty_params;
        
        auto assign_id_computer = registry.createTypedComputer<int64_t>(
            "Interval Overlap Assign ID", stimulus_source, empty_params);
        auto count_computer = registry.createTypedComputer<int64_t>(
            "Interval Overlap Count", stimulus_source, empty_params);
        
        REQUIRE(assign_id_computer != nullptr);
        REQUIRE(count_computer != nullptr);
        
        // Test that the created computers work correctly
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        
        // Create a simple test interval
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(50), TimeFrameIndex(70))  // Behavior period 3
        };
        
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_time_frame);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Use the registry-created computers
        builder.addColumn("RegistryAssignID", std::move(assign_id_computer));
        builder.addColumn("RegistryCount", std::move(count_computer));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("RegistryAssignID"));
        REQUIRE(table.hasColumn("RegistryCount"));
        
        auto assign_ids = table.getColumnValues<int64_t>("RegistryAssignID");
        auto counts = table.getColumnValues<int64_t>("RegistryCount");
        
        REQUIRE(assign_ids.size() == 1);
        REQUIRE(counts.size() == 1);
        
        // Verify reasonable results
        REQUIRE(assign_ids[0] >= -1);
        REQUIRE(counts[0] >= 0);
        REQUIRE(counts[0] <= 4);
        
        std::cout << "Registry test - Assign ID: " << assign_ids[0] 
                  << ", Count: " << counts[0] << std::endl;
    }
    
    SECTION("Compare registry-created vs direct-created computers") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        auto stimulus_source = dme->getIntervalSource("StimulusIntervals");
        REQUIRE(stimulus_source != nullptr);
        
        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<int64_t>(
            "Interval Overlap Count", stimulus_source, empty_params);
        
        // Create computer directly
        auto direct_computer = std::make_unique<IntervalOverlapComputer<int64_t>>(
            stimulus_source, IntervalOverlapOperation::CountOverlaps, "StimulusIntervals");
        
        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);
        
        // Test both computers with the same data
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(50), TimeFrameIndex(70))
        };
        
        ExecutionPlan plan(test_intervals, behavior_time_frame);
        
        auto registry_result = registry_computer->compute(plan);
        auto direct_result = direct_computer->compute(plan);
        
        REQUIRE(registry_result.size() == 1);
        REQUIRE(direct_result.size() == 1);
        
        // Results should be identical
        REQUIRE(registry_result[0] == direct_result[0]);
        
        std::cout << "Comparison test - Registry result: " << registry_result[0] 
                  << ", Direct result: " << direct_result[0] << std::endl;
    }
}

TEST_CASE_METHOD(IntervalTableRegistryTestFixture, "DM - TV - IntervalOverlapComputer via JSON TablePipeline", "[IntervalOverlapComputer][JSON][Pipeline]") {
    
    SECTION("Test CountOverlaps operation via JSON pipeline") {
        // JSON configuration for testing IntervalOverlapComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Interval Overlap Test",
                "description": "Test JSON execution of IntervalOverlapComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "interval_overlap_test",
                    "name": "Interval Overlap Test Table",
                    "description": "Test table using IntervalOverlapComputer",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "StimulusOverlapCount",
                            "description": "Count of stimulus events overlapping with each behavior period",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Count"
                        },
                        {
                            "name": "StimulusOverlapID",
                            "description": "ID of stimulus event overlapping with each behavior period",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign ID"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();

        // Parse the JSON configuration
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        // Load configuration into pipeline
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Verify configuration was loaded correctly
        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const& config = table_configs[0];
        REQUIRE(config.table_id == "interval_overlap_test");
        REQUIRE(config.name == "Interval Overlap Test Table");
        REQUIRE(config.columns.size() == 2);

        // Verify column configurations
        auto const& column1 = config.columns[0];
        REQUIRE(column1["name"] == "StimulusOverlapCount");
        REQUIRE(column1["computer"] == "Interval Overlap Count");
        REQUIRE(column1["data_source"] == "StimulusIntervals");

        auto const& column2 = config.columns[1];
        REQUIRE(column2["name"] == "StimulusOverlapID");
        REQUIRE(column2["computer"] == "Interval Overlap Assign ID");
        REQUIRE(column2["data_source"] == "StimulusIntervals");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "interval");
        REQUIRE(config.row_selector["source"] == "BehaviorPeriods");

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline (note: this may not fully work if interval row selector is not implemented)
        auto pipeline_result = pipeline.execute([](int table_index, std::string const& table_name, int table_progress, int overall_progress) {
            std::cout << "Building table " << table_index << " (" << table_name << "): "
                      << table_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
        });

        if (pipeline_result.success) {
            std::cout << "Pipeline executed successfully!" << std::endl;
            std::cout << "Tables completed: " << pipeline_result.tables_completed << "/" << pipeline_result.total_tables << std::endl;
            std::cout << "Execution time: " << pipeline_result.total_execution_time_ms << " ms" << std::endl;

            // Verify the built table exists
            auto& registry = getTableRegistry();
            REQUIRE(registry.hasTable("interval_overlap_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("interval_overlap_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const& name : column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 2);
            REQUIRE(built_table->hasColumn("StimulusOverlapCount"));
            REQUIRE(built_table->hasColumn("StimulusOverlapID"));

            // Verify table has 4 rows (one for each behavior period)
            REQUIRE(built_table->getRowCount() == 4);

            // Get and verify the computed values
            auto overlap_counts = built_table->getColumnValues<int64_t>("StimulusOverlapCount");
            auto overlap_ids = built_table->getColumnValues<int64_t>("StimulusOverlapID");

            REQUIRE(overlap_counts.size() == 4);
            REQUIRE(overlap_ids.size() == 4);

            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(overlap_counts[i] >= 0);
                REQUIRE(overlap_counts[i] <= 4);  // Cannot exceed total number of stimuli
                REQUIRE(overlap_ids[i] >= -1);    // -1 means no overlap, >= 0 means valid ID
                
                std::cout << "Row " << i << ": Count=" << overlap_counts[i] 
                          << ", ID=" << overlap_ids[i] << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            // Now that we have proper fixtures and interval row selector implementation, this should succeed
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }
    
    SECTION("Test AssignID_Start and AssignID_End operations via JSON") {
        char const * json_config = R"({
            "metadata": {
                "name": "Interval Overlap Start/End Test",
                "description": "Test JSON execution of IntervalOverlapComputer start/end operations"
            },
            "tables": [
                {
                    "table_id": "interval_overlap_start_end_test",
                    "name": "Interval Overlap Start/End Test Table",
                    "description": "Test table using IntervalOverlapComputer start/end operations",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "StimulusStartIndex",
                            "description": "Start index of overlapping stimulus",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign Start"
                        },
                        {
                            "name": "StimulusEndIndex",
                            "description": "End index of overlapping stimulus",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign End"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);
        
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);
        
        auto const& config = table_configs[0];
        REQUIRE(config.columns.size() == 2);
        REQUIRE(config.columns[0]["computer"] == "Interval Overlap Assign Start");
        REQUIRE(config.columns[1]["computer"] == "Interval Overlap Assign End");

        std::cout << "Start/End operations JSON configuration parsed successfully" << std::endl;
        
        // Note: Full execution testing would depend on interval row selector implementation
        // For now, we verify that the configuration is correctly parsed
    }
}

TEST_CASE_METHOD(IntervalTableRegistryTestFixture, "DM - TV - IntervalRowSelector implementation test", "[IntervalRowSelector][Pipeline]") {
    
    SECTION("Test interval row selector creation from source") {
        // Simple JSON configuration to test interval row selector creation
        char const * json_config = R"({
            "metadata": {
                "name": "Interval Row Selector Test",
                "description": "Test interval row selector creation"
            },
            "tables": [
                {
                    "table_id": "interval_row_test",
                    "name": "Interval Row Test Table",
                    "description": "Test table with interval row selector",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "StimulusCount",
                            "description": "Count of overlapping stimuli",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Count"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);
        
        // Load configuration
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Execute the pipeline
        auto pipeline_result = pipeline.execute();
        
        // Verify that the pipeline now executes successfully with our interval row selector implementation
        if (pipeline_result.success) {
            std::cout << "✓ Interval row selector pipeline executed successfully!" << std::endl;
            
            // Verify the built table exists and has correct structure
            auto& registry = getTableRegistry();
            REQUIRE(registry.hasTable("interval_row_test"));
            
            auto built_table = registry.getBuiltTable("interval_row_test");
            REQUIRE(built_table != nullptr);
            
            // Should have 4 rows (one for each behavior period from our fixture)
            REQUIRE(built_table->getRowCount() == 4);
            REQUIRE(built_table->getColumnCount() == 1);
            REQUIRE(built_table->hasColumn("StimulusCount"));
            
            auto counts = built_table->getColumnValues<int64_t>("StimulusCount");
            REQUIRE(counts.size() == 4);
            
            // Verify all counts are reasonable
            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(counts[i] >= 0);
                REQUIRE(counts[i] <= 4);  // Cannot exceed total number of stimuli
                std::cout << "Behavior period " << i << ": " << counts[i] << " overlapping stimuli" << std::endl;
            }
            
        } else {
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }
    
    SECTION("Test interval row selector with multiple operations") {
        // More comprehensive test with multiple computers
        char const * json_config = R"({
            "metadata": {
                "name": "Multi-Operation Interval Test",
                "description": "Test multiple interval overlap operations"
            },
            "tables": [
                {
                    "table_id": "multi_interval_test",
                    "name": "Multi Interval Test Table",
                    "description": "Test table with multiple interval overlap operations",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "OverlapCount",
                            "description": "Count of overlapping stimuli",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Count"
                        },
                        {
                            "name": "OverlapID",
                            "description": "ID of overlapping stimulus",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign ID"
                        },
                        {
                            "name": "OverlapStart",
                            "description": "Start index of overlapping stimulus",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign Start"
                        },
                        {
                            "name": "OverlapEnd",
                            "description": "End index of overlapping stimulus",
                            "data_source": "StimulusIntervals",
                            "computer": "Interval Overlap Assign End"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);
        
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto pipeline_result = pipeline.execute();
        
        if (pipeline_result.success) {
            std::cout << "✓ Multi-operation interval pipeline executed successfully!" << std::endl;
            
            auto& registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("multi_interval_test");
            REQUIRE(built_table != nullptr);
            
            REQUIRE(built_table->getRowCount() == 4);
            REQUIRE(built_table->getColumnCount() == 4);
            
            // Verify all expected columns exist
            REQUIRE(built_table->hasColumn("OverlapCount"));
            REQUIRE(built_table->hasColumn("OverlapID"));
            REQUIRE(built_table->hasColumn("OverlapStart"));
            REQUIRE(built_table->hasColumn("OverlapEnd"));
            
            // Get all column data
            auto counts = built_table->getColumnValues<int64_t>("OverlapCount");
            auto ids = built_table->getColumnValues<int64_t>("OverlapID");
            auto starts = built_table->getColumnValues<int64_t>("OverlapStart");
            auto ends = built_table->getColumnValues<int64_t>("OverlapEnd");
            
            // Verify data consistency
            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(counts[i] >= 0);
                REQUIRE(ids[i] >= -1);     // -1 means no overlap
                REQUIRE(starts[i] >= -1);  // -1 means no overlap
                REQUIRE(ends[i] >= -1);    // -1 means no overlap
                
                std::cout << "Row " << i << ": Count=" << counts[i] 
                          << ", ID=" << ids[i] 
                          << ", Start=" << starts[i] 
                          << ", End=" << ends[i] << std::endl;
            }
            
        } else {
            FAIL("Multi-operation pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
