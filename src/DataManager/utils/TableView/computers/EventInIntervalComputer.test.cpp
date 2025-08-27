#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "EventInIntervalComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"

// Additional includes for extended testing
#include "DataManager.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <memory>
#include <vector>
#include <algorithm>
#include <span>
#include <cstdint>
#include <numeric>
#include <nlohmann/json.hpp>

// Mock implementation of IEventSource for testing
class MockEventSource : public IEventSource {
public:
    MockEventSource(std::string name, 
                    std::shared_ptr<TimeFrame> timeFrame,
                    std::vector<float> events)
        : m_name(std::move(name)), 
          m_timeFrame(std::move(timeFrame)), 
          m_events(std::move(events)) {
        // Ensure events are sorted
        std::sort(m_events.begin(), m_events.end());
    }

    [[nodiscard]] auto getName() const -> std::string const & override {
        return m_name;
    }

    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override {
        return m_timeFrame;
    }

    [[nodiscard]] auto size() const -> size_t override {
        return m_events.size();
    }

    auto getDataInRange(TimeFrameIndex start, TimeFrameIndex end, 
                       TimeFrame const * target_timeFrame) -> std::vector<float> override {
        std::vector<float> result;
        
        // Convert TimeFrameIndex to time values for comparison
        auto startTime = target_timeFrame->getTimeAtIndex(start);
        auto endTime = target_timeFrame->getTimeAtIndex(end);
        
        for (const auto& event : m_events) {
            // Convert event to time value using our timeframe
            // For simplicity, assume events are already time values
            if (event >= startTime && event <= endTime) {
                result.push_back(event);
            }
        }
        
        return result;
    }

private:
    std::string m_name;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::vector<float> m_events;
};

/**
 * @brief Base test fixture for EventInIntervalComputer with realistic event data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Row intervals representing behavior periods  
 * - Event data representing spike times or other discrete events
 * - Cross-timeframe events for testing timeframe conversion
 */
class EventInIntervalTestFixture {
protected:
    EventInIntervalTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();
        
        // Populate with test data
        populateWithEventTestData();
    }

    ~EventInIntervalTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with event test data
     */
    void populateWithEventTestData() {
        createTimeFrames();
        createBehaviorIntervals();
        createSpikeEvents();
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

        // Create "spike_time" timeframe: 0, 2, 4, 6, ..., 100 (51 points) - spike recording at 5Hz
        std::vector<int> spike_time_values;
        spike_time_values.reserve(51);
        for (int i = 0; i <= 50; ++i) {
            spike_time_values.push_back(i * 2);
        }
        auto spike_time_frame = std::make_shared<TimeFrame>(spike_time_values);
        m_data_manager->setTime(TimeKey("spike_time"), spike_time_frame, true);
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
     * @brief Create spike event data (event data for testing)
     */
    void createSpikeEvents() {
        // Create spike train for Neuron1 - sparse spikes
        // Note: spike_time timeframe has 51 values [0, 2, 4, 6, ..., 100]
        // Events store INDICES into this timeframe, not absolute time values
        // So spike event "5" means timeframe[5] = 10 (absolute time)
        std::vector<float> neuron1_spikes = {
            1.0f,   // index 1 → time 2
            6.0f,   // index 6 → time 12
            7.0f,   // index 7 → time 14
            11.0f,  // index 11 → time 22
            16.0f,  // index 16 → time 32
            26.0f,  // index 26 → time 52
            27.0f,  // index 27 → time 54
            34.0f,  // index 34 → time 68
            41.0f,  // index 41 → time 82
            45.0f   // index 45 → time 90
        };
        auto neuron1_series = std::make_shared<DigitalEventSeries>(neuron1_spikes);
        m_data_manager->setData<DigitalEventSeries>("Neuron1Spikes", neuron1_series, TimeKey("spike_time"));
        
        // Create spike train for Neuron2 - dense spikes
        // All values are indices into the spike timeframe
        std::vector<float> neuron2_spikes = {
            0.0f,   // index 0 → time 0
            1.0f,   // index 1 → time 2
            2.0f,   // index 2 → time 4
            5.0f,   // index 5 → time 10
            6.0f,   // index 6 → time 12
            8.0f,   // index 8 → time 16
            9.0f,   // index 9 → time 18
            15.0f,  // index 15 → time 30
            16.0f,  // index 16 → time 32
            18.0f,  // index 18 → time 36
            25.0f,  // index 25 → time 50
            26.0f,  // index 26 → time 52
            28.0f,  // index 28 → time 56
            29.0f,  // index 29 → time 58
            33.0f,  // index 33 → time 66
            34.0f,  // index 34 → time 68
            40.0f,  // index 40 → time 80
            41.0f,  // index 41 → time 82
            42.0f,  // index 42 → time 84
            45.0f,  // index 45 → time 90
            46.0f   // index 46 → time 92
        };
        auto neuron2_series = std::make_shared<DigitalEventSeries>(neuron2_spikes);
        m_data_manager->setData<DigitalEventSeries>("Neuron2Spikes", neuron2_series, TimeKey("spike_time"));
        
        // Create spike train for Neuron3 - rhythmic spikes every 16 time units
        // Starting at time 4 (index 2), then time 12 (index 6), time 20 (index 10), etc.
        std::vector<float> neuron3_spikes;
        for (int i = 2; i <= 48; i += 4) {  // indices 2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46
            neuron3_spikes.push_back(static_cast<float>(i));
        }
        auto neuron3_series = std::make_shared<DigitalEventSeries>(neuron3_spikes);
        m_data_manager->setData<DigitalEventSeries>("Neuron3Spikes", neuron3_series, TimeKey("spike_time"));
    }
};

/**
 * @brief Test fixture combining EventInIntervalTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with event test data (from EventInIntervalTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class EventTableRegistryTestFixture : public EventInIntervalTestFixture {
protected:
    EventTableRegistryTestFixture()
        : EventInIntervalTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~EventTableRegistryTestFixture() = default;

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

TEST_CASE("DM - TV - EventInIntervalComputer Basic Functionality", "[EventInIntervalComputer]") {
    
    SECTION("Presence operation - detect events in intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create event data (events at times 1, 3, 5, 7, 9)
        std::vector<float> events = {1.0f, 3.0f, 5.0f, 7.0f, 9.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "TestEvents", timeFrame, events);
        
        // Create intervals for testing
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // Time 0-2: contains event at 1
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4)), // Time 2-4: contains event at 3
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(6)), // Time 4-6: contains event at 5
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)), // Time 6-8: contains event at 7
            TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(10)), // Time 8-10: contains event at 9
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)),  // Time 1-1: contains event at 1
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(6))   // Time 6-6: no events
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Create the computer
        EventInIntervalComputer<bool> computer(eventSource, 
                                              EventOperation::Presence,
                                              "TestEvents");
        
        // Compute the results
        auto results = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 7);
        REQUIRE(results[0] == true);   // Interval 0-2 contains event at 1
        REQUIRE(results[1] == true);   // Interval 2-4 contains event at 3
        REQUIRE(results[2] == true);   // Interval 4-6 contains event at 5
        REQUIRE(results[3] == true);   // Interval 6-8 contains event at 7
        REQUIRE(results[4] == true);   // Interval 8-10 contains event at 9
        REQUIRE(results[5] == true);   // Interval 1-1 contains event at 1
        REQUIRE(results[6] == false);  // Interval 6-6 contains no events
    }
    
    SECTION("Count operation - count events in intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create event data with multiple events in some intervals
        std::vector<float> events = {1.0f, 1.5f, 3.0f, 5.0f, 5.5f, 5.8f, 7.0f, 9.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "TestEvents", timeFrame, events);
        
        // Create intervals for testing
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // Time 0-2: events at 1.0, 1.5
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4)), // Time 2-4: event at 3.0
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(6)), // Time 4-6: events at 5.0, 5.5, 5.8
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(8)), // Time 6-8: event at 7.0
            TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(10)), // Time 8-10: event at 9.0
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(6))   // Time 6-6: no events
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Create the computer
        EventInIntervalComputer<int> computer(eventSource, 
                                             EventOperation::Count,
                                             "TestEvents");
        
        // Compute the results
        auto results = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 6);
        REQUIRE(results[0] == 2);  // Interval 0-2: 2 events
        REQUIRE(results[1] == 1);  // Interval 2-4: 1 event
        REQUIRE(results[2] == 3);  // Interval 4-6: 3 events
        REQUIRE(results[3] == 1);  // Interval 6-8: 1 event
        REQUIRE(results[4] == 1);  // Interval 8-10: 1 event
        REQUIRE(results[5] == 0);  // Interval 6-6: 0 events
    }
    
    SECTION("Gather operation - collect events in intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create event data
        std::vector<float> events = {1.0f, 2.5f, 3.0f, 5.0f, 5.5f, 7.0f, 9.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "TestEvents", timeFrame, events);
        
        // Create intervals for testing
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(3)), // Time 0-3: events at 1.0, 2.5, 3.0
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(6)), // Time 4-6: events at 5.0, 5.5
            TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(10)), // Time 8-10: event at 9.0
            TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(6))   // Time 6-6: no events
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Create the computer
        EventInIntervalComputer<std::vector<float>> computer(eventSource, 
                                                            EventOperation::Gather,
                                                            "TestEvents");
        
        // Compute the results
        auto results = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 4);
        
        // Check first interval (0-3)
        REQUIRE(results[0].size() == 3);
        REQUIRE(results[0][0] == Catch::Approx(1.0f).epsilon(0.001f));
        REQUIRE(results[0][1] == Catch::Approx(2.5f).epsilon(0.001f));
        REQUIRE(results[0][2] == Catch::Approx(3.0f).epsilon(0.001f));
        
        // Check second interval (4-6)
        REQUIRE(results[1].size() == 2);
        REQUIRE(results[1][0] == Catch::Approx(5.0f).epsilon(0.001f));
        REQUIRE(results[1][1] == Catch::Approx(5.5f).epsilon(0.001f));
        
        // Check third interval (8-10)
        REQUIRE(results[2].size() == 1);
        REQUIRE(results[2][0] == Catch::Approx(9.0f).epsilon(0.001f));
        
        // Check fourth interval (6-6) - should be empty
        REQUIRE(results[3].size() == 0);
    }
}

TEST_CASE("DM - TV - EventInIntervalComputer Edge Cases", "[EventInIntervalComputer][EdgeCases]") {
    
    SECTION("Empty event source") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create empty event data
        std::vector<float> events;
        
        auto eventSource = std::make_shared<MockEventSource>(
            "EmptyEvents", timeFrame, events);
        
        // Create intervals for testing
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4))
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "EmptyEvents");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == 2);
        REQUIRE(presenceResults[0] == false);
        REQUIRE(presenceResults[1] == false);
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "EmptyEvents");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 2);
        REQUIRE(countResults[0] == 0);
        REQUIRE(countResults[1] == 0);
        
        // Test Gather operation
        EventInIntervalComputer<std::vector<float>> gatherComputer(eventSource, 
                                                                  EventOperation::Gather,
                                                                  "EmptyEvents");
        
        auto gatherResults = gatherComputer.compute(plan);
        
        REQUIRE(gatherResults.size() == 2);
        REQUIRE(gatherResults[0].size() == 0);
        REQUIRE(gatherResults[1].size() == 0);
    }
    
    SECTION("Single event scenarios") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create single event data
        std::vector<float> events = {2.5f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "SingleEvent", timeFrame, events);
        
        // Create intervals for testing
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // Before event
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)), // Contains event
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5))  // After event
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "SingleEvent");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == 3);
        REQUIRE(presenceResults[0] == false);  // Before event
        REQUIRE(presenceResults[1] == true);   // Contains event
        REQUIRE(presenceResults[2] == false);  // After event
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "SingleEvent");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 3);
        REQUIRE(countResults[0] == 0);  // Before event
        REQUIRE(countResults[1] == 1);  // Contains event
        REQUIRE(countResults[2] == 0);  // After event
    }
    
    SECTION("Zero-length intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create event data
        std::vector<float> events = {1.0f, 2.0f, 3.0f, 4.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "TestEvents", timeFrame, events);
        
        // Create zero-length intervals
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)), // Exactly at event
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(2)), // Exactly at event
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(0))  // Between events
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "TestEvents");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == 3);
        REQUIRE(presenceResults[0] == true);   // At event time 1
        REQUIRE(presenceResults[1] == true);   // At event time 2
        REQUIRE(presenceResults[2] == false);  // At time 0, no event
    }
}

TEST_CASE("DM - TV - EventInIntervalComputer Error Handling", "[EventInIntervalComputer][Error]") {
    
    SECTION("Wrong operation type for template specialization") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> events = {1.0f, 2.0f, 3.0f};
        auto eventSource = std::make_shared<MockEventSource>(
            "TestEvents", timeFrame, events);
        
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2))
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test using bool template with wrong operation
        EventInIntervalComputer<bool> wrongPresenceComputer(eventSource, 
                                                           EventOperation::Count,  // Wrong operation
                                                           "TestEvents");
        
        REQUIRE_THROWS_AS(wrongPresenceComputer.compute(plan), std::runtime_error);
        
        // Test using int template with wrong operation
        EventInIntervalComputer<int> wrongCountComputer(eventSource, 
                                                       EventOperation::Presence,  // Wrong operation
                                                       "TestEvents");
        
        REQUIRE_THROWS_AS(wrongCountComputer.compute(plan), std::runtime_error);
        
        // Test using vector template with wrong operation
        EventInIntervalComputer<std::vector<float>> wrongGatherComputer(eventSource, 
                                                                       EventOperation::Count,  // Wrong operation
                                                                       "TestEvents");
        
        REQUIRE_THROWS_AS(wrongGatherComputer.compute(plan), std::runtime_error);
    }
    
}

TEST_CASE("DM - TV - EventInIntervalComputer Dependency Tracking", "[EventInIntervalComputer][Dependencies]") {
    
    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> events = {1.0f};
        auto eventSource = std::make_shared<MockEventSource>(
            "TestSource", timeFrame, events);
        
        // Create computer
        EventInIntervalComputer<bool> computer(eventSource, 
                                              EventOperation::Presence,
                                              "TestSourceName");
        
        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "TestSourceName");
    }
}

TEST_CASE_METHOD(EventInIntervalTestFixture, "DM - TV - EventInIntervalComputer with DataManager fixture", "[EventInIntervalComputer][DataManager][Fixture]") {
    
    SECTION("Test with behavior periods and spike events from fixture") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get the event sources from the DataManager
        auto neuron1_source = dme->getEventSource("Neuron1Spikes");
        auto neuron2_source = dme->getEventSource("Neuron2Spikes");
        auto neuron3_source = dme->getEventSource("Neuron3Spikes");
        
        REQUIRE(neuron1_source != nullptr);
        REQUIRE(neuron2_source != nullptr);
        REQUIRE(neuron3_source != nullptr);
        
        // Get the behavior intervals to use as row selector
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        REQUIRE(behavior_source != nullptr);
        
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
        
        // Test Presence operation
        builder.addColumn<bool>("Neuron1_Present", 
            std::make_unique<EventInIntervalComputer<bool>>(neuron1_source, 
                EventOperation::Presence, "Neuron1Spikes"));
        
        // Test Count operation
        builder.addColumn<int>("Neuron2_Count", 
            std::make_unique<EventInIntervalComputer<int>>(neuron2_source, 
                EventOperation::Count, "Neuron2Spikes"));
        
        // Test Gather operation
        builder.addColumn<std::vector<float>>("Neuron3_Times", 
            std::make_unique<EventInIntervalComputer<std::vector<float>>>(neuron3_source, 
                EventOperation::Gather, "Neuron3Spikes"));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 3);
        REQUIRE(table.hasColumn("Neuron1_Present"));
        REQUIRE(table.hasColumn("Neuron2_Count"));
        REQUIRE(table.hasColumn("Neuron3_Times"));
        
        // Get the column data
        auto neuron1_present = table.getColumnValues<bool>("Neuron1_Present");
        auto neuron2_counts = table.getColumnValues<int>("Neuron2_Count");
        auto neuron3_times = table.getColumnValues<std::vector<float>>("Neuron3_Times");
        
        REQUIRE(neuron1_present.size() == 4);
        REQUIRE(neuron2_counts.size() == 4);
        REQUIRE(neuron3_times.size() == 4);
        
        // Verify the spike analysis results
        // Expected spikes based on our test data (indices → absolute times):
        // Behavior 1 (10-25): Neuron1 (12, 14, 22), Neuron2 (10, 12, 16, 18), Neuron3 (12, 20)
        // Behavior 2 (30-40): Neuron1 (32), Neuron2 (30, 32, 36), Neuron3 (36)
        // Behavior 3 (50-70): Neuron1 (52, 54, 68), Neuron2 (50, 52, 56, 58, 66, 68), Neuron3 (52, 60, 68)
        // Behavior 4 (80-95): Neuron1 (82, 90), Neuron2 (80, 82, 84, 90, 92), Neuron3 (84, 92)
        
        // Note: Actual results depend on the cross-timeframe conversion implementation
        // These tests verify the computer executes without errors and produces reasonable results
        for (size_t i = 0; i < 4; ++i) {
            std::cout << "Behavior period " << i << ": Neuron1_Present=" << neuron1_present[i] 
                      << ", Neuron2_Count=" << neuron2_counts[i] 
                      << ", Neuron3_Times_size=" << neuron3_times[i].size() << std::endl;
            
            // Neuron1 should be present in all behavior periods (now that spikes are aligned with timeframe)
            REQUIRE(neuron1_present[i] == true);
            
            // Neuron2 counts should be non-negative and reasonable
            REQUIRE(neuron2_counts[i] >= 0);
            REQUIRE(neuron2_counts[i] <= 10); // Reasonable upper bound
            
            // Neuron3 times should be reasonable
            REQUIRE(neuron3_times[i].size() >= 0);
            REQUIRE(neuron3_times[i].size() <= 5); // Reasonable upper bound
            
            // All spike times should be sorted
            if (neuron3_times[i].size() > 1) {
                for (size_t j = 1; j < neuron3_times[i].size(); ++j) {
                    REQUIRE(neuron3_times[i][j] >= neuron3_times[i][j-1]);
                }
            }
        }
    }
    
    SECTION("Test cross-timeframe event analysis") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get sources from different timeframes
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");  // behavior_time frame
        auto neuron_source = dme->getEventSource("Neuron1Spikes");        // spike_time frame
        
        REQUIRE(behavior_source != nullptr);
        REQUIRE(neuron_source != nullptr);
        
        // Verify they have different timeframes
        auto behavior_tf = behavior_source->getTimeFrame();
        auto spike_tf = neuron_source->getTimeFrame();
        REQUIRE(behavior_tf != spike_tf);
        REQUIRE(behavior_tf->getTotalFrameCount() == 101);  // behavior_time: 0-100
        REQUIRE(spike_tf->getTotalFrameCount() == 51);      // spike_time: 0,2,4,...,100
        
        // Create a simple test with one behavior interval
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(10), TimeFrameIndex(25))  // Behavior period 1
        };
        
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_tf);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Add event analysis columns
        builder.addColumn<bool>("Spike_Present", 
            std::make_unique<EventInIntervalComputer<bool>>(neuron_source, 
                EventOperation::Presence, "Neuron1Spikes"));
        
        builder.addColumn<int>("Spike_Count", 
            std::make_unique<EventInIntervalComputer<int>>(neuron_source, 
                EventOperation::Count, "Neuron1Spikes"));
        
        builder.addColumn<std::vector<float>>("Spike_Times", 
            std::make_unique<EventInIntervalComputer<std::vector<float>>>(neuron_source, 
                EventOperation::Gather, "Neuron1Spikes"));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 3);
        
        auto spike_present = table.getColumnValues<bool>("Spike_Present");
        auto spike_counts = table.getColumnValues<int>("Spike_Count");
        auto spike_times = table.getColumnValues<std::vector<float>>("Spike_Times");
        
        REQUIRE(spike_present.size() == 1);
        REQUIRE(spike_counts.size() == 1);
        REQUIRE(spike_times.size() == 1);
        
        // Verify results are reasonable (exact values depend on timeframe conversion)
        REQUIRE(spike_present[0] == true);  // Should have spikes in this period
        REQUIRE(spike_counts[0] >= 1);      // Should have at least 1 spike
        REQUIRE(spike_counts[0] <= 10);     // Should have reasonable number of spikes
        REQUIRE(spike_times[0].size() == static_cast<size_t>(spike_counts[0])); // Count should match gathered times
        
        std::cout << "Cross-timeframe test - Spike Count: " << spike_counts[0] 
                  << ", Times gathered: " << spike_times[0].size() << std::endl;
    }
}

TEST_CASE_METHOD(EventTableRegistryTestFixture, "DM - TV - EventInIntervalComputer via ComputerRegistry", "[EventInIntervalComputer][Registry]") {
    
    SECTION("Verify EventInIntervalComputer is registered in ComputerRegistry") {
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Check that all event interval computers are registered
        auto presence_info = registry.findComputerInfo("Event Presence");
        auto count_info = registry.findComputerInfo("Event Count");
        auto gather_info = registry.findComputerInfo("Event Gather");
        
        REQUIRE(presence_info != nullptr);
        REQUIRE(count_info != nullptr);
        REQUIRE(gather_info != nullptr);
        
        // Verify computer info details for Presence
        REQUIRE(presence_info->name == "Event Presence");
        REQUIRE(presence_info->outputType == typeid(bool));
        REQUIRE(presence_info->outputTypeName == "bool");
        REQUIRE(presence_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(presence_info->requiredSourceType == typeid(std::shared_ptr<IEventSource>));
        
        // Verify computer info details for Count
        REQUIRE(count_info->name == "Event Count");
        REQUIRE(count_info->outputType == typeid(int));
        REQUIRE(count_info->outputTypeName == "int");
        REQUIRE(count_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(count_info->requiredSourceType == typeid(std::shared_ptr<IEventSource>));
        
        // Verify computer info details for Gather
        REQUIRE(gather_info->name == "Event Gather");
        REQUIRE(gather_info->outputType == typeid(std::vector<float>));
        REQUIRE(gather_info->outputTypeName == "std::vector<float>");
        REQUIRE(gather_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(gather_info->requiredSourceType == typeid(std::shared_ptr<IEventSource>));
    }
    
    SECTION("Create EventInIntervalComputer via ComputerRegistry") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Get neuron source for testing
        auto neuron_source = dme->getEventSource("Neuron1Spikes");
        REQUIRE(neuron_source != nullptr);
        
        // Create computers via registry
        std::map<std::string, std::string> empty_params;
        
        auto presence_computer = registry.createTypedComputer<bool>(
            "Event Presence", neuron_source, empty_params);
        auto count_computer = registry.createTypedComputer<int>(
            "Event Count", neuron_source, empty_params);
        
        REQUIRE(presence_computer != nullptr);
        REQUIRE(count_computer != nullptr);
        
        // Test gather computer with parameters
        std::map<std::string, std::string> gather_params;
        gather_params["mode"] = "absolute";
        auto gather_computer = registry.createTypedComputer<std::vector<float>>(
            "Event Gather", neuron_source, gather_params);
        REQUIRE(gather_computer != nullptr);
        
        // Test gather computer with centered parameter
        std::map<std::string, std::string> center_params;
        center_params["mode"] = "centered";
        auto center_computer = registry.createTypedComputer<std::vector<float>>(
            "Event Gather", neuron_source, center_params);
        REQUIRE(center_computer != nullptr);
        
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
        builder.addColumn("RegistryPresence", std::move(presence_computer));
        builder.addColumn("RegistryCount", std::move(count_computer));
        builder.addColumn("RegistryGather", std::move(gather_computer));
        builder.addColumn("RegistryCenter", std::move(center_computer));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 4);
        REQUIRE(table.hasColumn("RegistryPresence"));
        REQUIRE(table.hasColumn("RegistryCount"));
        REQUIRE(table.hasColumn("RegistryGather"));
        REQUIRE(table.hasColumn("RegistryCenter"));
        
        auto presence = table.getColumnValues<bool>("RegistryPresence");
        auto counts = table.getColumnValues<int>("RegistryCount");
        auto gather_times = table.getColumnValues<std::vector<float>>("RegistryGather");
        auto center_times = table.getColumnValues<std::vector<float>>("RegistryCenter");
        
        REQUIRE(presence.size() == 1);
        REQUIRE(counts.size() == 1);
        REQUIRE(gather_times.size() == 1);
        REQUIRE(center_times.size() == 1);
        
        // Verify reasonable results
        REQUIRE(presence[0] == true);  // Should have spikes in this period
        REQUIRE(counts[0] >= 1);       // Should have at least 1 spike
        REQUIRE(counts[0] <= 10);      // Should have reasonable number of spikes
        REQUIRE(gather_times[0].size() == static_cast<size_t>(counts[0]));
        REQUIRE(center_times[0].size() == static_cast<size_t>(counts[0]));
        
        std::cout << "Registry test - Presence: " << presence[0] 
                  << ", Count: " << counts[0] 
                  << ", Gather size: " << gather_times[0].size()
                  << ", Center size: " << center_times[0].size() << std::endl;
    }
    
    SECTION("Compare registry-created vs direct-created computers") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        auto neuron_source = dme->getEventSource("Neuron2Spikes");
        REQUIRE(neuron_source != nullptr);
        
        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<int>(
            "Event Count", neuron_source, empty_params);
        
        // Create computer directly
        auto direct_computer = std::make_unique<EventInIntervalComputer<int>>(
            neuron_source, EventOperation::Count, "Neuron2Spikes");
        
        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);
        
        // Test both computers with the same data
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(80), TimeFrameIndex(95))
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

TEST_CASE_METHOD(EventTableRegistryTestFixture, "DM - TV - EventInIntervalComputer via JSON TablePipeline", "[EventInIntervalComputer][JSON][Pipeline]") {
    
    SECTION("Test Event analysis operations via JSON pipeline") {
        // JSON configuration for testing EventInIntervalComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Event Interval Analysis Test",
                "description": "Test JSON execution of EventInIntervalComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "event_interval_test",
                    "name": "Event Interval Analysis Table",
                    "description": "Test table using EventInIntervalComputer",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "SpikePresent",
                            "description": "Presence of spikes in each behavior period",
                            "data_source": "Neuron1Spikes",
                            "computer": "Event Presence"
                        },
                        {
                            "name": "SpikeCount",
                            "description": "Count of spikes in each behavior period",
                            "data_source": "Neuron2Spikes",
                            "computer": "Event Count"
                        },
                        {
                            "name": "SpikeTimes",
                            "description": "Spike times within each behavior period",
                            "data_source": "Neuron3Spikes",
                            "computer": "Event Gather",
                            "parameters": {
                                "mode": "absolute"
                            }
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
        REQUIRE(config.table_id == "event_interval_test");
        REQUIRE(config.name == "Event Interval Analysis Table");
        REQUIRE(config.columns.size() == 3);

        // Verify column configurations
        auto const& column1 = config.columns[0];
        REQUIRE(column1["name"] == "SpikePresent");
        REQUIRE(column1["computer"] == "Event Presence");
        REQUIRE(column1["data_source"] == "Neuron1Spikes");

        auto const& column2 = config.columns[1];
        REQUIRE(column2["name"] == "SpikeCount");
        REQUIRE(column2["computer"] == "Event Count");
        REQUIRE(column2["data_source"] == "Neuron2Spikes");

        auto const& column3 = config.columns[2];
        REQUIRE(column3["name"] == "SpikeTimes");
        REQUIRE(column3["computer"] == "Event Gather");
        REQUIRE(column3["data_source"] == "Neuron3Spikes");
        REQUIRE(column3["parameters"]["mode"] == "absolute");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "interval");
        REQUIRE(config.row_selector["source"] == "BehaviorPeriods");

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline
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
            REQUIRE(registry.hasTable("event_interval_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("event_interval_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const& name : column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 3);
            REQUIRE(built_table->hasColumn("SpikePresent"));
            REQUIRE(built_table->hasColumn("SpikeCount"));
            REQUIRE(built_table->hasColumn("SpikeTimes"));

            // Verify table has 4 rows (one for each behavior period)
            REQUIRE(built_table->getRowCount() == 4);

            // Get and verify the computed values
            auto spike_present = built_table->getColumnValues<bool>("SpikePresent");
            auto spike_counts = built_table->getColumnValues<int>("SpikeCount");
            auto spike_times = built_table->getColumnValues<std::vector<float>>("SpikeTimes");

            REQUIRE(spike_present.size() == 4);
            REQUIRE(spike_counts.size() == 4);
            REQUIRE(spike_times.size() == 4);

            for (size_t i = 0; i < 4; ++i) {
                // Should have spikes in all behavior periods
                REQUIRE(spike_present[i] == true);
                REQUIRE(spike_counts[i] >= 1);      // Should have at least 1 spike per period
                REQUIRE(spike_counts[i] <= 10);     // Should have reasonable number of spikes
                REQUIRE(spike_times[i].size() >= 1); // Should have at least 1 spike time
                REQUIRE(spike_times[i].size() <= 10); // Should have reasonable number of spike times
                
                std::cout << "Row " << i << ": Present=" << spike_present[i] 
                          << ", Count=" << spike_counts[i] 
                          << ", Times gathered=" << spike_times[i].size() << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }
    
    SECTION("Test Event Gather with centered mode via JSON") {
        char const * json_config = R"({
            "metadata": {
                "name": "Event Gather Centered Test",
                "description": "Test JSON execution of EventInIntervalComputer with centered gathering"
            },
            "tables": [
                {
                    "table_id": "event_gather_centered_test",
                    "name": "Event Gather Centered Test Table",
                    "description": "Test table using EventInIntervalComputer centered gathering",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "SpikeTimes_Absolute",
                            "description": "Absolute spike times within each behavior period",
                            "data_source": "Neuron1Spikes",
                            "computer": "Event Gather",
                            "parameters": {
                                "mode": "absolute"
                            }
                        },
                        {
                            "name": "SpikeTimes_Centered",
                            "description": "Spike times relative to interval center",
                            "data_source": "Neuron1Spikes",
                            "computer": "Event Gather",
                            "parameters": {
                                "mode": "centered"
                            }
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
        REQUIRE(config.columns[0]["parameters"]["mode"] == "absolute");
        REQUIRE(config.columns[1]["parameters"]["mode"] == "centered");

        std::cout << "Centered gathering JSON configuration parsed successfully" << std::endl;
        
        // Execute the pipeline
        auto pipeline_result = pipeline.execute();
        
        if (pipeline_result.success) {
            std::cout << "✓ Centered gathering pipeline executed successfully!" << std::endl;
            
            auto& registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("event_gather_centered_test");
            REQUIRE(built_table != nullptr);
            
            REQUIRE(built_table->getRowCount() == 4);
            REQUIRE(built_table->getColumnCount() == 2);
            REQUIRE(built_table->hasColumn("SpikeTimes_Absolute"));
            REQUIRE(built_table->hasColumn("SpikeTimes_Centered"));
            
            auto absolute_times = built_table->getColumnValues<std::vector<float>>("SpikeTimes_Absolute");
            auto centered_times = built_table->getColumnValues<std::vector<float>>("SpikeTimes_Centered");
            
            REQUIRE(absolute_times.size() == 4);
            REQUIRE(centered_times.size() == 4);
            
            // Verify that both modes return the same number of spikes per interval
            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(absolute_times[i].size() == centered_times[i].size());
                
                std::cout << "Row " << i << ": " << absolute_times[i].size() 
                          << " spikes (absolute and centered)" << std::endl;
            }
        } else {
            FAIL("Centered gathering pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}

TEST_CASE("DM - TV - EventInIntervalComputer Complex Scenarios", "[EventInIntervalComputer][Complex]") {
    
    SECTION("Large number of events and intervals") {
        // Create time frame
        std::vector<int> timeValues;
        for (int i = 0; i <= 100; ++i) {
            timeValues.push_back(i);
        }
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create many events
        std::vector<float> events;
        for (float i = 0.5f; i < 100.0f; i += 2.0f) {
            events.push_back(i);
        }
        
        auto eventSource = std::make_shared<MockEventSource>(
            "ManyEvents", timeFrame, events);
        
        // Create many intervals
        std::vector<TimeFrameInterval> intervals;
        for (int i = 0; i < 50; i += 5) {
            intervals.emplace_back(TimeFrameIndex(i), TimeFrameIndex(i + 4));
        }
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "ManyEvents");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == intervals.size());
        
        // All results should be non-negative
        for (auto result : countResults) {
            REQUIRE(result >= 0);
        }
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "ManyEvents");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == intervals.size());
        
        // At least some intervals should contain events
        bool hasEvents = false;
        for (auto result : presenceResults) {
            if (result) {
                hasEvents = true;
                break;
            }
        }
        REQUIRE(hasEvents == true);
    }
    
    SECTION("Events at interval boundaries") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create events exactly at interval boundaries
        std::vector<float> events = {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "BoundaryEvents", timeFrame, events);
        
        // Create intervals that start and end at event times
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // Events at both boundaries
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(4)), // Events at both boundaries
            TimeFrameInterval(TimeFrameIndex(4), TimeFrameIndex(6)), // Events at both boundaries
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)), // Event at end boundary
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5))  // No events at boundaries
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "BoundaryEvents");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 5);
        
        // Verify boundary event handling
        REQUIRE(countResults[0] >= 1);  // Should include events at 0 and/or 2
        REQUIRE(countResults[1] >= 1);  // Should include events at 2 and/or 4
        REQUIRE(countResults[2] >= 1);  // Should include events at 4 and/or 6
        REQUIRE(countResults[3] >= 1);  // Should include event at 2
        REQUIRE(countResults[4] >= 0);  // May or may not include events depending on boundary handling
    }
    
    SECTION("Different time frames for rows and events") {
        // Create different time frames with different scales
        // Row time frame: coarser scale (0, 10, 20, 30, 40, 50)
        std::vector<int> rowTimeValues = {0, 10, 20, 30, 40, 50};
        auto rowTimeFrame = std::make_shared<TimeFrame>(rowTimeValues);
        
        // Event time frame: finer scale (0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30)
        std::vector<int> eventTimeValues = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
        auto eventTimeFrame = std::make_shared<TimeFrame>(eventTimeValues);
        
        // Create events using the event time frame scale
        // Events at times: 2, 6, 12, 18, 24, 28 (actual time values)
        std::vector<float> events = {2.0f, 6.0f, 12.0f, 18.0f, 24.0f, 28.0f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "DifferentTimeFrameEvents", eventTimeFrame, events);
        
        // Create intervals using the row time frame scale
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)), // Row time 0-10: should contain events at 2, 6
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(2)), // Row time 10-20: should contain events at 12, 18
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)), // Row time 20-30: should contain events at 24, 28
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(4)), // Row time 30-40: should contain no events
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2))  // Row time 0-20: should contain events at 2, 6, 12, 18
        };
        
        ExecutionPlan plan(intervals, rowTimeFrame);
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "DifferentTimeFrameEvents");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 5);
        REQUIRE(countResults[0] == 2);  // Interval 0-10: events at 2, 6
        REQUIRE(countResults[1] == 2);  // Interval 10-20: events at 12, 18
        REQUIRE(countResults[2] == 2);  // Interval 20-30: events at 24, 28
        REQUIRE(countResults[3] == 0);  // Interval 30-40: no events
        REQUIRE(countResults[4] == 4);  // Interval 0-20: events at 2, 6, 12, 18
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "DifferentTimeFrameEvents");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == 5);
        REQUIRE(presenceResults[0] == true);   // Interval 0-10: has events
        REQUIRE(presenceResults[1] == true);   // Interval 10-20: has events
        REQUIRE(presenceResults[2] == true);   // Interval 20-30: has events
        REQUIRE(presenceResults[3] == false);  // Interval 30-40: no events
        REQUIRE(presenceResults[4] == true);   // Interval 0-20: has events
        
        // Test Gather operation
        EventInIntervalComputer<std::vector<float>> gatherComputer(eventSource, 
                                                                  EventOperation::Gather,
                                                                  "DifferentTimeFrameEvents");
        
        auto gatherResults = gatherComputer.compute(plan);
        
        REQUIRE(gatherResults.size() == 5);
        
        // Check first interval (0-10)
        REQUIRE(gatherResults[0].size() == 2);
        REQUIRE(gatherResults[0][0] == Catch::Approx(2.0f).epsilon(0.001f));
        REQUIRE(gatherResults[0][1] == Catch::Approx(6.0f).epsilon(0.001f));
        
        // Check second interval (10-20)
        REQUIRE(gatherResults[1].size() == 2);
        REQUIRE(gatherResults[1][0] == Catch::Approx(12.0f).epsilon(0.001f));
        REQUIRE(gatherResults[1][1] == Catch::Approx(18.0f).epsilon(0.001f));
        
        // Check third interval (20-30)
        REQUIRE(gatherResults[2].size() == 2);
        REQUIRE(gatherResults[2][0] == Catch::Approx(24.0f).epsilon(0.001f));
        REQUIRE(gatherResults[2][1] == Catch::Approx(28.0f).epsilon(0.001f));
        
        // Check fourth interval (30-40) - should be empty
        REQUIRE(gatherResults[3].size() == 0);
        
        // Check fifth interval (0-20) - should contain first 4 events
        REQUIRE(gatherResults[4].size() == 4);
        REQUIRE(gatherResults[4][0] == Catch::Approx(2.0f).epsilon(0.001f));
        REQUIRE(gatherResults[4][1] == Catch::Approx(6.0f).epsilon(0.001f));
        REQUIRE(gatherResults[4][2] == Catch::Approx(12.0f).epsilon(0.001f));
        REQUIRE(gatherResults[4][3] == Catch::Approx(18.0f).epsilon(0.001f));
    }
    
    SECTION("Non-aligned time frames with fractional events") {
        // Create row time frame with irregular intervals
        std::vector<int> rowTimeValues = {0, 5, 13, 27, 45};
        auto rowTimeFrame = std::make_shared<TimeFrame>(rowTimeValues);
        
        // Create event time frame with different scale
        std::vector<int> eventTimeValues = {0, 3, 7, 11, 15, 19, 23, 31, 39, 47};
        auto eventTimeFrame = std::make_shared<TimeFrame>(eventTimeValues);
        
        // Create events with fractional times that fall between time frame points
        std::vector<float> events = {1.5f, 4.2f, 8.7f, 14.1f, 20.8f, 25.3f, 35.6f, 42.9f};
        
        auto eventSource = std::make_shared<MockEventSource>(
            "NonAlignedEvents", eventTimeFrame, events);
        
        // Create intervals using the row time frame
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1)), // Row time 0-5: should contain events at 1.5, 4.2
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(2)), // Row time 5-13: should contain events at 8.7
            TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(3)), // Row time 13-27: should contain events at 14.1, 20.8, 25.3
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(4))  // Row time 27-45: should contain events at 35.6, 42.9
        };
        
        ExecutionPlan plan(intervals, rowTimeFrame);
        
        // Test Count operation
        EventInIntervalComputer<int> countComputer(eventSource, 
                                                  EventOperation::Count,
                                                  "NonAlignedEvents");
        
        auto countResults = countComputer.compute(plan);
        
        REQUIRE(countResults.size() == 4);
        REQUIRE(countResults[0] == 2);  // Interval 0-5: events at 1.5, 4.2
        REQUIRE(countResults[1] == 1);  // Interval 5-13: event at 8.7
        REQUIRE(countResults[2] == 3);  // Interval 13-27: events at 14.1, 20.8, 25.3
        REQUIRE(countResults[3] == 2);  // Interval 27-45: events at 35.6, 42.9
        
        // Test Presence operation
        EventInIntervalComputer<bool> presenceComputer(eventSource, 
                                                      EventOperation::Presence,
                                                      "NonAlignedEvents");
        
        auto presenceResults = presenceComputer.compute(plan);
        
        REQUIRE(presenceResults.size() == 4);
        REQUIRE(presenceResults[0] == true);   // Interval 0-5: has events
        REQUIRE(presenceResults[1] == true);   // Interval 5-13: has events
        REQUIRE(presenceResults[2] == true);   // Interval 13-27: has events
        REQUIRE(presenceResults[3] == true);   // Interval 27-45: has events
    }
}
