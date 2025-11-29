#ifndef ANALOG_INTERVAL_THRESHOLD_TEST_FIXTURE_HPP
#define ANALOG_INTERVAL_THRESHOLD_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>

class AnalogIntervalThresholdTestFixture {
protected:
    AnalogIntervalThresholdTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogIntervalThresholdTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_signals;
    std::map<std::string, std::shared_ptr<TimeFrame>> m_test_timeframes;

private:
    void populateTestData() {
        // Data from "Positive threshold - simple case"
        createSignal("positive_simple", 
            {0.5f, 1.5f, 2.0f, 1.8f, 0.8f, 2.5f, 1.2f, 0.3f},
            {100, 200, 300, 400, 500, 600, 700, 800});

        // Data from "Negative threshold"
        createSignal("negative_threshold",
            {0.5f, -1.5f, -2.0f, -1.8f, 0.8f, -2.5f, -1.2f, 0.3f},
            {100, 200, 300, 400, 500, 600, 700, 800});

        // Data from "Absolute threshold"
        createSignal("absolute_threshold",
            {0.5f, 1.5f, -2.0f, 1.8f, 0.8f, -2.5f, 1.2f, 0.3f},
            {100, 200, 300, 400, 500, 600, 700, 800});

        // Data from "With lockout time"
        createSignal("with_lockout",
            {0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f, 0.3f},
            {100, 200, 250, 300, 400, 450, 500});

        // Data from "With minimum duration"
        createSignal("with_min_duration",
            {0.5f, 1.5f, 0.8f, 1.8f, 1.2f, 1.1f, 0.5f},
            {100, 200, 250, 300, 400, 500, 600});

        // Data from "Signal ends while above threshold"
        createSignal("ends_above_threshold",
            {0.5f, 1.5f, 2.0f, 1.8f, 1.2f},
            {100, 200, 300, 400, 500});

        // Data from "No intervals detected"
        createSignal("no_intervals",
            {0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
            {100, 200, 300, 400, 500});

        // Data from "Progress callback detailed check"
        createSignal("progress_callback",
            {0.5f, 1.5f, 0.8f, 2.0f, 0.3f},
            {100, 200, 300, 400, 500});

        // Data from "Complex signal with multiple parameters"
        createSignal("complex_signal",
            {0.0f, 2.0f, 1.8f, 1.5f, 0.5f, 2.5f, 2.2f, 1.9f, 0.8f, 1.1f, 0.3f},
            {0, 100, 150, 200, 300, 400, 450, 500, 600, 700, 800});

        // Data from "Single sample above threshold"
        createSignal("single_above",
            {2.0f},
            {100});

        // Data from "Single sample below threshold"
        createSignal("single_below",
            {0.5f},
            {100});

        // Data from "All values above threshold"
        createSignal("all_above",
            {1.5f, 2.0f, 1.8f, 2.5f, 1.2f},
            {100, 200, 300, 400, 500});

        // Data from "Zero threshold"
        createSignal("zero_threshold",
            {-1.0f, 0.0f, 1.0f, -0.5f, 0.5f},
            {100, 200, 300, 400, 500});

        // Data from "Negative threshold value"
        createSignal("negative_value",
            {-2.0f, -1.0f, -0.5f, -1.5f, -0.8f},
            {100, 200, 300, 400, 500});

        // Data from "Very large lockout time"
        createSignal("large_lockout",
            {0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f},
            {100, 200, 300, 400, 500, 600});

        // Data from "Very large minimum duration"
        createSignal("large_min_duration",
            {0.5f, 1.5f, 1.8f, 1.2f, 0.5f},
            {100, 200, 300, 400, 500});

        // Data from "Irregular timestamp spacing"
        createSignal("irregular_spacing",
            {0.5f, 1.5f, 0.8f, 1.8f, 0.5f},
            {0, 1, 100, 101, 1000});

        // Data from "Single sample above threshold followed by below threshold"
        createSignal("single_sample_lockout",
            {0.5f, 2.0f, 0.8f, 0.3f},
            {100, 200, 300, 400});

        // Data from "Multiple single samples above threshold"
        createSignal("multiple_single_samples",
            {0.5f, 2.0f, 0.8f, 1.5f, 0.3f, 1.8f, 0.6f},
            {100, 200, 300, 400, 500, 600, 700});

        // Data from "Operation Interface tests"
        createSignal("operation_interface",
            {0.5f, 1.5f, 0.8f, 1.8f},
            {100, 200, 300, 400});

        // Data from "Operation Interface - different threshold directions test"
        createSignal("operation_different_directions",
            {0.5f, -1.5f, 0.8f, 1.8f},
            {100, 200, 300, 400});

        // Data from "Missing data treated as zero - positive threshold"
        createSignal("missing_data_positive",
            {0.5f, 1.5f, 1.8f, 0.5f, 1.2f},
            {100, 101, 102, 152, 153});

        // Data from "Missing data treated as zero - negative threshold"
        createSignal("missing_data_negative",
            {0.5f, -1.5f, 0.5f, -1.2f},
            {100, 101, 151, 152});

        // Data from "Missing data ignored mode"
        createSignal("missing_data_ignore",
            {0.5f, 1.5f, 1.8f, 0.5f, 1.2f},
            {100, 101, 102, 152, 153});

        // Data from "No gaps in data"
        createSignal("no_gaps",
            {0.5f, 1.5f, 1.8f, 0.5f, 1.2f},
            {100, 101, 102, 103, 104});

        // Data from "JSON pipeline" and "load_data_from_json_config"
        createSignal("test_signal",
            {0.5f, 1.5f, 2.0f, 1.8f, 0.8f, 2.5f, 1.2f, 0.3f},
            {100, 200, 300, 400, 500, 600, 700, 800});

        // Empty signal for null/edge case tests
        createSignal("empty_signal", {}, {});
    }

    void createSignal(const std::string& key, const std::vector<float>& values, const std::vector<int>& times_int) {
        std::vector<TimeFrameIndex> times;
        for (int t : times_int) {
            times.push_back(TimeFrameIndex(t));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        auto time_frame = std::make_shared<TimeFrame>(times_int);
        ats->setTimeFrame(time_frame);
        
        // Register the TimeFrame with the DataManager
        TimeKey time_key(key + "_time");
        m_data_manager->setTime(time_key, time_frame);
        
        m_data_manager->setData(key, ats, time_key);
        m_test_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    std::unique_ptr<DataManager> m_data_manager;
};

#endif // ANALOG_INTERVAL_THRESHOLD_TEST_FIXTURE_HPP
