#ifndef ANALOG_TEST_FIXTURES_HPP
#define ANALOG_TEST_FIXTURES_HPP

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Test fixture for DataManager with analog signals at different sampling rates
 * 
 * This fixture provides a DataManager populated with:
 * - Two TimeFrame objects:
 *   - "time": values 0 to 1000 (1001 points)
 *   - "time_10": values 0, 10, 20, ..., 1000 (101 points)
 * - Two AnalogTimeSeries:
 *   - Signal A: in "time" timeframe, triangular wave 0->500->0
 *   - Signal B: in "time_10" timeframe, same triangular wave but sampled at 1/10th resolution
 */
class AnalogTestFixture {
protected:
    AnalogTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithAnalogTestData();
    }

    ~AnalogTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     * @return Reference to the DataManager
     */
    DataManager & getDataManager() { return *m_data_manager; }

    /**
     * @brief Get the DataManager instance (const version)
     * @return Const reference to the DataManager
     */
    DataManager const & getDataManager() const { return *m_data_manager; }

    /**
     * @brief Get a pointer to the DataManager
     * @return Raw pointer to the DataManager
     */
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with analog test data
     */
    void populateWithAnalogTestData() {
        createTimeFrames();
        createAnalogSignals();
    }

    /**
     * @brief Create the two TimeFrame objects
     */
    void createTimeFrames() {
        // Create "time" timeframe: 0 to 1000 (1001 points)
        std::vector<int> time_values(1001);
        std::iota(time_values.begin(), time_values.end(), 0);
        auto time_frame = std::make_shared<TimeFrame>(time_values);
        m_data_manager->setTime(TimeKey("time"), time_frame, true);

        // Create "time_10" timeframe: 0, 10, 20, ..., 1000 (101 points)
        std::vector<int> time_10_values;
        time_10_values.reserve(101);
        for (int i = 0; i <= 100; ++i) {
            time_10_values.push_back(i * 10);
        }
        auto time_10_frame = std::make_shared<TimeFrame>(time_10_values);
        m_data_manager->setTime(TimeKey("time_10"), time_10_frame, true);
    }

    /**
     * @brief Create the analog signals
     */
    void createAnalogSignals() {
        // Create signal A: triangular wave 0->500->0 over 1001 points
        std::vector<float> signal_a_values;
        std::vector<TimeFrameIndex> signal_a_times;
        signal_a_values.reserve(1001);
        signal_a_times.reserve(1001);

        for (int i = 0; i <= 1000; ++i) {
            float value;
            if (i <= 500) {
                // Rising edge: 0 to 500
                value = static_cast<float>(i);
            } else {
                // Falling edge: 500 to 0
                value = static_cast<float>(1000 - i);
            }
            signal_a_values.push_back(value);
            signal_a_times.emplace_back(i);
        }

        auto signal_a = std::make_shared<AnalogTimeSeries>(signal_a_values, signal_a_times);
        m_data_manager->setData<AnalogTimeSeries>("A", signal_a, TimeKey("time"));

        // Create signal B: same triangular wave but sampled at 1/10th resolution (101 points)
        std::vector<float> signal_b_values;
        std::vector<TimeFrameIndex> signal_b_times;
        signal_b_values.reserve(101);
        signal_b_times.reserve(101);

        for (int i = 0; i <= 100; ++i) {
            int time_index = i * 10;// 0, 10, 20, ..., 1000
            float value;
            if (time_index <= 500) {
                // Rising edge: 0 to 500
                value = static_cast<float>(time_index);
            } else {
                // Falling edge: 500 to 0
                value = static_cast<float>(1000 - time_index);
            }
            signal_b_values.push_back(value);
            signal_b_times.emplace_back(i);// Note: using i (0-100) as the index in time_10 frame
        }

        auto signal_b = std::make_shared<AnalogTimeSeries>(signal_b_values, signal_b_times);
        m_data_manager->setData<AnalogTimeSeries>("B", signal_b, TimeKey("time_10"));
    }
};

/**
 * @brief Extended analog test fixture with additional signals for comprehensive testing
 * 
 * This fixture extends the basic AnalogTestFixture with additional signals:
 * - Signal C: sine wave in "time" timeframe
 * - Signal D: cosine wave in "time_10" timeframe
 */
class ExtendedAnalogTestFixture : public AnalogTestFixture {
protected:
    ExtendedAnalogTestFixture()
        : AnalogTestFixture() {
        createAdditionalSignals();
    }

private:
    /**
     * @brief Create additional test signals
     */
    void createAdditionalSignals() {
        createSineWave();
        createCosineWave();
    }

    /**
     * @brief Create a sine wave signal in "time" timeframe
     */
    void createSineWave() {
        std::vector<float> sine_values;
        std::vector<TimeFrameIndex> sine_times;
        sine_values.reserve(1001);
        sine_times.reserve(1001);

        float const frequency = 0.01f;// Frequency parameter for sine wave
        float const amplitude = 100.0f;

        for (int i = 0; i <= 1000; ++i) {
            float value = amplitude * std::sin(2.0f * M_PI * frequency * static_cast<float>(i));
            sine_values.push_back(value);
            sine_times.emplace_back(i);
        }

        auto sine_signal = std::make_shared<AnalogTimeSeries>(sine_values, sine_times);
        getDataManagerPtr()->setData<AnalogTimeSeries>("C", sine_signal, TimeKey("time"));
    }

    /**
     * @brief Create a cosine wave signal in "time_10" timeframe
     */
    void createCosineWave() {
        std::vector<float> cosine_values;
        std::vector<TimeFrameIndex> cosine_times;
        cosine_values.reserve(101);
        cosine_times.reserve(101);

        float const frequency = 0.01f;// Same frequency as sine wave
        float const amplitude = 100.0f;

        for (int i = 0; i <= 100; ++i) {
            int time_index = i * 10;// 0, 10, 20, ..., 1000
            float value = amplitude * std::cos(2.0f * M_PI * frequency * static_cast<float>(time_index));
            cosine_values.push_back(value);
            cosine_times.emplace_back(i);// Using i (0-100) as index in time_10 frame
        }

        auto cosine_signal = std::make_shared<AnalogTimeSeries>(cosine_values, cosine_times);
        getDataManagerPtr()->setData<AnalogTimeSeries>("D", cosine_signal, TimeKey("time_10"));
    }
};

#endif// ANALOG_TEST_FIXTURES_HPP
