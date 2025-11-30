#ifndef ANALOG_SCALING_TEST_FIXTURE_HPP
#define ANALOG_SCALING_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>

#include <string>
#include <map>

class AnalogScalingTestFixture {
protected:
    AnalogScalingTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogScalingTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    // Helper method to create a new DataManager with test data for JSON pipeline tests
    std::unique_ptr<DataManager> createDataManagerWithTestSignal(const std::string& key = "TestSignal.channel1") {
        auto dm = std::make_unique<DataManager>();
        
        std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<int> times_int = {100, 200, 300, 400, 500};
        std::vector<TimeFrameIndex> times;
        for (int t : times_int) {
            times.push_back(TimeFrameIndex(t));
        }
        
        auto time_frame = std::make_shared<TimeFrame>(times_int);
        dm->setTime(TimeKey("default"), time_frame);
        
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        ats->setTimeFrame(time_frame);
        dm->setData(key, ats, TimeKey("default"));
        
        return dm;
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_signals;
    std::map<std::string, std::shared_ptr<TimeFrame>> m_test_timeframes;

private:
    void populateTestData() {
        // Standard test signal used in multiple tests
        createSignal("standard_signal",
            {1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {100, 200, 300, 400, 500});

        // Constant values (zero std dev)
        createSignal("constant_values",
            {3.0f, 3.0f, 3.0f, 3.0f, 3.0f},
            {100, 200, 300, 400, 500});

        // Negative values
        createSignal("negative_values",
            {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f},
            {100, 200, 300, 400, 500});

        // Empty signal
        createSignal("empty_signal", {}, {});

        // Test signal for JSON pipeline and load_data_from_json_config
        createSignal("test_signal",
            {1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            {100, 200, 300, 400, 500});
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

#endif // ANALOG_SCALING_TEST_FIXTURE_HPP
