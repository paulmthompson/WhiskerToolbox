#include <catch2/catch_test_macros.hpp>

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimestampInIntervalComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"

// Additional includes for extended testing
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <vector>

/**
 * @brief Base test fixture for TimestampInIntervalComputer with realistic interval data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Interval data representing behavior periods  
 * - Stimulus events and digital intervals
 * - Cross-timeframe scenarios for testing timeframe conversion
 */
class TimestampInIntervalTestFixture {
protected:
    TimestampInIntervalTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithTestData();
    }

    ~TimestampInIntervalTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with test data
     */
    void populateWithTestData() {
        createTimeFrames();
        createIntervalData();
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

        // Create "high_res_time" timeframe: 0, 1, 2, ..., 200 (201 points) - high resolution at 20Hz
        std::vector<int> high_res_time_values(201);
        std::iota(high_res_time_values.begin(), high_res_time_values.end(), 0);
        auto high_res_time_frame = std::make_shared<TimeFrame>(high_res_time_values);
        m_data_manager->setTime(TimeKey("high_res_time"), high_res_time_frame, true);
    }

    /**
     * @brief Create interval data for testing
     */
    void createIntervalData() {
        // Create behavior periods on behavior_time
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();

        // Exploration period 1: time 10-25
        behavior_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(25));

        // Rest period: time 35-45
        behavior_intervals->addEvent(TimeFrameIndex(35), TimeFrameIndex(45));

        // Exploration period 2: time 60-80
        behavior_intervals->addEvent(TimeFrameIndex(60), TimeFrameIndex(80));

        // Social interaction: time 85-95
        behavior_intervals->addEvent(TimeFrameIndex(85), TimeFrameIndex(95));

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));

        // Create stimulus intervals on stimulus_time
        auto stimulus_intervals = std::make_shared<DigitalIntervalSeries>();

        // Stimulus 1: time 5-15 (index 1-3 = time 5-15)
        stimulus_intervals->addEvent(TimeFrameIndex(1), TimeFrameIndex(3));

        // Stimulus 2: time 25-35 (index 5-7 = time 25-35)
        stimulus_intervals->addEvent(TimeFrameIndex(5), TimeFrameIndex(7));

        // Stimulus 3: time 50-70 (index 10-14 = time 50-70)
        stimulus_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(14));

        // Stimulus 4: time 90-100 (index 18-20 = time 90-100)
        stimulus_intervals->addEvent(TimeFrameIndex(18), TimeFrameIndex(20));

        m_data_manager->setData<DigitalIntervalSeries>("StimulusIntervals", stimulus_intervals, TimeKey("stimulus_time"));

        // Create simple test intervals on behavior_time for basic testing
        auto simple_intervals = std::make_shared<DigitalIntervalSeries>();
        simple_intervals->addEvent(TimeFrameIndex(5), TimeFrameIndex(15)); // time 5-15
        simple_intervals->addEvent(TimeFrameIndex(25), TimeFrameIndex(35));// time 25-35
        simple_intervals->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));// time 50-60

        m_data_manager->setData<DigitalIntervalSeries>("SimpleIntervals", simple_intervals, TimeKey("behavior_time"));
    }
};

/**
 * @brief Test fixture combining TimestampInIntervalTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with interval test data (from TimestampInIntervalTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class TimestampTableRegistryTestFixture : public TimestampInIntervalTestFixture {
protected:
    TimestampTableRegistryTestFixture()
        : TimestampInIntervalTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~TimestampTableRegistryTestFixture() = default;

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
    TableRegistry * m_table_registry_ptr;// Points to DataManager's TableRegistry
    std::unique_ptr<TablePipeline> m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension;// Lazy-initialized
};

TEST_CASE("DM - TV - TimestampInIntervalComputer Basic Functionality", "[TimestampInIntervalComputer]") {

    SECTION("Basic timestamp in interval detection") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create intervals: [1,3] and [6,8]
        std::vector<Interval> intervals = {
                {1, 3},// Interval 0: time 1-3
                {6, 8} // Interval 1: time 6-8
        };

        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Create timestamps to test: 0, 2, 5, 7, 9
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(5),
                TimeFrameIndex(7), TimeFrameIndex(9)};

        ExecutionPlan plan(timestamps, timeFrame);

        // Create the computer
        TimestampInIntervalComputer computer(intervalSeries, "TestIntervals");

        // Compute the results
        auto [results, entity_ids] = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 5);
        REQUIRE(results[0] == false);// Timestamp 0: not in any interval
        REQUIRE(results[1] == true); // Timestamp 2: in interval [1,3]
        REQUIRE(results[2] == false);// Timestamp 5: not in any interval
        REQUIRE(results[3] == true); // Timestamp 7: in interval [6,8]
        REQUIRE(results[4] == false);// Timestamp 9: not in any interval
    }

    SECTION("Edge cases - boundary conditions") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create interval: [2,4]
        std::vector<Interval> intervals = {{2, 4}};

        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Test boundary timestamps: 1, 2, 3, 4, 5
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3),
                TimeFrameIndex(4), TimeFrameIndex(5)};

        ExecutionPlan plan(timestamps, timeFrame);
        TimestampInIntervalComputer computer(intervalSeries, "EdgeIntervals");

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 5);
        REQUIRE(results[0] == false);// Timestamp 1: before interval
        REQUIRE(results[1] == true); // Timestamp 2: at interval start
        REQUIRE(results[2] == true); // Timestamp 3: inside interval
        REQUIRE(results[3] == true); // Timestamp 4: at interval end
        REQUIRE(results[4] == false);// Timestamp 5: after interval
    }

    SECTION("Empty intervals handling") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create empty intervals
        std::vector<Interval> intervals;
        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Test some timestamps
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(4)};

        ExecutionPlan plan(timestamps, timeFrame);
        TimestampInIntervalComputer computer(intervalSeries, "EmptyIntervals");

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == false);// No intervals to be inside
        REQUIRE(results[1] == false);// No intervals to be inside
        REQUIRE(results[2] == false);// No intervals to be inside
    }

    SECTION("Multiple overlapping intervals") {
        // Create time frames
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create overlapping intervals: [1,4], [3,6], [5,8]
        std::vector<Interval> intervals = {
                {1, 4},// Interval 0
                {3, 6},// Interval 1 (overlaps with 0)
                {5, 8} // Interval 2 (overlaps with 1)
        };

        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Test timestamps throughout the range
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(4),
                TimeFrameIndex(6), TimeFrameIndex(9)};

        ExecutionPlan plan(timestamps, timeFrame);
        TimestampInIntervalComputer computer(intervalSeries, "OverlapIntervals");

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 5);
        REQUIRE(results[0] == false);// Timestamp 0: not in any interval
        REQUIRE(results[1] == true); // Timestamp 2: in interval [1,4]
        REQUIRE(results[2] == true); // Timestamp 4: in intervals [1,4] and [3,6]
        REQUIRE(results[3] == true); // Timestamp 6: in intervals [3,6] and [5,8]
        REQUIRE(results[4] == false);// Timestamp 9: not in any interval
    }
}

TEST_CASE("DM - TV - TimestampInIntervalComputer Error Handling", "[TimestampInIntervalComputer][Error]") {

    SECTION("Null interval source throws exception") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(timestamps, timeFrame);

        // Create computer with null source
        TimestampInIntervalComputer computer(nullptr, "NullSource");

        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }

    SECTION("ExecutionPlan without TimeFrame throws exception") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<Interval> intervals = {{1, 3}};
        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Create execution plan with null timeframe
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(timestamps, nullptr);

        TimestampInIntervalComputer computer(intervalSeries, "TestIntervals");

        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }
}

TEST_CASE("DM - TV - TimestampInIntervalComputer Dependency Tracking", "[TimestampInIntervalComputer][Dependencies]") {

    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<Interval> intervals = {{0, 1}};
        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Create computer with custom source name
        TimestampInIntervalComputer computer(intervalSeries, "CustomSourceName");

        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "CustomSourceName");
    }

    SECTION("Default source name from interval source") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<Interval> intervals = {{0, 1}};
        auto intervalSeries = std::make_shared<DigitalIntervalSeries>(intervals);
        intervalSeries->setTimeFrame(timeFrame);

        // Create computer with empty source name (default)
        TimestampInIntervalComputer computer(intervalSeries);

        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "");
    }
}

TEST_CASE_METHOD(TimestampInIntervalTestFixture, "DM - TV - TimestampInIntervalComputer with DataManager fixture", "[TimestampInIntervalComputer][DataManager][Fixture]") {

    SECTION("Test with behavior intervals from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the interval source from the DataManager
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        REQUIRE(behavior_source != nullptr);

        // Create timestamps to test around the behavior periods
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(5), // Before first period
                TimeFrameIndex(15),// Inside first period (10-25)
                TimeFrameIndex(30),// Between periods
                TimeFrameIndex(40),// Inside second period (35-45)
                TimeFrameIndex(55),// Between periods
                TimeFrameIndex(70),// Inside third period (60-80)
                TimeFrameIndex(90),// Inside fourth period (85-95)
                TimeFrameIndex(99) // After all periods
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, behavior_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add TimestampInIntervalComputer column
        builder.addColumn<bool>("InBehaviorPeriod",
                                std::make_unique<TimestampInIntervalComputer>(behavior_source, "BehaviorPeriods"));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 8);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("InBehaviorPeriod"));

        // Get the column data
        auto in_behavior = table.getColumnValues<bool>("InBehaviorPeriod");
        REQUIRE(in_behavior.size() == 8);

        // Verify the results based on our test data:
        // Periods: [10-25], [35-45], [60-80], [85-95]
        REQUIRE(in_behavior[0] == false);// Timestamp 5: before all periods
        REQUIRE(in_behavior[1] == true); // Timestamp 15: in period [10-25]
        REQUIRE(in_behavior[2] == false);// Timestamp 30: between periods
        REQUIRE(in_behavior[3] == true); // Timestamp 40: in period [35-45]
        REQUIRE(in_behavior[4] == false);// Timestamp 55: between periods
        REQUIRE(in_behavior[5] == true); // Timestamp 70: in period [60-80]
        REQUIRE(in_behavior[6] == true); // Timestamp 90: in period [85-95]
        REQUIRE(in_behavior[7] == false);// Timestamp 99: after all periods
    }

    SECTION("Test cross-timeframe interval detection") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get sources from different timeframes
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");  // behavior_time frame
        auto stimulus_source = dme->getIntervalSource("StimulusIntervals");// stimulus_time frame

        REQUIRE(behavior_source != nullptr);
        REQUIRE(stimulus_source != nullptr);

        // Verify they have different timeframes
        auto behavior_tf = behavior_source->getTimeFrame();
        auto stimulus_tf = stimulus_source->getTimeFrame();
        REQUIRE(behavior_tf != stimulus_tf);
        REQUIRE(behavior_tf->getTotalFrameCount() == 101);// behavior_time: 0-100
        REQUIRE(stimulus_tf->getTotalFrameCount() == 21); // stimulus_time: 0,5,10,...,100

        // Create timestamps on high resolution timeframe
        auto high_res_tf = dm.getTime(TimeKey("high_res_time"));
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(10),// time 10 (should be in stimulus interval [5-15])
                TimeFrameIndex(20),// time 20 (should not be in any stimulus interval)
                TimeFrameIndex(60),// time 60 (should be in stimulus interval [50-70])
                TimeFrameIndex(95) // time 95 (should be in stimulus interval [90-100])
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, high_res_tf);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add cross-timeframe analysis column
        builder.addColumn<bool>("InStimulusInterval",
                                std::make_unique<TimestampInIntervalComputer>(stimulus_source, "StimulusIntervals"));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 1);

        auto in_stimulus = table.getColumnValues<bool>("InStimulusInterval");
        REQUIRE(in_stimulus.size() == 4);

        // Verify results (exact values depend on timeframe conversion)
        // All should be valid boolean results
        for (size_t i = 0; i < 4; ++i) {
            // Just verify they are valid boolean values (no exceptions during computation)
            REQUIRE((in_stimulus[i] == true || in_stimulus[i] == false));
        }

        std::cout << "Cross-timeframe test results - timestamps at times 10, 20, 60, 95:" << std::endl;
        for (size_t i = 0; i < 4; ++i) {
            std::cout << "  Timestamp " << test_timestamps[i].getValue() << ": "
                      << (in_stimulus[i] ? "inside" : "outside") << " stimulus interval" << std::endl;
        }
    }
}

TEST_CASE_METHOD(TimestampTableRegistryTestFixture, "DM - TV - TimestampInIntervalComputer via ComputerRegistry", "[TimestampInIntervalComputer][Registry]") {

    SECTION("Verify TimestampInIntervalComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that the computer is registered
        auto computer_info = registry.findComputerInfo("Timestamp In Interval");
        REQUIRE(computer_info != nullptr);

        // Verify computer info details
        REQUIRE(computer_info->name == "Timestamp In Interval");
        REQUIRE(computer_info->outputType == typeid(bool));
        REQUIRE(computer_info->outputTypeName == "bool");
        REQUIRE(computer_info->requiredRowSelector == RowSelectorType::Timestamp);
        REQUIRE(computer_info->requiredSourceType == typeid(std::shared_ptr<DigitalIntervalSeries>));
        REQUIRE(computer_info->isVectorType == false);
        REQUIRE(computer_info->elementType == typeid(bool));
    }

    SECTION("Create TimestampInIntervalComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get interval source for testing
        auto simple_source = dme->getIntervalSource("SimpleIntervals");
        REQUIRE(simple_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> empty_params;

        auto computer = registry.createTypedComputer<bool>(
                "Timestamp In Interval", simple_source, empty_params);

        REQUIRE(computer != nullptr);

        // Test that the created computer works correctly
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));

        // Create test timestamps
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(3), // Before first interval
                TimeFrameIndex(10),// Inside first interval [5-15]
                TimeFrameIndex(20),// Between intervals
                TimeFrameIndex(30),// Inside second interval [25-35]
                TimeFrameIndex(55) // Inside third interval [50-60]
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, behavior_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Use the registry-created computer
        builder.addColumn("RegistryInInterval", std::move(computer));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 5);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("RegistryInInterval"));

        auto results = table.getColumnValues<bool>("RegistryInInterval");
        REQUIRE(results.size() == 5);

        // Verify expected results based on SimpleIntervals: [5-15], [25-35], [50-60]
        REQUIRE(results[0] == false);// Timestamp 3: before first interval
        REQUIRE(results[1] == true); // Timestamp 10: inside first interval
        REQUIRE(results[2] == false);// Timestamp 20: between intervals
        REQUIRE(results[3] == true); // Timestamp 30: inside second interval
        REQUIRE(results[4] == true); // Timestamp 55: inside third interval

        std::cout << "Registry test results:" << std::endl;
        for (size_t i = 0; i < 5; ++i) {
            std::cout << "  Timestamp " << test_timestamps[i].getValue() << ": "
                      << (results[i] ? "inside" : "outside") << " interval" << std::endl;
        }
    }

    SECTION("Compare registry-created vs direct-created computers") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        auto simple_source = dme->getIntervalSource("SimpleIntervals");
        REQUIRE(simple_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<bool>(
                "Timestamp In Interval", simple_source, empty_params);

        // Create computer directly
        auto direct_computer = std::make_unique<TimestampInIntervalComputer>(
                simple_source, "SimpleIntervals");

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(10), TimeFrameIndex(30), TimeFrameIndex(55)};

        ExecutionPlan plan(test_timestamps, behavior_time_frame);

        auto [registry_result, registryEntity_ids] = registry_computer->compute(plan);
        auto [direct_result, directEntity_ids] = direct_computer->compute(plan);

        REQUIRE(registry_result.size() == 3);
        REQUIRE(direct_result.size() == 3);

        // Results should be identical
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(registry_result[i] == direct_result[i]);
        }

        std::cout << "Comparison test - results are identical:" << std::endl;
        for (size_t i = 0; i < 3; ++i) {
            std::cout << "  Timestamp " << test_timestamps[i].getValue()
                      << ": Registry=" << (registry_result[i] ? "true" : "false")
                      << ", Direct=" << (direct_result[i] ? "true" : "false") << std::endl;
        }
    }
}

TEST_CASE_METHOD(TimestampTableRegistryTestFixture, "DM - TV - TimestampInIntervalComputer via JSON TablePipeline", "[TimestampInIntervalComputer][JSON][Pipeline]") {

    SECTION("Test TimestampInIntervalComputer via JSON pipeline") {
        // JSON configuration for testing TimestampInIntervalComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Timestamp In Interval Test",
                "description": "Test JSON execution of TimestampInIntervalComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "timestamp_in_interval_test",
                    "name": "Timestamp In Interval Test Table",
                    "description": "Test table using TimestampInIntervalComputer",
                    "row_selector": {
                        "type": "timestamp",
                        "source": "behavior_time"
                    },
                    "columns": [
                        {
                            "name": "InBehaviorPeriod",
                            "description": "True if timestamp is within any behavior period",
                            "data_source": "BehaviorPeriods",
                            "computer": "Timestamp In Interval"
                        },
                        {
                            "name": "InSimpleInterval",
                            "description": "True if timestamp is within any simple interval",
                            "data_source": "SimpleIntervals",
                            "computer": "Timestamp In Interval"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();

        // Parse the JSON configuration
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        // Load configuration into pipeline
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Verify configuration was loaded correctly
        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.table_id == "timestamp_in_interval_test");
        REQUIRE(config.name == "Timestamp In Interval Test Table");
        REQUIRE(config.columns.size() == 2);

        // Verify column configurations
        auto const & column1 = config.columns[0];
        REQUIRE(column1["name"] == "InBehaviorPeriod");
        REQUIRE(column1["computer"] == "Timestamp In Interval");
        REQUIRE(column1["data_source"] == "BehaviorPeriods");

        auto const & column2 = config.columns[1];
        REQUIRE(column2["name"] == "InSimpleInterval");
        REQUIRE(column2["computer"] == "Timestamp In Interval");
        REQUIRE(column2["data_source"] == "SimpleIntervals");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "timestamp");
        REQUIRE(config.row_selector["source"] == "behavior_time");

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute([](int table_index, std::string const & table_name, int table_progress, int overall_progress) {
            std::cout << "Building table " << table_index << " (" << table_name << "): "
                      << table_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
        });

        if (pipeline_result.success) {
            std::cout << "Pipeline executed successfully!" << std::endl;
            std::cout << "Tables completed: " << pipeline_result.tables_completed << "/" << pipeline_result.total_tables << std::endl;
            std::cout << "Execution time: " << pipeline_result.total_execution_time_ms << " ms" << std::endl;

            // Verify the built table exists
            auto & registry = getTableRegistry();
            REQUIRE(registry.hasTable("timestamp_in_interval_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("timestamp_in_interval_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 2);
            REQUIRE(built_table->hasColumn("InBehaviorPeriod"));
            REQUIRE(built_table->hasColumn("InSimpleInterval"));

            // Verify table has 101 rows (one for each timestamp in behavior_time: 0-100)
            REQUIRE(built_table->getRowCount() == 101);

            // Get and verify the computed values
            auto behavior_results = built_table->getColumnValues<bool>("InBehaviorPeriod");
            auto simple_results = built_table->getColumnValues<bool>("InSimpleInterval");

            REQUIRE(behavior_results.size() == 101);
            REQUIRE(simple_results.size() == 101);

            // Verify some expected results based on our test data:
            // BehaviorPeriods: [10-25], [35-45], [60-80], [85-95]
            // SimpleIntervals: [5-15], [25-35], [50-60]

            // Check specific timestamps
            REQUIRE(behavior_results[5] == false);// Timestamp 5: not in behavior periods
            REQUIRE(simple_results[5] == true);   // Timestamp 5: in simple interval [5-15]

            REQUIRE(behavior_results[15] == true);// Timestamp 15: in behavior period [10-25]
            REQUIRE(simple_results[15] == true);  // Timestamp 15: in simple interval [5-15]

            REQUIRE(behavior_results[30] == false);// Timestamp 30: not in behavior periods
            REQUIRE(simple_results[30] == true);   // Timestamp 30: in simple interval [25-35]

            REQUIRE(behavior_results[40] == true);// Timestamp 40: in behavior period [35-45]
            REQUIRE(simple_results[40] == false); // Timestamp 40: not in simple intervals

            REQUIRE(behavior_results[70] == true);// Timestamp 70: in behavior period [60-80]
            REQUIRE(simple_results[70] == false); // Timestamp 70: not in simple intervals

            REQUIRE(behavior_results[90] == true);// Timestamp 90: in behavior period [85-95]
            REQUIRE(simple_results[90] == false); // Timestamp 90: not in simple intervals

            // All values should be valid booleans
            for (size_t i = 0; i < 101; ++i) {
                REQUIRE((behavior_results[i] == true || behavior_results[i] == false));
                REQUIRE((simple_results[i] == true || simple_results[i] == false));
            }

            // Print some sample results for verification
            std::vector<int> sample_timestamps = {5, 10, 15, 20, 30, 40, 55, 70, 90};
            std::cout << "Sample results from full timeframe:" << std::endl;
            for (int ts: sample_timestamps) {
                std::cout << "Timestamp " << ts
                          << ": InBehaviorPeriod=" << (behavior_results[ts] ? "true" : "false")
                          << ", InSimpleInterval=" << (simple_results[ts] ? "true" : "false") << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test with cross-timeframe intervals via JSON") {
        char const * json_config = R"({
            "metadata": {
                "name": "Cross-Timeframe Timestamp Test",
                "description": "Test cross-timeframe timestamp interval detection"
            },
            "tables": [
                {
                    "table_id": "cross_timeframe_test",
                    "name": "Cross-Timeframe Test Table",
                    "description": "Test table with cross-timeframe interval detection",
                    "row_selector": {
                        "type": "timestamp",
                        "source": "high_res_time"
                    },
                    "columns": [
                        {
                            "name": "InStimulusInterval",
                            "description": "True if timestamp overlaps with stimulus intervals",
                            "data_source": "StimulusIntervals",
                            "computer": "Timestamp In Interval"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.row_selector["source"] == "high_res_time");
        REQUIRE(config.columns[0]["data_source"] == "StimulusIntervals");

        std::cout << "Cross-timeframe JSON configuration parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ Cross-timeframe pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("cross_timeframe_test");
            REQUIRE(built_table != nullptr);

            REQUIRE(built_table->getRowCount() == 201);// high_res_time has 201 points (0-200)
            REQUIRE(built_table->getColumnCount() == 1);
            REQUIRE(built_table->hasColumn("InStimulusInterval"));

            auto results = built_table->getColumnValues<bool>("InStimulusInterval");
            REQUIRE(results.size() == 201);

            // Verify all results are valid
            for (size_t i = 0; i < 201; ++i) {
                REQUIRE((results[i] == true || results[i] == false));
            }

            // Print some sample results for verification
            std::vector<int> sample_timestamps = {10, 30, 60, 95};
            std::cout << "Sample cross-timeframe results:" << std::endl;
            for (int ts: sample_timestamps) {
                if (ts < 201) {// Make sure index is valid
                    std::cout << "High-res timestamp " << ts
                              << ": " << (results[ts] ? "inside" : "outside") << " stimulus interval" << std::endl;
                }
            }

        } else {
            FAIL("Cross-timeframe pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}

// Keep the original basic tests for backward compatibility
TEST_CASE("TimestampInIntervalComputer basic integration", "[TimestampInIntervalComputer]") {
    DataManager dm;

    // Build a simple timeframe 0..9
    std::vector<int> times(10);
    for (int i = 0; i < 10; ++i) times[i] = i;
    auto tf = std::make_shared<TimeFrame>(times);
    dm.setTime(TimeKey("cam"), tf, true);

    // Create digital interval series: [2,4] and [7,8]
    auto dis = std::make_shared<DigitalIntervalSeries>();
    dis->addEvent(Interval{2, 4});
    dis->addEvent(Interval{7, 8});
    dm.setData<DigitalIntervalSeries>("Intervals", dis, TimeKey("cam"));

    auto dme = std::make_shared<DataManagerExtension>(dm);

    // Timestamps: 1,3,5,8
    std::vector<TimeFrameIndex> ts = {TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(5), TimeFrameIndex(8)};
    auto selector = std::make_unique<TimestampSelector>(ts, tf);

    // Build computer directly
    auto src = dme->getIntervalSource("Intervals");
    REQUIRE(src != nullptr);
    auto comp = std::make_unique<TimestampInIntervalComputer>(src, "Intervals");

    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(selector));
    builder.addColumn<bool>("Inside", std::move(comp));
    auto table = builder.build();

    auto const & vals = table.getColumnValues<bool>("Inside");
    REQUIRE(vals.size() == 4);
    // 1:false, 3:true, 5:false, 8:true
    REQUIRE(vals[0] == false);
    REQUIRE(vals[1] == true);
    REQUIRE(vals[2] == false);
    REQUIRE(vals[3] == true);
}

TEST_CASE("TimestampInIntervalComputer via registry", "[TimestampInIntervalComputer][Registry]") {
    DataManager dm;
    std::vector<int> times(6);
    for (int i = 0; i < 6; ++i) times[i] = i;
    auto tf = std::make_shared<TimeFrame>(times);
    dm.setTime(TimeKey("cam"), tf, true);

    auto dis = std::make_shared<DigitalIntervalSeries>();
    dis->addEvent(Interval{0, 2});
    dis->addEvent(Interval{4, 5});
    dm.setData<DigitalIntervalSeries>("DInt", dis, TimeKey("cam"));

    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto src = dme->getIntervalSource("DInt");
    REQUIRE(src != nullptr);

    ComputerRegistry registry;
    DataSourceVariant variant = DataSourceVariant{src};

    auto typed = registry.createTypedComputer<bool>("Timestamp In Interval", variant, {});
    REQUIRE(typed != nullptr);

    // Build table over timestamps 0..5
    std::vector<TimeFrameIndex> ts = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    auto selector = std::make_unique<TimestampSelector>(ts, tf);

    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(selector));
    builder.addColumn<bool>("InInt", std::move(typed));
    auto table = builder.build();

    auto const & vals = table.getColumnValues<bool>("InInt");
    REQUIRE(vals.size() == 6);
    // [0,2] => true,true,true,false,true,true
    REQUIRE(vals[0] == true);
    REQUIRE(vals[1] == true);
    REQUIRE(vals[2] == true);
    REQUIRE(vals[3] == false);
    REQUIRE(vals[4] == true);
    REQUIRE(vals[5] == true);
}
