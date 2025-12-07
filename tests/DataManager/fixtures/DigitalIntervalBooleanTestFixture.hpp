#ifndef DIGITAL_INTERVAL_BOOLEAN_TEST_FIXTURE_HPP
#define DIGITAL_INTERVAL_BOOLEAN_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <string>
#include <map>
#include <vector>

/**
 * @brief Test fixture for DigitalIntervalBoolean transform tests
 * 
 * This fixture provides reusable test data for both V1 and V2 tests.
 * Each test scenario is stored with a descriptive key that describes the
 * data pattern, not the expected result.
 * 
 * The fixture creates pairs of interval series for testing boolean operations.
 */
class DigitalIntervalBooleanTestFixture {
protected:
    DigitalIntervalBooleanTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~DigitalIntervalBooleanTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    // Map of named test interval series - primary inputs
    std::map<std::string, std::shared_ptr<DigitalIntervalSeries>> m_input_series;
    
    // Map of named test interval series - secondary inputs (for binary operations)
    std::map<std::string, std::shared_ptr<DigitalIntervalSeries>> m_other_series;

private:
    void populateTestData() {
        // ========================================================================
        // AND Operation Test Data
        // ========================================================================

        // Scenario: Basic overlapping intervals
        // Input: (1,5), (10,15)
        // Other: (3,7), (12,20)
        // AND Expected: (3,5), (12,15)
        createIntervalPair("and_overlapping", 
            {{1, 5}, {10, 15}},
            {{3, 7}, {12, 20}});

        // Scenario: No overlap between intervals
        // Input: (1,5)
        // Other: (10,15)
        // AND Expected: empty
        createIntervalPair("and_no_overlap",
            {{1, 5}},
            {{10, 15}});

        // Scenario: Complete overlap (identical intervals)
        // Input: (1,10)
        // Other: (1,10)
        // AND Expected: (1,10)
        createIntervalPair("and_complete_overlap",
            {{1, 10}},
            {{1, 10}});

        // Scenario: One series is subset of other
        // Input: (5,15)
        // Other: (1,20)
        // AND Expected: (5,15)
        createIntervalPair("and_subset",
            {{5, 15}},
            {{1, 20}});

        // ========================================================================
        // OR Operation Test Data
        // ========================================================================

        // Scenario: Separate intervals (no overlap)
        // Input: (1,5)
        // Other: (10,15)
        // OR Expected: (1,5), (10,15)
        createIntervalPair("or_separate",
            {{1, 5}},
            {{10, 15}});

        // Scenario: Overlapping intervals that should merge
        // Input: (1,10)
        // Other: (5,15)
        // OR Expected: (1,15)
        createIntervalPair("or_overlapping_merge",
            {{1, 10}},
            {{5, 15}});

        // Scenario: Multiple intervals with gaps
        // Input: (1,5), (15,20)
        // Other: (8,12), (18,25)
        // OR Expected: (1,5), (8,12), (15,25)
        createIntervalPair("or_multiple_with_gaps",
            {{1, 5}, {15, 20}},
            {{8, 12}, {18, 25}});

        // ========================================================================
        // XOR Operation Test Data
        // ========================================================================

        // Scenario: No overlap (same as OR)
        // Input: (1,5)
        // Other: (10,15)
        // XOR Expected: (1,5), (10,15)
        createIntervalPair("xor_no_overlap",
            {{1, 5}},
            {{10, 15}});

        // Scenario: Partial overlap (excludes overlapping part)
        // Input: (1,10)
        // Other: (5,15)
        // XOR Expected: (1,4), (11,15)
        createIntervalPair("xor_partial_overlap",
            {{1, 10}},
            {{5, 15}});

        // Scenario: Complete overlap (results in nothing)
        // Input: (1,10)
        // Other: (1,10)
        // XOR Expected: empty
        createIntervalPair("xor_complete_overlap",
            {{1, 10}},
            {{1, 10}});

        // Scenario: Complex pattern with multiple intervals
        // Input: (1,5), (10,15)
        // Other: (3,12)
        // XOR Expected: (1,2), (6,9), (13,15)
        createIntervalPair("xor_complex",
            {{1, 5}, {10, 15}},
            {{3, 12}});

        // ========================================================================
        // NOT Operation Test Data (only uses input, ignores other)
        // ========================================================================

        // Scenario: Single interval (NOT gives empty within its range)
        // Input: (5,10)
        // NOT Expected: empty (entire range is covered)
        createSingleInterval("not_single_interval", {{5, 10}});

        // Scenario: Intervals with gaps (NOT fills the gaps)
        // Input: (1,5), (10,15)
        // NOT Expected: (6,9) - the gap between intervals
        createSingleInterval("not_with_gaps", {{1, 5}, {10, 15}});

        // Scenario: Multiple gaps
        // Input: (1,3), (5,7), (9,11)
        // NOT Expected: (4,4), (8,8)
        createSingleInterval("not_multiple_gaps", {{1, 3}, {5, 7}, {9, 11}});

        // ========================================================================
        // AND_NOT Operation Test Data
        // ========================================================================

        // Scenario: Subtract overlapping portion
        // Input: (1,10)
        // Other: (5,15)
        // AND_NOT Expected: (1,4)
        createIntervalPair("and_not_subtract_overlap",
            {{1, 10}},
            {{5, 15}});

        // Scenario: No overlap (input unchanged)
        // Input: (1,5)
        // Other: (10,15)
        // AND_NOT Expected: (1,5)
        createIntervalPair("and_not_no_overlap",
            {{1, 5}},
            {{10, 15}});

        // Scenario: Complete overlap removes everything
        // Input: (5,10)
        // Other: (1,15)
        // AND_NOT Expected: empty
        createIntervalPair("and_not_complete_overlap",
            {{5, 10}},
            {{1, 15}});

        // Scenario: Punch holes in input
        // Input: (1,20)
        // Other: (5,8), (12,15)
        // AND_NOT Expected: (1,4), (9,11), (16,20)
        createIntervalPair("and_not_punch_holes",
            {{1, 20}},
            {{5, 8}, {12, 15}});

        // ========================================================================
        // Edge Cases
        // ========================================================================

        // Empty input series
        createIntervalPair("empty_input",
            {},
            {{1, 10}});

        // Both series empty
        createIntervalPair("both_empty",
            {},
            {});

        // Empty for NOT operation
        createSingleInterval("not_empty", {});

        // Large intervals for progress callback testing
        createIntervalPair("large_intervals",
            {{1, 100}},
            {{50, 150}});
    }

    void createIntervalPair(const std::string& key, 
                            const std::vector<Interval>& input_intervals,
                            const std::vector<Interval>& other_intervals) {
        auto input_dis = std::make_shared<DigitalIntervalSeries>(input_intervals);
        auto other_dis = std::make_shared<DigitalIntervalSeries>(other_intervals);
        
        // Store in maps for direct access
        m_input_series[key] = input_dis;
        m_other_series[key] = other_dis;
        
        // Also store in DataManager with distinct keys
        m_data_manager->setData(key + "_input", input_dis, TimeKey("default"));
        m_data_manager->setData(key + "_other", other_dis, TimeKey("default"));
    }

    void createSingleInterval(const std::string& key,
                              const std::vector<Interval>& intervals) {
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        
        // Store in input map (other map gets empty series for NOT operations)
        m_input_series[key] = dis;
        m_other_series[key] = std::make_shared<DigitalIntervalSeries>();
        
        // Store in DataManager
        m_data_manager->setData(key + "_input", dis, TimeKey("default"));
        m_data_manager->setData(key + "_other", m_other_series[key], TimeKey("default"));
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif // DIGITAL_INTERVAL_BOOLEAN_TEST_FIXTURE_HPP
