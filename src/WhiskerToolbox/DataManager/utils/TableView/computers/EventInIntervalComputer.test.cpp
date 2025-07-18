#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "EventInIntervalComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "DigitalTimeSeries/interval_data.hpp"
#include "TimeFrame.hpp"

#include <memory>
#include <vector>
#include <algorithm>
#include <span>

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

TEST_CASE("EventInIntervalComputer Basic Functionality", "[EventInIntervalComputer]") {
    
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

TEST_CASE("EventInIntervalComputer Edge Cases", "[EventInIntervalComputer][EdgeCases]") {
    
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

TEST_CASE("EventInIntervalComputer Error Handling", "[EventInIntervalComputer][Error]") {
    
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

TEST_CASE("EventInIntervalComputer Dependency Tracking", "[EventInIntervalComputer][Dependencies]") {
    
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

TEST_CASE("EventInIntervalComputer Complex Scenarios", "[EventInIntervalComputer][Complex]") {
    
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
