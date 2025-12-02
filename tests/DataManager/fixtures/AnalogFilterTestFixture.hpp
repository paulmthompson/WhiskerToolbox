#ifndef ANALOG_FILTER_TEST_FIXTURE_HPP
#define ANALOG_FILTER_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cmath>

class AnalogFilterTestFixture {
protected:
    AnalogFilterTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogFilterTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_analog_signals;
    std::map<std::string, std::shared_ptr<TimeFrame>> m_test_timeframes;

private:
    void populateTestData() {
        // Main test signal: sine wave at 10 Hz sampled at 1000 Hz (2000 samples)
        createSineWaveSignal("sine_10hz_2000", 2000, 1000.0, 10.0);

        // For operation tests: constant signal
        createConstantSignal("constant_1000", 1000, 1.0f);

        // For interface tests: simple pattern
        createPatternSignal("pattern_1000", 1000);

        // For filter creation tests: 10 Hz sine wave (1000 samples)
        createSineWaveSignal("sine_10hz_1000", 1000, 1000.0, 10.0);

        // For pipeline tests: multi-frequency signal (5 Hz + 50 Hz)
        createMultiFrequencySignal("multi_freq_5_50", 2000, 1000.0, 
                                  {{5.0, 1.0f}, {50.0, 0.5f}});

        // For multi-filter pipeline: 10 Hz + 60 Hz + 100 Hz
        createMultiFrequencySignal("multi_freq_10_60_100", 2000, 1000.0,
                                  {{10.0, 1.0f}, {60.0, 1.0f}, {100.0, 1.0f}});
    }

    void createSineWaveSignal(const std::string& key, size_t num_samples, 
                             double sampling_rate, double frequency) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;
        
        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sampling_rate;
            data.push_back(static_cast<float>(std::sin(2.0 * M_PI * frequency * t)));
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }
        
        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);
        
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createConstantSignal(const std::string& key, size_t num_samples, float value) {
        std::vector<float> data(num_samples, value);
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;
        
        for (size_t i = 0; i < num_samples; ++i) {
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }
        
        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);
        
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createPatternSignal(const std::string& key, size_t num_samples) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;
        
        for (size_t i = 0; i < num_samples; ++i) {
            data.push_back(static_cast<float>(i % 10)); // Simple repeating pattern
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }
        
        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);
        
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createMultiFrequencySignal(const std::string& key, size_t num_samples,
                                   double sampling_rate,
                                   const std::vector<std::pair<double, float>>& freq_amp_pairs) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;
        
        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sampling_rate;
            float value = 0.0f;
            
            for (const auto& [freq, amp] : freq_amp_pairs) {
                value += amp * std::sin(2.0 * M_PI * freq * t);
            }
            
            data.push_back(value);
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }
        
        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);
        
        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    std::unique_ptr<DataManager> m_data_manager;
};

#endif // ANALOG_FILTER_TEST_FIXTURE_HPP
