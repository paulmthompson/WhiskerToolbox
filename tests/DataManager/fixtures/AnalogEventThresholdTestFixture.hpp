#ifndef ANALOG_EVENT_THRESHOLD_TEST_FIXTURE_HPP
#define ANALOG_EVENT_THRESHOLD_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>

class AnalogEventThresholdTestFixture {
protected:
    AnalogEventThresholdTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogEventThresholdTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_signals;

private:
    void populateTestData() {
        // Data from "Positive threshold, no lockout"
        createSignal("positive_no_lockout", {0.5f, 1.5f, 0.8f, 2.5f, 1.2f}, {100, 200, 300, 400, 500});

        // Data from "Positive threshold, with lockout"
        createSignal("positive_with_lockout", {0.5f, 1.5f, 1.8f, 0.5f, 2.5f, 2.2f}, {100, 200, 300, 400, 500, 600});

        // Data from "Negative threshold, no lockout"
        createSignal("negative_no_lockout", {0.5f, -1.5f, -0.8f, -2.5f, -1.2f}, {100, 200, 300, 400, 500});

        // Data from "Negative threshold, with lockout"
        createSignal("negative_with_lockout", {0.0f, -1.5f, -1.2f, 0.0f, -2.0f, -0.5f}, {100, 200, 300, 400, 500, 600});

        // Data from "Absolute threshold, no lockout"
        createSignal("absolute_no_lockout", {0.5f, -1.5f, 0.8f, 2.5f, -1.2f, 0.9f}, {100, 200, 300, 400, 500, 600});

        // Data from "Absolute threshold, with lockout"
        createSignal("absolute_with_lockout", {0.5f, 1.5f, -1.2f, 0.5f, -2.0f, 0.8f}, {100, 200, 300, 400, 500, 600});

        // Data from "No events expected (threshold too high)"
        createSignal("no_events_high_threshold", {0.5f, 1.5f, 0.8f, 2.5f, 1.2f}, {100, 200, 300, 400, 500});

        // Data from "All events expected (threshold very low, no lockout)"
        createSignal("all_events_low_threshold", {0.5f, 1.5f, 0.8f, 2.5f, 1.2f}, {100, 200, 300, 400, 500});

        // Data from "Progress callback detailed check"
        createSignal("progress_callback_check", {0.5f, 1.5f, 0.8f, 2.5f, 1.2f}, {100, 200, 300, 400, 500});

        // Data from "Empty AnalogTimeSeries"
        createSignal("empty_signal", {}, {});

        // Data from "Lockout time larger than series duration"
        createSignal("lockout_larger_than_duration", {1.5f, 2.5f, 3.5f}, {100, 200, 300});

        // Data from "Events exactly at threshold value"
        createSignal("events_at_threshold", {0.5f, 1.0f, 1.5f}, {100, 200, 300});

        // Data from "Timestamps are zero or start from zero"
        createSignal("zero_based_timestamps", {1.5f, 0.5f, 2.5f}, {0, 10, 20});
    }

    void createSignal(const std::string& key, const std::vector<float>& values, const std::vector<int>& times_int) {
        std::vector<TimeFrameIndex> times;
        for (int t : times_int) {
            times.push_back(TimeFrameIndex(t));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        auto time_frame = std::make_shared<TimeFrame>(times_int);
        ats->setTimeFrame(time_frame);
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_signals[key] = ats;
    }

    std::unique_ptr<DataManager> m_data_manager;
};

#endif // ANALOG_EVENT_THRESHOLD_TEST_FIXTURE_HPP
