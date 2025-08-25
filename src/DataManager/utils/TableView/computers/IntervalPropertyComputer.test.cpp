#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TimeFrame/interval_data.hpp"
#include "IntervalPropertyComputer.h"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IIntervalSource.h"

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
 * @brief Base test fixture for IntervalPropertyComputer with realistic interval data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Intervals representing different event periods with realistic durations
 * - Cross-timeframe scenarios for comprehensive testing
 */
class IntervalPropertyTestFixture {
protected:
    IntervalPropertyTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithIntervalTestData();
    }

    ~IntervalPropertyTestFixture() = default;

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
        createNeuralEvents();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "behavior_time" timeframe: 0 to 200 (201 points) - behavior tracking at 10Hz
        std::vector<int> behavior_time_values(201);
        std::iota(behavior_time_values.begin(), behavior_time_values.end(), 0);
        auto behavior_time_frame = std::make_shared<TimeFrame>(behavior_time_values);
        m_data_manager->setTime(TimeKey("behavior_time"), behavior_time_frame, true);

        // Create "neural_time" timeframe: 0, 2, 4, 6, ..., 200 (101 points) - neural recording at 5Hz
        std::vector<int> neural_time_values;
        neural_time_values.reserve(101);
        for (int i = 0; i <= 100; ++i) {
            neural_time_values.push_back(i * 2);
        }
        auto neural_time_frame = std::make_shared<TimeFrame>(neural_time_values);
        m_data_manager->setTime(TimeKey("neural_time"), neural_time_frame, true);

        // Create "high_res_time" timeframe: millisecond resolution for precise timing
        std::vector<int> high_res_values;
        high_res_values.reserve(201);
        for (int i = 0; i <= 200; ++i) {
            high_res_values.push_back(i);
        }
        auto high_res_frame = std::make_shared<TimeFrame>(high_res_values);
        m_data_manager->setTime(TimeKey("high_res_time"), high_res_frame, true);
    }

    /**
     * @brief Create behavior intervals for testing property extraction
     */
    void createBehaviorIntervals() {
        // Create behavior periods with varying durations
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();

        // Short grooming bout: time 10-15 (duration = 5)
        behavior_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(15));

        // Medium exploration period: time 30-50 (duration = 20)
        behavior_intervals->addEvent(TimeFrameIndex(30), TimeFrameIndex(50));

        // Long rest period: time 70-120 (duration = 50)
        behavior_intervals->addEvent(TimeFrameIndex(70), TimeFrameIndex(120));

        // Brief social interaction: time 150-155 (duration = 5)
        behavior_intervals->addEvent(TimeFrameIndex(150), TimeFrameIndex(155));

        // Very long sleep period: time 160-190 (duration = 30)
        behavior_intervals->addEvent(TimeFrameIndex(160), TimeFrameIndex(190));

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));
    }

    /**
     * @brief Create stimulus intervals with different patterns
     */
    void createStimulusIntervals() {
        // Create stimulus presentation periods with precise timing
        auto stimulus_intervals = std::make_shared<DigitalIntervalSeries>();

        // Brief stimulus 1: time 5-8 (duration = 3)
        stimulus_intervals->addEvent(TimeFrameIndex(5), TimeFrameIndex(8));

        // Medium stimulus 2: time 25-35 (duration = 10)
        stimulus_intervals->addEvent(TimeFrameIndex(25), TimeFrameIndex(35));

        // Long stimulus 3: time 80-100 (duration = 20)
        stimulus_intervals->addEvent(TimeFrameIndex(80), TimeFrameIndex(100));

        // Short stimulus 4: time 140-142 (duration = 2)
        stimulus_intervals->addEvent(TimeFrameIndex(140), TimeFrameIndex(142));

        m_data_manager->setData<DigitalIntervalSeries>("StimulusIntervals", stimulus_intervals, TimeKey("neural_time"));
    }

    /**
     * @brief Create neural event intervals for high-resolution testing
     */
    void createNeuralEvents() {
        // Create neural activity bursts with precise timing
        auto neural_intervals = std::make_shared<DigitalIntervalSeries>();

        // Microsecond-level precision events
        // Event 1: time 12-13 (duration = 1)
        neural_intervals->addEvent(TimeFrameIndex(12), TimeFrameIndex(13));

        // Event 2: time 45-47 (duration = 2)
        neural_intervals->addEvent(TimeFrameIndex(45), TimeFrameIndex(47));

        // Event 3: time 89-92 (duration = 3)
        neural_intervals->addEvent(TimeFrameIndex(89), TimeFrameIndex(92));

        // Event 4: time 156-161 (duration = 5)
        neural_intervals->addEvent(TimeFrameIndex(156), TimeFrameIndex(161));

        m_data_manager->setData<DigitalIntervalSeries>("NeuralEvents", neural_intervals, TimeKey("high_res_time"));
    }
};

/**
 * @brief Test fixture combining IntervalPropertyTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with interval test data (from IntervalPropertyTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class IntervalPropertyTableRegistryTestFixture : public IntervalPropertyTestFixture {
protected:
    IntervalPropertyTableRegistryTestFixture()
        : IntervalPropertyTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~IntervalPropertyTableRegistryTestFixture() = default;

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

        for (auto const & interval: m_intervals) {
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

TEST_CASE("DM - TV - IntervalPropertyComputer Basic Functionality", "[IntervalPropertyComputer]") {

    SECTION("Start property extraction") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy - not used for property extraction)
        std::vector<Interval> columnIntervals = {{0, 1}, {3, 5}, {7, 9}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestIntervals", timeFrame, columnIntervals);

        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)),// Start = 1
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(5)),// Start = 2
                TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)),// Start = 6
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)) // Start = 0
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Create the computer for Start property
        IntervalPropertyComputer<int64_t> computer(intervalSource,
                                                   IntervalProperty::Start,
                                                   "TestIntervals");

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 1);// First interval start
        REQUIRE(results[1] == 2);// Second interval start
        REQUIRE(results[2] == 6);// Third interval start
        REQUIRE(results[3] == 0);// Fourth interval start
    }

    SECTION("End property extraction") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{0, 1}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestIntervals", timeFrame, columnIntervals);

        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)),   // End = 3
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(5)),   // End = 5
                TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)),   // End = 8
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1))    // End = 1
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Create the computer for End property
        IntervalPropertyComputer<int64_t> computer(intervalSource,
                                                   IntervalProperty::End,
                                                   "TestIntervals");

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 3);// First interval end
        REQUIRE(results[1] == 5);// Second interval end
        REQUIRE(results[2] == 8);// Third interval end
        REQUIRE(results[3] == 1);// Fourth interval end
    }

    SECTION("Duration property extraction") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{0, 1}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestIntervals", timeFrame, columnIntervals);

        // Create row intervals with different durations
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)),// Duration = 3-1 = 2
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(5)),// Duration = 5-2 = 3
                TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)),// Duration = 8-6 = 2
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(6)) // Duration = 6-0 = 6
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Create the computer for Duration property
        IntervalPropertyComputer<int64_t> computer(intervalSource,
                                                   IntervalProperty::Duration,
                                                   "TestIntervals");

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == 2);// Duration = 3-1
        REQUIRE(results[1] == 3);// Duration = 5-2
        REQUIRE(results[2] == 2);// Duration = 8-6
        REQUIRE(results[3] == 6);// Duration = 6-0
    }

    SECTION("Single interval scenarios") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{1, 3}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "SingleInterval", timeFrame, columnIntervals);

        // Create single row interval
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4))// Start=2, End=4, Duration=2
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Test Start property
        IntervalPropertyComputer<int64_t> startComputer(intervalSource,
                                                        IntervalProperty::Start,
                                                        "SingleInterval");
        auto startResults = startComputer.compute(plan);
        REQUIRE(startResults.size() == 1);
        REQUIRE(startResults[0] == 2);

        // Test End property
        IntervalPropertyComputer<int64_t> endComputer(intervalSource,
                                                      IntervalProperty::End,
                                                      "SingleInterval");
        auto endResults = endComputer.compute(plan);
        REQUIRE(endResults.size() == 1);
        REQUIRE(endResults[0] == 4);

        // Test Duration property
        IntervalPropertyComputer<int64_t> durationComputer(intervalSource,
                                                           IntervalProperty::Duration,
                                                           "SingleInterval");
        auto durationResults = durationComputer.compute(plan);
        REQUIRE(durationResults.size() == 1);
        REQUIRE(durationResults[0] == 2);
    }

    SECTION("Zero-duration intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{0, 0}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "ZeroDuration", timeFrame, columnIntervals);

        // Create zero-duration row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)),// Duration = 0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(3)) // Duration = 0
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Test Duration property
        IntervalPropertyComputer<int64_t> durationComputer(intervalSource,
                                                           IntervalProperty::Duration,
                                                           "ZeroDuration");
        auto durationResults = durationComputer.compute(plan);

        REQUIRE(durationResults.size() == 2);
        REQUIRE(durationResults[0] == 0);// Zero duration
        REQUIRE(durationResults[1] == 0);// Zero duration
    }
}

TEST_CASE("DM - TV - IntervalPropertyComputer Error Handling", "[IntervalPropertyComputer][Error]") {

    SECTION("ExecutionPlan without intervals throws exception") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{1, 3}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestIntervals", timeFrame, columnIntervals);

        // Create execution plan with indices instead of intervals
        std::vector<TimeFrameIndex> indices = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(indices, timeFrame);

        // Create the computer
        IntervalPropertyComputer<int64_t> computer(intervalSource,
                                                   IntervalProperty::Start,
                                                   "TestIntervals");

        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }
}

TEST_CASE("DM - TV - IntervalPropertyComputer Template Types", "[IntervalPropertyComputer][Templates]") {

    SECTION("Test with different numeric types") {
        // Create time frame
        std::vector<int> timeValues = {0, 5, 10, 15, 20, 25};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create column intervals (dummy)
        std::vector<Interval> columnIntervals = {{1, 3}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestIntervals", timeFrame, columnIntervals);

        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
                TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(4))// Start=1, End=4, Duration=3
        };

        ExecutionPlan plan(rowIntervals, timeFrame);

        // Test with int64_t
        IntervalPropertyComputer<int64_t> intComputer(intervalSource,
                                                      IntervalProperty::Duration,
                                                      "TestIntervals");
        auto intResults = intComputer.compute(plan);
        REQUIRE(intResults.size() == 1);
        REQUIRE(intResults[0] == 3);

        // Test with float
        IntervalPropertyComputer<float> floatComputer(intervalSource,
                                                      IntervalProperty::Duration,
                                                      "TestIntervals");
        auto floatResults = floatComputer.compute(plan);
        REQUIRE(floatResults.size() == 1);
        REQUIRE(floatResults[0] == Catch::Approx(3.0f));

        // Test with double
        IntervalPropertyComputer<double> doubleComputer(intervalSource,
                                                        IntervalProperty::Duration,
                                                        "TestIntervals");
        auto doubleResults = doubleComputer.compute(plan);
        REQUIRE(doubleResults.size() == 1);
        REQUIRE(doubleResults[0] == Catch::Approx(3.0));
    }
}

TEST_CASE("DM - TV - IntervalPropertyComputer Dependency Tracking", "[IntervalPropertyComputer][Dependencies]") {

    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<Interval> columnIntervals = {{0, 1}};
        auto intervalSource = std::make_shared<MockIntervalSource>(
                "TestSource", timeFrame, columnIntervals);

        // Create computer
        IntervalPropertyComputer<int64_t> computer(intervalSource,
                                                   IntervalProperty::Start,
                                                   "TestSourceName");

        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "TestSourceName");
    }
}

TEST_CASE_METHOD(IntervalPropertyTestFixture, "DM - TV - IntervalPropertyComputer with DataManager fixture", "[IntervalPropertyComputer][DataManager][Fixture]") {

    SECTION("Test with behavior intervals from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the interval source from the DataManager
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");

        REQUIRE(behavior_source != nullptr);

        // Create row selector from behavior intervals
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        auto behavior_intervals = behavior_source->getIntervalsInRange(
                TimeFrameIndex(0), TimeFrameIndex(200), behavior_time_frame.get());

        REQUIRE(behavior_intervals.size() == 5);// 5 behavior periods from fixture

        // Convert to TimeFrameIntervals for row selector
        std::vector<TimeFrameInterval> row_intervals;
        for (auto const & interval: behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Test Start property
        builder.addColumn<double>("IntervalStart",
                                  std::make_unique<IntervalPropertyComputer<double>>(behavior_source,
                                                                                     IntervalProperty::Start, "BehaviorPeriods"));

        // Test End property
        builder.addColumn<double>("IntervalEnd",
                                  std::make_unique<IntervalPropertyComputer<double>>(behavior_source,
                                                                                     IntervalProperty::End, "BehaviorPeriods"));

        // Test Duration property
        builder.addColumn<double>("IntervalDuration",
                                  std::make_unique<IntervalPropertyComputer<double>>(behavior_source,
                                                                                     IntervalProperty::Duration, "BehaviorPeriods"));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 5);
        REQUIRE(table.getColumnCount() == 3);
        REQUIRE(table.hasColumn("IntervalStart"));
        REQUIRE(table.hasColumn("IntervalEnd"));
        REQUIRE(table.hasColumn("IntervalDuration"));

        // Get the column data
        auto starts = table.getColumnValues<double>("IntervalStart");
        auto ends = table.getColumnValues<double>("IntervalEnd");
        auto durations = table.getColumnValues<double>("IntervalDuration");

        REQUIRE(starts.size() == 5);
        REQUIRE(ends.size() == 5);
        REQUIRE(durations.size() == 5);

        // Verify the property values match our test data expectations
        // Expected behavior periods from fixture:
        // Period 1: 10-15 (duration = 5)
        // Period 2: 30-50 (duration = 20)
        // Period 3: 70-120 (duration = 50)
        // Period 4: 150-155 (duration = 5)
        // Period 5: 160-190 (duration = 30)

        for (size_t i = 0; i < 5; ++i) {
            REQUIRE(starts[i] >= 0);                       // Valid start times
            REQUIRE(ends[i] > starts[i]);                  // End > Start
            REQUIRE(durations[i] > 0);                     // Positive durations
            REQUIRE(durations[i] == (ends[i] - starts[i]));// Duration = End - Start

            std::cout << "Behavior period " << i << ": Start=" << starts[i]
                      << ", End=" << ends[i] << ", Duration=" << durations[i] << std::endl;
        }

        // Verify specific expected values (first few intervals)
        REQUIRE(starts[0] == Catch::Approx(10.0));  // First period starts at 10
        REQUIRE(ends[0] == Catch::Approx(15.0));    // First period ends at 15
        REQUIRE(durations[0] == Catch::Approx(5.0));// First period duration = 5

        REQUIRE(starts[1] == Catch::Approx(30.0));   // Second period starts at 30
        REQUIRE(durations[1] == Catch::Approx(20.0));// Second period duration = 20
    }

    SECTION("Test with neural events from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get neural events from high-resolution timeframe
        auto neural_source = dme->getIntervalSource("NeuralEvents");
        REQUIRE(neural_source != nullptr);

        auto neural_time_frame = dm.getTime(TimeKey("high_res_time"));
        auto neural_intervals = neural_source->getIntervalsInRange(
                TimeFrameIndex(0), TimeFrameIndex(200), neural_time_frame.get());

        REQUIRE(neural_intervals.size() == 4);// 4 neural events from fixture

        // Convert to TimeFrameIntervals for row selector
        std::vector<TimeFrameInterval> row_intervals;
        for (auto const & interval: neural_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, neural_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add property computers
        builder.addColumn<int64_t>("EventStart",
                                   std::make_unique<IntervalPropertyComputer<int64_t>>(neural_source,
                                                                                       IntervalProperty::Start, "NeuralEvents"));

        builder.addColumn<int64_t>("EventDuration",
                                   std::make_unique<IntervalPropertyComputer<int64_t>>(neural_source,
                                                                                       IntervalProperty::Duration, "NeuralEvents"));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 2);

        auto starts = table.getColumnValues<int64_t>("EventStart");
        auto durations = table.getColumnValues<int64_t>("EventDuration");

        REQUIRE(starts.size() == 4);
        REQUIRE(durations.size() == 4);

        // Verify neural events match our test data:
        // Event 1: 12-13 (duration = 1)
        // Event 2: 45-47 (duration = 2)
        // Event 3: 89-92 (duration = 3)
        // Event 4: 156-161 (duration = 5)

        std::vector<int64_t> expected_starts = {12, 45, 89, 156};
        std::vector<int64_t> expected_durations = {1, 2, 3, 5};

        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(starts[i] == expected_starts[i]);
            REQUIRE(durations[i] == expected_durations[i]);

            std::cout << "Neural event " << i << ": Start=" << starts[i]
                      << ", Duration=" << durations[i] << std::endl;
        }
    }
}

TEST_CASE_METHOD(IntervalPropertyTableRegistryTestFixture, "DM - TV - IntervalPropertyComputer via ComputerRegistry", "[IntervalPropertyComputer][Registry]") {

    SECTION("Verify IntervalPropertyComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that all interval property computers are registered
        auto start_info = registry.findComputerInfo("Interval Start");
        auto end_info = registry.findComputerInfo("Interval End");
        auto duration_info = registry.findComputerInfo("Interval Duration");

        REQUIRE(start_info != nullptr);
        REQUIRE(end_info != nullptr);
        REQUIRE(duration_info != nullptr);

        // Verify computer info details for Start
        REQUIRE(start_info->name == "Interval Start");
        REQUIRE(start_info->outputType == typeid(double));
        REQUIRE(start_info->outputTypeName == "double");
        REQUIRE(start_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(start_info->requiredSourceType == typeid(std::shared_ptr<IIntervalSource>));

        // Verify computer info details for End
        REQUIRE(end_info->name == "Interval End");
        REQUIRE(end_info->outputType == typeid(double));
        REQUIRE(end_info->outputTypeName == "double");
        REQUIRE(end_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(end_info->requiredSourceType == typeid(std::shared_ptr<IIntervalSource>));

        // Verify computer info details for Duration
        REQUIRE(duration_info->name == "Interval Duration");
        REQUIRE(duration_info->outputType == typeid(double));
        REQUIRE(duration_info->outputTypeName == "double");
        REQUIRE(duration_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(duration_info->requiredSourceType == typeid(std::shared_ptr<IIntervalSource>));
    }

    SECTION("Create IntervalPropertyComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get behavior source for testing
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        REQUIRE(behavior_source != nullptr);

        // Create computers via registry
        std::map<std::string, std::string> empty_params;

        auto start_computer = registry.createTypedComputer<double>(
                "Interval Start", behavior_source, empty_params);
        auto end_computer = registry.createTypedComputer<double>(
                "Interval End", behavior_source, empty_params);
        auto duration_computer = registry.createTypedComputer<double>(
                "Interval Duration", behavior_source, empty_params);

        REQUIRE(start_computer != nullptr);
        REQUIRE(end_computer != nullptr);
        REQUIRE(duration_computer != nullptr);

        // Test that the created computers work correctly
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));

        // Create a simple test interval
        std::vector<TimeFrameInterval> test_intervals = {
                TimeFrameInterval(TimeFrameIndex(30), TimeFrameIndex(50))// Behavior period 2
        };

        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Use the registry-created computers
        builder.addColumn("RegistryStart", std::move(start_computer));
        builder.addColumn("RegistryEnd", std::move(end_computer));
        builder.addColumn("RegistryDuration", std::move(duration_computer));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 3);
        REQUIRE(table.hasColumn("RegistryStart"));
        REQUIRE(table.hasColumn("RegistryEnd"));
        REQUIRE(table.hasColumn("RegistryDuration"));

        auto starts = table.getColumnValues<double>("RegistryStart");
        auto ends = table.getColumnValues<double>("RegistryEnd");
        auto durations = table.getColumnValues<double>("RegistryDuration");

        REQUIRE(starts.size() == 1);
        REQUIRE(ends.size() == 1);
        REQUIRE(durations.size() == 1);

        // Verify reasonable results (should match the test interval: 30-50)
        REQUIRE(starts[0] == Catch::Approx(30.0));
        REQUIRE(ends[0] == Catch::Approx(50.0));
        REQUIRE(durations[0] == Catch::Approx(20.0));

        std::cout << "Registry test - Start: " << starts[0]
                  << ", End: " << ends[0] << ", Duration: " << durations[0] << std::endl;
    }

    SECTION("Compare registry-created vs direct-created computers") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        REQUIRE(behavior_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<double>(
                "Interval Duration", behavior_source, empty_params);

        // Create computer directly
        auto direct_computer = std::make_unique<IntervalPropertyComputer<double>>(
                behavior_source, IntervalProperty::Duration, "BehaviorPeriods");

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
                TimeFrameInterval(TimeFrameIndex(70), TimeFrameIndex(120))// Period 3: duration = 50
        };

        ExecutionPlan plan(test_intervals, behavior_time_frame);

        auto registry_result = registry_computer->compute(plan);
        auto direct_result = direct_computer->compute(plan);

        REQUIRE(registry_result.size() == 1);
        REQUIRE(direct_result.size() == 1);

        // Results should be identical
        REQUIRE(registry_result[0] == Catch::Approx(direct_result[0]));
        REQUIRE(registry_result[0] == Catch::Approx(50.0));// Expected duration

        std::cout << "Comparison test - Registry result: " << registry_result[0]
                  << ", Direct result: " << direct_result[0] << std::endl;
    }
}

TEST_CASE_METHOD(IntervalPropertyTableRegistryTestFixture, "DM - TV - IntervalPropertyComputer via JSON TablePipeline", "[IntervalPropertyComputer][JSON][Pipeline]") {

    SECTION("Test all property operations via JSON pipeline") {
        // JSON configuration for testing IntervalPropertyComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Interval Property Test",
                "description": "Test JSON execution of IntervalPropertyComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "interval_property_test",
                    "name": "Interval Property Test Table",
                    "description": "Test table using IntervalPropertyComputer",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "IntervalStartTime",
                            "description": "Start time of each behavior period",
                            "data_source": "BehaviorPeriods",
                            "computer": "Interval Start"
                        },
                        {
                            "name": "IntervalEndTime",
                            "description": "End time of each behavior period",
                            "data_source": "BehaviorPeriods",
                            "computer": "Interval End"
                        },
                        {
                            "name": "IntervalDuration",
                            "description": "Duration of each behavior period",
                            "data_source": "BehaviorPeriods",
                            "computer": "Interval Duration"
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
        REQUIRE(config.table_id == "interval_property_test");
        REQUIRE(config.name == "Interval Property Test Table");
        REQUIRE(config.columns.size() == 3);

        // Verify column configurations
        auto const & column1 = config.columns[0];
        REQUIRE(column1["name"] == "IntervalStartTime");
        REQUIRE(column1["computer"] == "Interval Start");
        REQUIRE(column1["data_source"] == "BehaviorPeriods");

        auto const & column2 = config.columns[1];
        REQUIRE(column2["name"] == "IntervalEndTime");
        REQUIRE(column2["computer"] == "Interval End");
        REQUIRE(column2["data_source"] == "BehaviorPeriods");

        auto const & column3 = config.columns[2];
        REQUIRE(column3["name"] == "IntervalDuration");
        REQUIRE(column3["computer"] == "Interval Duration");
        REQUIRE(column3["data_source"] == "BehaviorPeriods");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "interval");
        REQUIRE(config.row_selector["source"] == "BehaviorPeriods");

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
            REQUIRE(registry.hasTable("interval_property_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("interval_property_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 3);
            REQUIRE(built_table->hasColumn("IntervalStartTime"));
            REQUIRE(built_table->hasColumn("IntervalEndTime"));
            REQUIRE(built_table->hasColumn("IntervalDuration"));

            // Verify table has 5 rows (one for each behavior period)
            REQUIRE(built_table->getRowCount() == 5);

            // Get and verify the computed values
            auto start_times = built_table->getColumnValues<double>("IntervalStartTime");
            auto end_times = built_table->getColumnValues<double>("IntervalEndTime");
            auto durations = built_table->getColumnValues<double>("IntervalDuration");

            REQUIRE(start_times.size() == 5);
            REQUIRE(end_times.size() == 5);
            REQUIRE(durations.size() == 5);

            for (size_t i = 0; i < 5; ++i) {
                REQUIRE(start_times[i] >= 0);                                         // Valid start times
                REQUIRE(end_times[i] > start_times[i]);                               // End > Start
                REQUIRE(durations[i] > 0);                                            // Positive durations
                REQUIRE(durations[i] == Catch::Approx(end_times[i] - start_times[i]));// Duration = End - Start

                std::cout << "Row " << i << ": Start=" << start_times[i]
                          << ", End=" << end_times[i] << ", Duration=" << durations[i] << std::endl;
            }

            // Verify specific expected values from our fixture
            REQUIRE(start_times[0] == Catch::Approx(10.0));// First period: 10-15
            REQUIRE(end_times[0] == Catch::Approx(15.0));
            REQUIRE(durations[0] == Catch::Approx(5.0));

            REQUIRE(start_times[1] == Catch::Approx(30.0));// Second period: 30-50
            REQUIRE(durations[1] == Catch::Approx(20.0));

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test mixed interval properties with neural data") {
        char const * json_config = R"({
            "metadata": {
                "name": "Neural Event Property Test",
                "description": "Test IntervalPropertyComputer with neural events"
            },
            "tables": [
                {
                    "table_id": "neural_property_test",
                    "name": "Neural Event Property Table",
                    "description": "Test table using neural event intervals",
                    "row_selector": {
                        "type": "interval",
                        "source": "NeuralEvents"
                    },
                    "columns": [
                        {
                            "name": "EventStart",
                            "description": "Start time of neural events",
                            "data_source": "NeuralEvents",
                            "computer": "Interval Start"
                        },
                        {
                            "name": "EventDuration",
                            "description": "Duration of neural events",
                            "data_source": "NeuralEvents",
                            "computer": "Interval Duration"
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
        REQUIRE(config.columns.size() == 2);
        REQUIRE(config.columns[0]["computer"] == "Interval Start");
        REQUIRE(config.columns[1]["computer"] == "Interval Duration");

        std::cout << "Neural event properties JSON configuration parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ Neural event property pipeline executed successfully!" << std::endl;

            // Verify the built table exists and has correct structure
            auto & registry = getTableRegistry();
            REQUIRE(registry.hasTable("neural_property_test"));

            auto built_table = registry.getBuiltTable("neural_property_test");
            REQUIRE(built_table != nullptr);

            // Should have 4 rows (one for each neural event from our fixture)
            REQUIRE(built_table->getRowCount() == 4);
            REQUIRE(built_table->getColumnCount() == 2);
            REQUIRE(built_table->hasColumn("EventStart"));
            REQUIRE(built_table->hasColumn("EventDuration"));

            auto starts = built_table->getColumnValues<double>("EventStart");
            auto durations = built_table->getColumnValues<double>("EventDuration");

            REQUIRE(starts.size() == 4);
            REQUIRE(durations.size() == 4);

            // Expected neural events from fixture:
            // Event 1: 12-13 (duration = 1)
            // Event 2: 45-47 (duration = 2)
            // Event 3: 89-92 (duration = 3)
            // Event 4: 156-161 (duration = 5)
            std::vector<double> expected_starts = {12.0, 45.0, 89.0, 156.0};
            std::vector<double> expected_durations = {1.0, 2.0, 3.0, 5.0};

            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(starts[i] == Catch::Approx(expected_starts[i]));
                REQUIRE(durations[i] == Catch::Approx(expected_durations[i]));
                std::cout << "Neural event " << i << ": Start=" << starts[i]
                          << ", Duration=" << durations[i] << std::endl;
            }

        } else {
            FAIL("Neural event pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
